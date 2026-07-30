/*
    Copyright 2019-2026 Hydr8gon

    This file is part of NooDS.

    NooDS is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    NooDS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with NooDS. If not, see <https://www.gnu.org/licenses/>.
*/

#include <cstring>
#include "../core.h"

SdMmc::~SdMmc() {
    // Close the NAND and SD files
    fclose(nand);
    fclose(sd);
}

void SdMmc::saveState(FILE *file) {
    // Write DSi state data to the file
    if (!core->dsiMode) return;
    fread(&cardStatus, sizeof(cardStatus), 1, file);
    fread(&opCond, sizeof(opCond), 1, file);
    fread(&blockLen, sizeof(blockLen), 1, file);
    fread(&curAddress, sizeof(curAddress), 1, file);
    fread(&curBlock, sizeof(curBlock), 1, file);
    fread(&sdCmd, sizeof(sdCmd), 1, file);
    fread(&sdPortSelect, sizeof(sdPortSelect), 1, file);
    fread(&sdCmdParam, sizeof(sdCmdParam), 1, file);
    fread(&sdData16Blkcnt, sizeof(sdData16Blkcnt), 1, file);
    fread(sdResponse, 4, sizeof(sdResponse) / 4, file);
    fread(&sdIrqStatus, sizeof(sdIrqStatus), 1, file);
    fread(&sdIrqMask, sizeof(sdIrqMask), 1, file);
    fread(&sdData16Blklen, sizeof(sdData16Blklen), 1, file);
    fread(&sdErrDetail, sizeof(sdErrDetail), 1, file);
    fread(&sdData16Fifo, sizeof(sdData16Fifo), 1, file);
    fread(&sdDataCtl, sizeof(sdDataCtl), 1, file);
    fread(&sdData32Irq, sizeof(sdData32Irq), 1, file);
    fread(&sdData32Blklen, sizeof(sdData32Blklen), 1, file);
    fread(&sdData32Fifo, sizeof(sdData32Fifo), 1, file);
    SaveStates::writeFifo(dataFifo16, file);
    SaveStates::writeFifo(dataFifo32, file);
}

void SdMmc::loadState(FILE *file) {
    // Read DSi state data from the file
    if (!core->dsiMode) return;
    fread(&cardStatus, sizeof(cardStatus), 1, file);
    fread(&opCond, sizeof(opCond), 1, file);
    fread(&blockLen, sizeof(blockLen), 1, file);
    fread(&curAddress, sizeof(curAddress), 1, file);
    fread(&curBlock, sizeof(curBlock), 1, file);
    fread(&sdCmd, sizeof(sdCmd), 1, file);
    fread(&sdPortSelect, sizeof(sdPortSelect), 1, file);
    fread(&sdCmdParam, sizeof(sdCmdParam), 1, file);
    fread(&sdData16Blkcnt, sizeof(sdData16Blkcnt), 1, file);
    fread(sdResponse, 4, sizeof(sdResponse) / 4, file);
    fread(&sdIrqStatus, sizeof(sdIrqStatus), 1, file);
    fread(&sdIrqMask, sizeof(sdIrqMask), 1, file);
    fread(&sdData16Blklen, sizeof(sdData16Blklen), 1, file);
    fread(&sdErrDetail, sizeof(sdErrDetail), 1, file);
    fread(&sdData16Fifo, sizeof(sdData16Fifo), 1, file);
    fread(&sdDataCtl, sizeof(sdDataCtl), 1, file);
    fread(&sdData32Irq, sizeof(sdData32Irq), 1, file);
    fread(&sdData32Blklen, sizeof(sdData32Blklen), 1, file);
    fread(&sdData32Fifo, sizeof(sdData32Fifo), 1, file);
    SaveStates::readFifo(dataFifo16, file);
    SaveStates::readFifo(dataFifo32, file);
}

uint32_t *SdMmc::init() {
    // Check the SD's capacity so cards over 2GB can be handled differently
    if (sd = fopen(Settings::sdImagePath.c_str(), "rb+")) {
        fseek(sd, 0, SEEK_END);
        sdhc = (ftell(sd) > 0x40000000);
        LOG_INFO("SD card is being treated as %s-capacity\n", sdhc ? "high" : "standard");
    }

    // Try to open a NAND dump and load IDs from its nocash footer
    if (!(nand = fopen(Settings::dsiNandPath.c_str(), "rb+"))) return nullptr;
    fseek(nand, -0x20, SEEK_END);
    fread(consoleId, sizeof(uint32_t), 2, nand);
    fseek(nand, -0x30, SEEK_END);
    fread(mmcCid, sizeof(uint32_t), 4, nand);
    return mmcCid;
}

