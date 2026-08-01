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

#include <algorithm>
#include <cstring>
#include <thread>

#include "core.h"

Core::Core(std::string ndsRom, std::string gbaRom, int id, int ndsRomFd, int gbaRomFd,
    int ndsSaveFd, int gbaSaveFd, int ndsStateFd, int gbaStateFd, int ndsCheatFd):
        id(id), actionReplay(this), aes(this), cartridgeGba(this), cartridgeNds(this), cp15(this), divSqrt(this), dldi(
        this), dma { Dma(this, 0), Dma(this, 1) }, gpu(this), gpu2D { Gpu2D(this, 0), Gpu2D(this, 1) }, gpu3D(this),
        gpu3DRenderer(this), hleArm7(this), hleBios { HleBios(this, 0, HleBios::swiTable9), HleBios(this, 1, HleBios::
        swiTable7), HleBios(this, 1, HleBios::swiTableGba) }, i2c(this), input(this), interpreter { Interpreter(
        this, 0), Interpreter(this, 1) }, ipc(this), memory(this), ndma { Ndma(this, 0), Ndma(this, 1) }, rtc(this),
        saveStates(this), sdMmc(this), spi(this), spu(this), timers { Timers(this, 0), Timers(this, 1) }, wifi(this) {
    // Set DSi mode now and ignore changes to it later
    dsiMode = Settings::dsiMode;
    updateRun();

    // Try to load BIOS and firmware; require DS files when not direct booting
    bool required = !Settings::directBoot || (ndsRom == "" && gbaRom == "" && ndsRomFd == -1 && gbaRomFd == -1);
    if (!memory.loadBios9() && (required || dsiMode)) throw dsiMode ? ERROR_DSI_BIOS : ERROR_NDS_BIOS;
    if (!memory.loadBios7() && (required || dsiMode)) throw dsiMode ? ERROR_DSI_BIOS : ERROR_NDS_BIOS;
    if (!spi.loadFirmware() && required && !dsiMode) throw ERROR_NDS_FIRM;
    realGbaBios = memory.loadGbaBios();

    // Define the tasks that can be scheduled
    tasks[UPDATE_RUN] = std::bind(&Core::updateRun, this);
    tasks[RESET_CYCLES] = std::bind(&Core::resetCycles, this);
    tasks[CART9_WORD_READY] = std::bind(&CartridgeNds::wordReady, &cartridgeNds, 0);
    tasks[CART7_WORD_READY] = std::bind(&CartridgeNds::wordReady, &cartridgeNds, 1);
    tasks[DMA9_TRANSFER0] = std::bind(&Dma::transfer, &dma[0], 0);
    tasks[DMA9_TRANSFER1] = std::bind(&Dma::transfer, &dma[0], 1);
    tasks[DMA9_TRANSFER2] = std::bind(&Dma::transfer, &dma[0], 2);
    tasks[DMA9_TRANSFER3] = std::bind(&Dma::transfer, &dma[0], 3);
    tasks[DMA7_TRANSFER0] = std::bind(&Dma::transfer, &dma[1], 0);
    tasks[DMA7_TRANSFER1] = std::bind(&Dma::transfer, &dma[1], 1);
    tasks[DMA7_TRANSFER2] = std::bind(&Dma::transfer, &dma[1], 2);
    tasks[DMA7_TRANSFER3] = std::bind(&Dma::transfer, &dma[1], 3);
    tasks[NDS_SCANLINE256] = std::bind(&Gpu::scanline256, &gpu);
    tasks[NDS_SCANLINE355] = std::bind(&Gpu::scanline355, &gpu);
    tasks[GBA_SCANLINE240] = std::bind(&Gpu::gbaScanline240, &gpu);
    tasks[GBA_SCANLINE308] = std::bind(&Gpu::gbaScanline308, &gpu);
    tasks[GPU3D_COMMANDS] = std::bind(&Gpu3D::runCommands, &gpu3D);
    tasks[ARM9_INTERRUPT] = std::bind(&Interpreter::interrupt, &interpreter[0]);
    tasks[ARM7_INTERRUPT] = std::bind(&Interpreter::interrupt, &interpreter[1]);
    tasks[NDS_SPU_SAMPLE] = std::bind(&Spu::runSample, &spu);
    tasks[GBA_SPU_SAMPLE] = std::bind(&Spu::runGbaSample, &spu);
    tasks[TIMER9_OVERFLOW0] = std::bind(&Timers::overflow, &timers[0], 0);
    tasks[TIMER9_OVERFLOW1] = std::bind(&Timers::overflow, &timers[0], 1);
    tasks[TIMER9_OVERFLOW2] = std::bind(&Timers::overflow, &timers[0], 2);
    tasks[TIMER9_OVERFLOW3] = std::bind(&Timers::overflow, &timers[0], 3);
    tasks[TIMER7_OVERFLOW0] = std::bind(&Timers::overflow, &timers[1], 0);
    tasks[TIMER7_OVERFLOW1] = std::bind(&Timers::overflow, &timers[1], 1);
    tasks[TIMER7_OVERFLOW2] = std::bind(&Timers::overflow, &timers[1], 2);
    tasks[TIMER7_OVERFLOW3] = std::bind(&Timers::overflow, &timers[1], 3);
    tasks[WIFI_COUNT_MS] = std::bind(&Wifi::countMs, &wifi);
    tasks[WIFI_TRANS_REPLY] = std::bind(&Wifi::transmitPacket, &wifi, CMD_REPLY);
    tasks[WIFI_TRANS_ACK] = std::bind(&Wifi::transmitPacket, &wifi, CMD_ACK);
    tasks[AES_UPDATE] = std::bind(&Aes::update, &aes);
    tasks[NDMA9_UPDATE] = std::bind(&Ndma::update, &ndma[0]);
    tasks[NDMA7_UPDATE] = std::bind(&Ndma::update, &ndma[1]);
    tasks[SDMMC_READ_BLOCK] = std::bind(&SdMmc::readBlock, &sdMmc);
    tasks[SDMMC_WRITE_BLOCK] = std::bind(&SdMmc::writeBlock, &sdMmc);

    // Schedule initial tasks for NDS mode
    schedule(RESET_CYCLES, 0x7FFFFFFF);
    schedule(NDS_SCANLINE256, 256 * 6);
    schedule(NDS_SCANLINE355, 355 * 6);
    schedule(NDS_SPU_SAMPLE, 512 * 2);

    // Initialize memory maps and anything else that needs it
    memory.updateMap9(0x00000000, 0xFFFFFFFF);
    memory.updateMap7(0x00000000, 0xFFFFFFFF);
    interpreter[0].init();
    interpreter[1].init();
    gpu.init();

    // HLE boot stage 1 in DSi mode since it's not easily dumpable
    if (dsiMode) {
        // Read the stage 2 header
        uint8_t header[0x200];
        FILE *nand = fopen(Settings::dsiNandPath.c_str(), "rb");
        if (!nand) throw ERROR_DSI_NAND;
        fseek(nand, 0x200, SEEK_SET);
        fread(header, sizeof(uint8_t), 0x200, nand);

        // Apply the header's WRAM mappings
        memory.writeWramCnt(header[0x1AF]);
        for (int i = 0; i < 4; i++) {
            memory.writeMbk1(i, header[0x180 + i]);
            memory.writeMbk23(i + 0, header[0x184 + i]);
            memory.writeMbk23(i + 4, header[0x188 + i]);
            memory.writeMbk45(i + 0, header[0x18C + i]);
            memory.writeMbk45(i + 4, header[0x190 + i]);
            if (i >= 2) continue;
            memory.writeMbk6(i, -1, U8TO32(header, 0x194 + i * 0xC));
            memory.writeMbk7(i, -1, U8TO32(header, 0x198 + i * 0xC));
            memory.writeMbk8(i, -1, U8TO32(header, 0x19C + i * 0xC));
        }

        // Normally, a Y-key is extracted from an RSA block and used to generate the final key
        // Just hardcode the result here instead, to avoid needing the RSA key
        aes.writeKey(0, 0, -1, 0x8080EE98);
        aes.writeKey(0, 1, -1, 0xF6B46C00);
        aes.writeKey(0, 2, -1, 0x626EC23A);
        aes.writeKey(0, 3, -1, 0xAD34ECF9);

        // Extract ARM9 code details and load the code itself
        uint32_t offset9 = U8TO32(header, 0x20);
        uint32_t size9 = U8TO32(header, 0x24);
        uint32_t entry9 = interpreter[0].entryAddr = U8TO32(header, 0x28);
        uint32_t align9 = U8TO32(header, 0x2C);
        uint8_t *code9 = new uint8_t[size9];
        fseek(nand, offset9, SEEK_SET);
        fread(code9, sizeof(uint8_t), size9, nand);

        // Configure AES for CTR decryption with IV based on aligned ARM9 code size
        aes.writeBlkcnt(-1, size9 << 12);
        aes.writeIv(0, -1, align9);
        aes.writeIv(1, -1, -align9);
        aes.writeIv(2, -1, ~align9);
        aes.writeIv(3, -1, 0);
        aes.writeCnt(-1, 0xA000C000);

        // Send ARM9 code through the AES engine and write it to memory
        for (int i = 0; i < size9 / 64; i++) {
            for (int j = 0; j < 64; j += 4)
                aes.writeWrfifo(-1, U8TO32(code9, i * 64 + j));
            aes.update();
            for (int j = 0; j < 64; j += 4)
                memory.write<uint32_t>(0, entry9 + i * 64 + j, aes.readRdfifo());
            aes.update();
        }

        // Reset the AES FIFO
        aes.writeCnt(-1, 0xC00);

        // Extract ARM7 code details and load the code itself
        uint32_t offset7 = U8TO32(header, 0x30);
        uint32_t size7 = U8TO32(header, 0x34);
        uint32_t entry7 = interpreter[1].entryAddr = U8TO32(header, 0x38);
        uint32_t align7 = U8TO32(header, 0x3C);
        uint8_t *code7 = new uint8_t[size7];
        fseek(nand, offset7, SEEK_SET);
        fread(code7, sizeof(uint8_t), size7, nand);

        // Configure AES for CTR decryption with IV based on aligned ARM7 code size
        aes.writeBlkcnt(-1, size7 << 12);
        aes.writeIv(0, -1, align7);
        aes.writeIv(1, -1, -align7);
        aes.writeIv(2, -1, ~align7);
        aes.writeIv(3, -1, 0);
        aes.writeCnt(-1, 0xA000C000);

        // Send ARM7 code through the AES engine and write it to memory
        for (int i = 0; i < size7 / 64; i++) {
            for (int j = 0; j < 64; j += 4)
                aes.writeWrfifo(-1, U8TO32(code7, i * 64 + j));
            aes.update();
            for (int j = 0; j < 64; j += 4)
                memory.write<uint32_t>(1, entry7 + i * 64 + j, aes.readRdfifo());
            aes.update();
        }

        // Clean up what was used
        delete[] code9;
        delete[] code7;
        fclose(nand);

        // Map Instruction TCM to its entire available space
        cp15.write(1, 0, 0, 0x0005707D); // Control
        cp15.write(9, 1, 1, 0x00000020); // ITCM size

        // Copy some keys from BIOS9 to ITCM
        for (int i = 0; i < 0x400; i += 4)
            memory.write<uint32_t>(0, 0x1FFC400 + i, memory.read<uint32_t>(0, 0xFFFF87F4 + i));
        for (int i = 0; i < 0x80; i += 4)
            memory.write<uint32_t>(0, 0x1FFC800 + i, memory.read<uint32_t>(0, 0xFFFF9920 + i));
        for (int i = 0; i < 0x1048; i += 4)
            memory.write<uint32_t>(0, 0x1FFC894 + i, memory.read<uint32_t>(0, 0xFFFF99A0 + i));
        for (int i = 0; i < 0x1048; i += 4)
            memory.write<uint32_t>(0, 0x1FFD8DC + i, memory.read<uint32_t>(0, 0xFFFFA9E8 + i));

        // Copy some keys from BIOS7 to WRAM
        for (int i = 0; i < 0x200; i += 4)
            memory.write<uint32_t>(1, 0x3FFC400 + i, memory.read<uint32_t>(1, 0x8188 + i));
        for (int i = 0; i < 0x40; i += 4)
            memory.write<uint32_t>(1, 0x3FFC600 + i, memory.read<uint32_t>(1, 0xB5D8 + i));
        for (int i = 0; i < 0x1048; i += 4)
            memory.write<uint32_t>(1, 0x3FFC654 + i, memory.read<uint32_t>(1, 0xC6D0 + i));
        for (int i = 0; i < 0x1048; i += 4)
            memory.write<uint32_t>(1, 0x3FFD69C + i, memory.read<uint32_t>(1, 0xD718 + i));

        // Initialize SD/MMC and fill in the info struct
        uint32_t *mmcCid = sdMmc.init();
        for (int i = 0; i < 0x10; i += 4)
            memory.write<uint32_t>(1, 0x3FFE6E4 + i, mmcCid[i / 4]);

        // Initialize string-based AES slot 0 key X values
        aes.writeKeyx(0, 0, -1, 0x746E694E); // "Nint"
        aes.writeKeyx(0, 1, -1, 0x6F646E65); // "endo"

        // Initialize console-based AES slot 1 key X values
        aes.writeKeyx(1, 0, -1, 0x4E00004A);
        aes.writeKeyx(1, 1, -1, 0x4A00004E);
        aes.writeKeyx(1, 2, -1, sdMmc.readConsoleId(1) ^ 0xC80C4B72);
        aes.writeKeyx(1, 3, -1, sdMmc.readConsoleId(0));

        // Initialize string-based AES slot 2 key X values
        aes.writeKeyx(2, 0, -1, 0x746E694E); // "Nint"
        aes.writeKeyx(2, 1, -1, 0x6F646E65); // "endo"
        aes.writeKeyx(2, 2, -1, 0x00534420); // " DS"

        // Initialize console-based AES slot 3 key X/Y values
        aes.writeKeyx(3, 0, -1, sdMmc.readConsoleId(0));
        aes.writeKeyx(3, 1, -1, sdMmc.readConsoleId(0) ^ 0x24EE6906);
        aes.writeKeyx(3, 2, -1, sdMmc.readConsoleId(1) ^ 0xE65B601D);
        aes.writeKeyx(3, 3, -1, sdMmc.readConsoleId(1));
        aes.writeKeyy(3, 0, -1, 0x0AB9DC76);
        aes.writeKeyy(3, 1, -1, 0xBD4DC4D3);
        aes.writeKeyy(3, 2, -1, 0x202DDD1D);

        interpreter[0].directBoot();
        interpreter[1].directBoot();
    }

    if (gbaRom != "" || gbaRomFd != -1) {
        // Load a GBA ROM
        if (!cartridgeGba.setRom(gbaRom, gbaRomFd, gbaSaveFd, gbaStateFd, -1))
            throw ERROR_ROM;

        // Enable GBA mode right away if direct boot is enabled
        if (Settings::directBoot && ndsRom == "" && ndsRomFd == -1) {
            memory.write<uint16_t>(0, 0x4000304, 0x8003); // POWCNT1
            enterGbaMode();
        }
    }

    if (ndsRom != "" || ndsRomFd != -1) {
        // Load an NDS ROM
        if (!cartridgeNds.setRom(ndsRom, ndsRomFd, ndsSaveFd, ndsStateFd, ndsCheatFd))
            throw ERROR_ROM;

        // Load cheats if any exist
        actionReplay.loadCheats();

        // Prepare to boot the NDS ROM directly if direct boot is enabled
        if (Settings::directBoot) {
            // Set some registers as the BIOS/firmware would
            cp15.write(1, 0, 0, 0x0005707D); // CP15 Control
            cp15.write(9, 1, 0, 0x0300000A); // Data TCM base/size
            cp15.write(9, 1, 1, 0x00000020); // Instruction TCM size
            memory.write<uint8_t>(0, 0x4000247, 0x03); // WRAMCNT
            memory.write<uint8_t>(0, 0x4000300, 0x01); // POSTFLG (ARM9)
            memory.write<uint8_t>(1, 0x4000300, 0x01); // POSTFLG (ARM7)
            memory.write<uint16_t>(0, 0x4000304, 0x0001); // POWCNT1
            memory.write<uint16_t>(1, 0x4000504, 0x0200); // SOUNDBIAS

            // Set some memory values as the BIOS/firmware would
            memory.write<uint32_t>(0, 0x27FF800, 0x00001FC2); // Chip ID 1
            memory.write<uint32_t>(0, 0x27FF804, 0x00001FC2); // Chip ID 2
            memory.write<uint16_t>(0, 0x27FF850, 0x5835); // ARM7 BIOS CRC
            memory.write<uint16_t>(0, 0x27FF880, 0x0007); // Message from ARM9 to ARM7
            memory.write<uint16_t>(0, 0x27FF884, 0x0006); // ARM7 boot task
            memory.write<uint32_t>(0, 0x27FFC00, 0x00001FC2); // Copy of chip ID 1
            memory.write<uint32_t>(0, 0x27FFC04, 0x00001FC2); // Copy of chip ID 2
            memory.write<uint16_t>(0, 0x27FFC10, 0x5835); // Copy of ARM7 BIOS CRC
            memory.write<uint16_t>(0, 0x27FFC40, 0x0001); // Boot indicator

            cartridgeNds.directBoot();
            interpreter[0].directBoot();
            interpreter[1].directBoot();
            spi.directBoot();
        }
    }

    // Initialize HLE ARM7 if enabled in DS mode
    if (!gbaMode && Settings::arm7Hle) {
        arm7Hle = true;
        hleArm7.init();
    }

    // Let the core run
    running.store(true);
}