void SdMmc::sendInterrupt(int bit) {
    // Set the interrupt's request bit
    sdIrqStatus |= BIT(bit);

    // Send an interrupt to the ARM9 if the new bit is unmasked
    if (~sdIrqMask & BIT(bit))
        core->interpreter[1].sendInterrupt(40);
}

void SdMmc::startWrite() {
    // Change to write state and trigger an initial DRQ
    cardStatus = (cardStatus & ~0x1E00) | (0x6 << 9);
    core->ndma[1].setDrq(0x8);

    // Write a block right away if the FIFO is full, or trigger a FIFO empty interrupt
    if (sdDataCtl & BIT(1)) { // 32-bit
        if ((dataFifo32.size() << 2) >= sdData32Blklen)
            core->schedule(SDMMC_WRITE_BLOCK, 1);
        else if (dataFifo32.empty() && (sdData32Irq & BIT(12)))
            core->interpreter[1].sendInterrupt(40);
    }
    else { // 16-bit
        if ((dataFifo16.size() << 1) >= sdData16Blklen)
            core->schedule(SDMMC_WRITE_BLOCK, 1);
        else if (dataFifo16.empty())
            sendInterrupt(25);
    }
}

uint32_t SdMmc::popFifo() {
    // Handle 32-bit FIFO mode
    if (sdDataCtl & BIT(1)) {
        // Pop a value from the 32-bit FIFO and check if empty
        if (dataFifo32.empty()) return 0;
        uint32_t value = dataFifo32.front();
        dataFifo32.pop_front();
        sdData32Irq &= ~BIT(8); // Not full
        if (!dataFifo32.empty()) return value;

        // Trigger a 32-bit FIFO empty interrupt if enabled and finish a block
        sdData32Irq &= ~BIT(9); // Empty
        if (sdData32Irq & BIT(12))
            core->interpreter[1].sendInterrupt(40);
        curBlock--;
        return value;
    }

    // Pop two values from the 16-bit FIFO and check if empty
    if (dataFifo16.empty()) return 0;
    uint32_t value = dataFifo16.front();
    dataFifo16.pop_front();
    if (!dataFifo16.empty()) {
        value |= dataFifo16.front() << 16;
        dataFifo16.pop_front();
    }
    if (!dataFifo16.empty()) return value;

    // Trigger a 16-bit FIFO empty interrupt and finish a block
    sendInterrupt(25);
    curBlock--;
    return value;
}

void SdMmc::pushFifo(uint32_t value) {
    // Handle 32-bit FIFO mode
    if (sdDataCtl & BIT(1)) {
        // Push a value to the 32-bit FIFO and check if full
        uint32_t size = (dataFifo32.size() << 2);
        if (size >= 0x200) return;
        dataFifo32.push_back(value);
        sdData32Irq |= BIT(9); // Not empty
        if (size + 4 < sdData32Blklen) return;

        // Trigger a 32-bit FIFO full interrupt if enabled and finish a block
        sdData32Irq |= BIT(8);
        if (sdData32Irq & BIT(11))
            core->interpreter[1].sendInterrupt(40);
        curBlock--;
        return;
    }

    // Push two values to the 16-bit FIFO and check if full
    uint32_t size = (dataFifo16.size() << 1);
    if (size >= 0x200) return;
    dataFifo16.push_back(value);
    dataFifo16.push_back(value >> 16);
    if (size + 4 < sdData16Blklen) return;

    // Trigger a 16-bit FIFO full interrupt and finish a block
    sendInterrupt(24);
    curBlock--;
}

void SdMmc::pushResponse(uint32_t value) {
    // Push a value to the response registers, shifting old data over
    sdResponse[3] = sdResponse[2];
    sdResponse[2] = sdResponse[1];
    sdResponse[1] = sdResponse[0];
    sdResponse[0] = value;
}

void SdMmc::readBlock() {
    // Check for high capacity and hardcode the block length if so
    bool hc = (~sdPortSelect & sdhc);
    uint32_t len = (hc ? 0x80 : blockLen);

    // Read a block of data from the SD or MMC if present, in 512- or 1-byte units
    uint32_t data[0x80];
    if (FILE *src = (sdPortSelect & BIT(0)) ? nand : sd) {
        fseek(src, hc ? (int64_t(curAddress) << 9) : curAddress, SEEK_SET);
        fread(data, sizeof(uint32_t), len, src);
    }
    else {
        memset(data, 0, sizeof(data));
    }

    // Push the data to a FIFO
    LOG_INFO("Reading %s block from 0x%X with size 0x%X\n",
        (sdPortSelect & BIT(0)) ? "MMC" : "SD", curAddress, len << 2);
    for (int i = 0; i < len; i++) pushFifo(data[i]);

    // Change to read or idle state based on blocks left and trigger a DRQ
    cardStatus = (cardStatus & ~0x1E00) | ((curBlock ? 0x5 : 0x4) << 9);
    curAddress += hc ? 1 : (len << 2);
    core->ndma[1].setDrq(0x8);
}

void SdMmc::writeBlock() {
    // Check for high capacity and hardcode the block length if so
    bool hc = (~sdPortSelect & sdhc);
    uint32_t len = (hc ? 0x80 : blockLen);

    // Pop a block of data from a FIFO
    uint32_t data[0x80];
    LOG_INFO("Writing %s block to 0x%X with size 0x%X\n",
        (sdPortSelect & BIT(0)) ? "MMC" : "SD", curAddress, len << 2);
    for (int i = 0; i < len; i++) data[i] = popFifo();

    // Write the data to the SD or MMC if present, in 512- or 1-byte units
    if (FILE *dst = (sdPortSelect & BIT(0)) ? nand : sd) {
        fseek(dst, hc ? (int64_t(curAddress) << 9) : curAddress, SEEK_SET);
        fread(data, sizeof(uint32_t), len, dst);
    }

    // Change state and trigger a DRQ or end interrupt based on blocks left
    cardStatus = (cardStatus & ~0x1E00) | ((curBlock ? 0x6 : 0x4) << 9);
    curAddress += hc ? 1 : (len << 2);
    if (!curBlock) return sendInterrupt(2);
    core->ndma[1].setDrq(0x8);
}

void SdMmc::runCommand() {
    // Ensure the command is being run in normal mode
    if (cardStatus & BIT(5)) {
        LOG_WARN("Tried to run %s CMD in app mode\n", (sdPortSelect & BIT(0)) ? "MMC" : "SD");
        cardStatus &= ~BIT(5);
        return;
    }

    // Execute a normal SD/MMC command
    switch (uint8_t cmd = sdCmd & 0x3F) {
        case 2: return getCid(); // ALL_GET_CID (stub)
        case 8: return setIfCond(); // SET_IF_COND
        case 9: return getCsd(); // GET_CSD
        case 10: return getCid(); // GET_CID
        case 13: return getStatus(); // GET_STATUS
        case 16: return setBlocklen(); // SET_BLOCKLEN
        case 17: return readSingleBlock(); // READ_SINGLE_BLOCK
        case 18: return readMultiBlock(); // READ_MULTIPLE_BLOCK
        case 24: return writeSingleBlock(); // WRITE_SINGLE_BLOCK
        case 25: return writeMultiBlock(); // WRITE_MULTIPLE_BLOCK
        case 55: return appCmd(); // APP_CMD

    case 1: // SEND_OP_COND
    case 3: // SET_RELATIVE_ADDR
        // Stub commands that want response bit 31 to be set
        LOG_WARN("Stubbed %s command: CMD%d\n", (sdPortSelect & BIT(0)) ? "MMC" : "SD", cmd);
        return pushResponse(0x80000000);

    default:
        // Assume response R1 for unknown commands
        LOG_WARN("Unknown %s command: CMD%d\n", (sdPortSelect & BIT(0)) ? "MMC" : "SD", cmd);
        return pushResponse(cardStatus);
    }
}

void SdMmc::runAppCommand() {
    // Ensure the command is being run in app mode
    if (~cardStatus & BIT(5)) {
        LOG_WARN("Tried to run %s ACMD in normal mode\n", (sdPortSelect & BIT(0)) ? "MMC" : "SD");
        return;
    }

    // Return to normal mode for the next command
    cardStatus &= ~BIT(5);

    // Execute an app SD/MMC command
    switch (uint8_t cmd = sdCmd & 0x3F) {
        case 13: return sdStatus(); // SD_STATUS
        case 41: return sdSendOpCond(); // SD_SEND_OP_COND
        case 51: return getScr(); // GET_SCR

    default:
        // Assume response R1 for unknown commands
        LOG_WARN("Unknown %s command: ACMD%d\n", (sdPortSelect & BIT(0)) ? "MMC" : "SD", cmd);
        return pushResponse(cardStatus);
    }
}