void Core::saveState(FILE *file) {
    // Write state data to the file
    fwrite(&arm7Hle, sizeof(arm7Hle), 1, file);
    fwrite(&dsiMode, sizeof(dsiMode), 1, file);
    fwrite(&gbaMode, sizeof(gbaMode), 1, file);
    fwrite(&globalCycles, sizeof(globalCycles), 1, file);

    // Parse the scheduler and save its events
    uint32_t count = events.size();
    fwrite(&count, sizeof(count), 1, file);
    for (uint32_t i = 0; i < count; i++)
        fwrite(&events[i], sizeof(events[i]), 1, file);
}

void Core::loadState(FILE *file) {
    // Read state data from the file
    fread(&arm7Hle, sizeof(arm7Hle), 1, file);
    fread(&dsiMode, sizeof(dsiMode), 1, file);
    fread(&gbaMode, sizeof(gbaMode), 1, file);
    fread(&globalCycles, sizeof(globalCycles), 1, file);

    // Reset the scheduler and refill it with loaded events
    events.clear();
    uint32_t count;
    SchedEvent event(MAX_TASKS, 0);
    fread(&count, sizeof(count), 1, file);
    for (uint32_t i = 0; i < count; i++) {
        fread(&event, sizeof(event), 1, file);
        events.push_back(event);
    }

    // Update the run function pointer
    updateRun();
}