void SdMmc::setIfCond() {
    // Respond with the same voltage and check pattern that was input
    pushResponse(sdCmdParam & 0xFFF);
}

void SdMmc::getCsd() {
    // Pretend to read the 128-bit CSD register
    for (int i = 0; i < 4; i++)
        pushResponse(0);
}

void SdMmc::getCid() {
    // Get the card ID value as a response
    for (int i = 0; i < 4; i++)
        pushResponse(mmcCid[3 - i]);
}

void SdMmc::getStatus() {
    // Get the status register without doing anything else
    pushResponse(cardStatus);
}

void SdMmc::setBlocklen() {
    // Set the word length for multi-block reads and writes
    blockLen = std::min(0x200U, sdCmdParam + 3) >> 2;
    pushResponse(cardStatus);
}

void SdMmc::readSingleBlock() {
    // Read a single block of data
    curAddress = sdCmdParam;
    curBlock = 1;
    pushResponse(cardStatus);
    core->schedule(SDMMC_READ_BLOCK, 1);
}

void SdMmc::readMultiBlock() {
    // Read the first of multiple blocks of data
    curAddress = sdCmdParam;
    curBlock = sdData16Blkcnt;
    pushResponse(cardStatus);
    core->schedule(SDMMC_READ_BLOCK, 1);
}

void SdMmc::writeSingleBlock() {
    // Prepare to write a single block of data
    curAddress = sdCmdParam;
    curBlock = 1;
    pushResponse(cardStatus);
    startWrite();
}

void SdMmc::writeMultiBlock() {
    // Prepare to write multiple blocks of data
    curAddress = sdCmdParam;
    curBlock = sdData16Blkcnt;
    pushResponse(cardStatus);
    startWrite();
}

void SdMmc::appCmd() {
    // Switch to app command mode
    cardStatus |= BIT(5);
    pushResponse(cardStatus);
}

void SdMmc::sdStatus() {
    // Pretend to read the 512-bit SD status register
    curBlock = 1;
    for (int i = 0; i < 16; i++)
        pushFifo(0);
    pushResponse(cardStatus);
}

void SdMmc::sdSendOpCond() {
    // Set the SD voltage window and return it along with high-capacity status
    opCond = (opCond & ~0xFFFFFF) | (sdCmdParam & 0xFFFFFF);
    pushResponse(opCond | ((~sdPortSelect & sdhc) << 30));
}

void SdMmc::getScr() {
    // Pretend to read the 64-bit SD configuration register
    curBlock = 1;
    pushFifo(0);
    pushFifo(0);
    pushResponse(cardStatus);
}

uint16_t SdMmc::readData16Fifo() {
    // Pop a value from the 16-bit FIFO and check if empty
    if (dataFifo16.empty()) return sdData16Fifo;
    sdData16Fifo = dataFifo16.front();
    dataFifo16.pop_front();
    if (!dataFifo16.empty()) return sdData16Fifo;

    // Trigger a 16-bit FIFO empty interrupt and read another block or finish with a data end interrupt
    sendInterrupt(25);
    core->ndma[1].clearDrq(0x8);
    (cardStatus & 0x1E00) == (0x5 << 9) ? core->schedule(SDMMC_READ_BLOCK, 1) : sendInterrupt(2);
    return sdData16Fifo;
}

uint32_t SdMmc::readData32Fifo() {
    // Pop a value from the 32-bit read FIFO and check if empty
    if (dataFifo32.empty()) return sdData32Fifo;
    sdData32Fifo = dataFifo32.front();
    dataFifo32.pop_front();
    sdData32Irq &= ~BIT(8); // Not full
    if (!dataFifo32.empty()) return sdData32Fifo;

    // Trigger a 32-bit FIFO empty interrupt if enabled
    sdData32Irq &= ~BIT(9);
    if (sdData32Irq & BIT(12))
        core->interpreter[1].sendInterrupt(40);
    core->ndma[1].clearDrq(0x8);

    // Read another block or finish with a data end interrupt
    (cardStatus & 0x1E00) == (0x5 << 9) ? core->schedule(SDMMC_READ_BLOCK, 1) : sendInterrupt(2);
    return sdData32Fifo;
}