void Core::updateRun() {
    // Set the run function based on active CPUs and core mode
    if (interpreter[0].halted && interpreter[1].halted)
        runFunc = &Interpreter::runCoreNone;
    else if (gbaMode)
        runFunc = &Interpreter::runCoreSingle<true, 0>;
    else if (dsiMode)
        runFunc = &Interpreter::runCoreDsi;
    else if (!interpreter[0].halted && !interpreter[1].halted)
        runFunc = &Interpreter::runCoreNds;
    else if (interpreter[0].halted)
        runFunc = &Interpreter::runCoreSingle<true, 1>;
    else
        runFunc = &Interpreter::runCoreSingle<false, 0>;
    running.store(false);
}

void Core::resetCycles() {
    // Reset the global cycle count periodically to prevent overflow
    for (size_t i = 0; i < events.size(); i++)
        events[i].cycles -= globalCycles;
    for (int i = 0; i < 2; i++)
        interpreter[i].resetCycles(), timers[i].resetCycles();
    globalCycles -= globalCycles;
    schedule(RESET_CYCLES, 0x7FFFFFFF);
}

void Core::schedule(SchedTask task, uint32_t cycles) {
    // Add a task to the scheduler, sorted by least to most cycles until execution
    SchedEvent event(task, globalCycles + cycles);
    auto it = std::upper_bound(events.cbegin(), events.cend(), event);
    events.insert(it, event);
}