void SdMmc::writeCmd(uint16_t mask, uint16_t value) {
    // Write to the SD_CMD register
    // TODO: handle response type and other bits
    sdCmd = (sdCmd & ~mask) | (value & mask);

    // Run the written command and send an interrupt for completion
    sendInterrupt(0);
    switch (uint8_t type = (sdCmd >> 6) & 0x3) {
        case 0: return runCommand();
        case 1: return runAppCommand();

    default:
        LOG_WARN("Unknown %s command type: %d\n", (sdPortSelect & BIT(0)) ? "MMC" : "SD", type);
        return;
    }
}

void SdMmc::writePortSelect(uint16_t mask, uint16_t value) {
    // Write to the SD_PORT_SELECT register
    mask &= 0xF;
    sdPortSelect = (sdPortSelect & ~mask) | (value & mask);
}

void SdMmc::writeCmdParam(uint32_t mask, uint32_t value) {
    // Write to the SD_CMD_PARAM parameter
    sdCmdParam = (sdCmdParam & ~mask) | (value & mask);
}

void SdMmc::writeData16Blkcnt(uint16_t mask, uint16_t value) {
    // Write to the SD_DATA16_BLKCNT block count
    sdData16Blkcnt = (sdData16Blkcnt & ~mask) | (value & mask);
}

void SdMmc::writeIrqStatus(uint32_t mask, uint32_t value) {
    // Acknowledge bits in the SD_IRQ_STATUS register
    mask &= ~0xA0; // Always set
    sdIrqStatus = (sdIrqStatus & ~mask) | (sdIrqStatus & value & mask);
}

void SdMmc::writeIrqMask(uint32_t mask, uint32_t value) {
    // Write to the SD_IRQ_MASK register
    mask &= 0x8B7F031D;
    uint32_t old = sdIrqMask;
    sdIrqMask = (sdIrqMask & ~mask) | (value & mask);

    // Trigger an ARM9 interrupt if a requested bit was newly unmasked
    if (sdIrqStatus & old & ~sdIrqMask)
        core->interpreter[1].sendInterrupt(40);
}

void SdMmc::writeData16Blklen(uint16_t mask, uint16_t value) {
    // Write to the SD_DATA16_BLKLEN block length, clipping past 0x200
    mask &= 0x3FF;
    sdData16Blklen = std::min(0x200, (sdData16Blklen & ~mask) | (value & mask));
}

void SdMmc::writeData16Fifo(uint16_t mask, uint16_t value) {
    // Push a value to the 16-bit FIFO and check if full
    uint32_t size = (dataFifo16.size() << 1);
    if (size >= 0x200) return;
    dataFifo16.push_back(value & mask);
    if (size + 2 < sdData16Blklen) return;

    // Trigger a 16-bit FIFO full interrupt and write a block if in write mode
    sendInterrupt(24);
    core->ndma[1].clearDrq(0x8);
    if ((cardStatus & 0x1E00) == (0x6 << 9))
        core->schedule(SDMMC_WRITE_BLOCK, 1);
}

void SdMmc::writeDataCtl(uint16_t mask, uint16_t value) {
    // Write to the SD_DATA_CTL register
    mask &= 0x22;
    sdDataCtl = (sdDataCtl & ~mask) | (value & mask);
}

void SdMmc::writeData32Irq(uint16_t mask, uint16_t value) {
    // Empty the 32-bit FIFO if requested
    if (value & mask & BIT(10)) {
        sdData32Irq &= ~(BIT(8) | BIT(9)); // Not full, empty
        dataFifo32 = {};
    }

    // Write to the SD_DATA32_IRQ register
    mask &= 0x1802;
    sdData32Irq = (sdData32Irq & ~mask) | (value & mask);
}

void SdMmc::writeData32Blklen(uint16_t mask, uint16_t value) {
    // Write to the SD_DATA32_BLKLEN block length
    mask &= 0x3FF;
    sdData32Blklen = (sdData32Blklen & ~mask) | (value & mask);
}

void SdMmc::writeData32Fifo(uint32_t mask, uint32_t value) {
    // Push a value to the 32-bit FIFO and check if full
    uint32_t size = (dataFifo32.size() << 2);
    if (size >= 0x200) return;
    dataFifo32.push_back(value & mask);
    sdData32Irq |= BIT(9); // Not empty
    if (size + 4 < sdData32Blklen) return;

    // Trigger a 32-bit FIFO full interrupt if enabled
    sdData32Irq |= BIT(8);
    if (sdData32Irq & BIT(11))
        core->interpreter[1].sendInterrupt(40);
    core->ndma[1].clearDrq(0x8);

    // Write a block if in write mode
    if ((cardStatus & 0x1E00) == (0x6 << 9))
        core->schedule(SDMMC_WRITE_BLOCK, 1);
}