void Core::enterGbaMode() {
    // Switch to GBA mode
    gbaMode = true;
    interpreter[0].halt(2);
    updateRun();

    // Reset the scheduler and schedule initial tasks for GBA mode
    events.clear();
    schedule(RESET_CYCLES, 1);
    schedule(GBA_SCANLINE240, 240 * 4);
    schedule(GBA_SCANLINE308, 308 * 4);
    schedule(GBA_SPU_SAMPLE, 512);

    // Reset the system for GBA mode
    memory.updateMap7(0x00000000, 0xFFFFFFFF);
    interpreter[1].init();
    rtc.reset();

    // Set VRAM blocks A and B to plain access mode
    // This is used by the GPU to access the VRAM borders
    memory.write<uint8_t>(0, 0x4000240, 0x80); // VRAMCNT_A
    memory.write<uint8_t>(0, 0x4000241, 0x80); // VRAMCNT_B

    // Disable HLE BIOS if a real one was loaded
    if (realGbaBios) {
        interpreter[1].bios = nullptr;
        return;
    }

    // Enable HLE BIOS and boot the GBA ROM directly
    interpreter[1].bios = &hleBios[2];
    interpreter[1].directBoot();
    memory.write<uint16_t>(1, 0x4000088, 0x200); // SOUNDBIAS
}

void Core::endFrame() {
    // Break execution at the end of a frame and count it
    running.store(false);
    fpsCount++;

    // Run HLE ARM7 per-frame tasks if enabled
    if (arm7Hle)
        hleArm7.runFrame();

    // Update the FPS and reset the counter every second
    std::chrono::duration<double> fpsTime = std::chrono::steady_clock::now() - lastFpsTime;
    if (fpsTime.count() >= 1.0f) {
        fps = fpsCount;
        fpsCount = 0;
        lastFpsTime = std::chrono::steady_clock::now();
    }

    // Schedule WiFi updates only when needed
    if (wifi.shouldSchedule())
        wifi.scheduleInit();
}
