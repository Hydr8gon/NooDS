/*
    Copyright 2019-2025 Hydr8gon

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
#include "core.h"

#define MS_CYCLES 34418

Wifi::Wifi(Core *core): core(core) {
    // Set some default BB register values
    bbRegisters[0x00] = 0x6D;
    bbRegisters[0x5D] = 0x01;
    bbRegisters[0x64] = 0xFF;
}

void Wifi::saveState(FILE *file) {
    // Write state data to the file
    fwrite(&scheduled, sizeof(scheduled), 1, file);
    fwrite(&wModeWep, sizeof(wModeWep), 1, file);
    fwrite(&wTxstatCnt, sizeof(wTxstatCnt), 1, file);
    fwrite(&wIrf, sizeof(wIrf), 1, file);
    fwrite(&wIe, sizeof(wIe), 1, file);
    fwrite(wMacaddr, 2, sizeof(wMacaddr) / 2, file);
    fwrite(wBssid, 2, sizeof(wBssid) / 2, file);
    fwrite(&wAidFull, sizeof(wAidFull), 1, file);
    fwrite(&wRxcnt, sizeof(wRxcnt), 1, file);
    fwrite(&wPowerstate, sizeof(wPowerstate), 1, file);
    fwrite(&wPowerforce, sizeof(wPowerforce), 1, file);
    fwrite(&wRxbufBegin, sizeof(wRxbufBegin), 1, file);
    fwrite(&wRxbufEnd, sizeof(wRxbufEnd), 1, file);
    fwrite(&wRxbufWrcsr, sizeof(wRxbufWrcsr), 1, file);
    fwrite(&wRxbufWrAddr, sizeof(wRxbufWrAddr), 1, file);
    fwrite(&wRxbufRdAddr, sizeof(wRxbufRdAddr), 1, file);
    fwrite(&wRxbufReadcsr, sizeof(wRxbufReadcsr), 1, file);
    fwrite(&wRxbufGap, sizeof(wRxbufGap), 1, file);
    fwrite(&wRxbufGapdisp, sizeof(wRxbufGapdisp), 1, file);
    fwrite(wTxbufLoc, 2, sizeof(wTxbufLoc) / 2, file);
    fwrite(&wBeaconInt, sizeof(wBeaconInt), 1, file);
    fwrite(&wTxbufReply1, sizeof(wTxbufReply1), 1, file);
    fwrite(&wTxbufReply2, sizeof(wTxbufReply2), 1, file);
    fwrite(&wTxreqRead, sizeof(wTxreqRead), 1, file);
    fwrite(&wTxstat, sizeof(wTxstat), 1, file);
    fwrite(&wUsCountcnt, sizeof(wUsCountcnt), 1, file);
    fwrite(&wUsComparecnt, sizeof(wUsComparecnt), 1, file);
    fwrite(&wCmdCountcnt, sizeof(wCmdCountcnt), 1, file);
    fwrite(&wUsCompare, sizeof(wUsCompare), 1, file);
    fwrite(&wUsCount, sizeof(wUsCount), 1, file);
    fwrite(&wPreBeacon, sizeof(wPreBeacon), 1, file);
    fwrite(&wCmdCount, sizeof(wCmdCount), 1, file);
    fwrite(&wBeaconCount, sizeof(wBeaconCount), 1, file);
    fwrite(&wRxbufCount, sizeof(wRxbufCount), 1, file);
    fwrite(&wTxbufWrAddr, sizeof(wTxbufWrAddr), 1, file);
    fwrite(&wTxbufCount, sizeof(wTxbufCount), 1, file);
    fwrite(&wTxbufGap, sizeof(wTxbufGap), 1, file);
    fwrite(&wTxbufGapdisp, sizeof(wTxbufGapdisp), 1, file);
    fwrite(&wPostBeacon, sizeof(wPostBeacon), 1, file);
    fwrite(&wBbWrite, sizeof(wBbWrite), 1, file);
    fwrite(&wBbRead, sizeof(wBbRead), 1, file);
    fwrite(&wTxSeqno, sizeof(wTxSeqno), 1, file);
    fwrite(bbRegisters, 1, sizeof(bbRegisters), file);
    fwrite(wConfig, 2, sizeof(wConfig) / 2, file);
}

void Wifi::loadState(FILE *file) {
    // Read state data from the file
    fread(&scheduled, sizeof(scheduled), 1, file);
    fread(&wModeWep, sizeof(wModeWep), 1, file);
    fread(&wTxstatCnt, sizeof(wTxstatCnt), 1, file);
    fread(&wIrf, sizeof(wIrf), 1, file);
    fread(&wIe, sizeof(wIe), 1, file);
    fread(wMacaddr, 2, sizeof(wMacaddr) / 2, file);
    fread(wBssid, 2, sizeof(wBssid) / 2, file);
    fread(&wAidFull, sizeof(wAidFull), 1, file);
    fread(&wRxcnt, sizeof(wRxcnt), 1, file);
    fread(&wPowerstate, sizeof(wPowerstate), 1, file);
    fread(&wPowerforce, sizeof(wPowerforce), 1, file);
    fread(&wRxbufBegin, sizeof(wRxbufBegin), 1, file);
    fread(&wRxbufEnd, sizeof(wRxbufEnd), 1, file);
    fread(&wRxbufWrcsr, sizeof(wRxbufWrcsr), 1, file);
    fread(&wRxbufWrAddr, sizeof(wRxbufWrAddr), 1, file);
    fread(&wRxbufRdAddr, sizeof(wRxbufRdAddr), 1, file);
    fread(&wRxbufReadcsr, sizeof(wRxbufReadcsr), 1, file);
    fread(&wRxbufGap, sizeof(wRxbufGap), 1, file);
    fread(&wRxbufGapdisp, sizeof(wRxbufGapdisp), 1, file);
    fread(wTxbufLoc, 2, sizeof(wTxbufLoc) / 2, file);
    fread(&wBeaconInt, sizeof(wBeaconInt), 1, file);
    fread(&wTxbufReply1, sizeof(wTxbufReply1), 1, file);
    fread(&wTxbufReply2, sizeof(wTxbufReply2), 1, file);
    fread(&wTxreqRead, sizeof(wTxreqRead), 1, file);
    fread(&wTxstat, sizeof(wTxstat), 1, file);
    fread(&wUsCountcnt, sizeof(wUsCountcnt), 1, file);
    fread(&wUsComparecnt, sizeof(wUsComparecnt), 1, file);
    fread(&wCmdCountcnt, sizeof(wCmdCountcnt), 1, file);
    fread(&wUsCompare, sizeof(wUsCompare), 1, file);
    fread(&wUsCount, sizeof(wUsCount), 1, file);
    fread(&wPreBeacon, sizeof(wPreBeacon), 1, file);
    fread(&wCmdCount, sizeof(wCmdCount), 1, file);
    fread(&wBeaconCount, sizeof(wBeaconCount), 1, file);
    fread(&wRxbufCount, sizeof(wRxbufCount), 1, file);
    fread(&wTxbufWrAddr, sizeof(wTxbufWrAddr), 1, file);
    fread(&wTxbufCount, sizeof(wTxbufCount), 1, file);
    fread(&wTxbufGap, sizeof(wTxbufGap), 1, file);
    fread(&wTxbufGapdisp, sizeof(wTxbufGapdisp), 1, file);
    fread(&wPostBeacon, sizeof(wPostBeacon), 1, file);
    fread(&wBbWrite, sizeof(wBbWrite), 1, file);
    fread(&wBbRead, sizeof(wBbRead), 1, file);
    fread(&wTxSeqno, sizeof(wTxSeqno), 1, file);
    fread(bbRegisters, 1, sizeof(bbRegisters), file);
    fread(wConfig, 2, sizeof(wConfig) / 2, file);
}

void Wifi::addConnection(Core *core) {
    // Add an external core to this one's connection list
    mutex.lock();
    connections.push_back(&core->wifi);
    mutex.unlock();

    // Add this core to the external one's connection list
    core->wifi.mutex.lock();
    core->wifi.connections.push_back(this);
    core->wifi.mutex.unlock();
}

void Wifi::remConnection(Core *core) {
    // Remove an external core from this one's connection list
    mutex.lock();
    auto position = std::find(connections.begin(), connections.end(), &core->wifi);
    connections.erase(position);
    mutex.unlock();

    // Remove this core from the external one's connection list
    core->wifi.mutex.lock();
    position = std::find(core->wifi.connections.begin(), core->wifi.connections.end(), this);
    core->wifi.connections.erase(position);
    core->wifi.mutex.unlock();
}

void Wifi::scheduleInit() {
    // Schedule an initial millisecond tick (this will reschedule itself as needed)
    core->schedule(WIFI_COUNT_MS, MS_CYCLES);
    scheduled = true;
}

void Wifi::countMs() {
    // Process any queued packets
    if (!packets.empty())
        receivePackets();

    if (wUsCountcnt) { // Counter enable
        // Decrement the beacon counter and trigger an interrupt if the pre-beacon value matches
        if (--wBeaconCount == wPreBeacon && wUsComparecnt)
            sendInterrupt(15);

        // Increment the main counter by a millisecond and handle compare events
        if ((wUsCount += 0x400) == wUsCompare || wBeaconCount == 0) {
            // Reload the beacon counter and trigger an interrupt with transmission if enabled
            wBeaconCount = wBeaconInt;
            if (wUsComparecnt) {
                sendInterrupt(14);
                if ((wTxbufLoc[BEACON_FRAME] & BIT(15)) && (wTxreqRead & BIT(BEACON_FRAME)))
                    transmitPacket(BEACON_FRAME);
            }
        }

        // Decrement the post-beacon counter and trigger an interrupt at zero
        if (wPostBeacon && --wPostBeacon == 0)
            sendInterrupt(13);
    }

    // Decrement the CMD counter every 10 microseconds and trigger an interrupt at zero
    if (wCmdCountcnt && (wCmdCount -= std::min<uint16_t>(0x400 / 10, wCmdCount)) == 0)
        sendInterrupt(12);

    // Reschedule the task as long as something is active
    if (!connections.empty() || wUsCountcnt || wCmdCountcnt)
        core->schedule(WIFI_COUNT_MS, MS_CYCLES);
    else
        scheduled = false;
}

void Wifi::sendInterrupt(int bit) {
    // Trigger a WiFi interrupt if W_IF & W_IE changes from zero
    if (!(wIe & wIrf) && (wIe & BIT(bit)))
        core->interpreter[1].sendInterrupt(24);

    // Set the interrupt's request bit
    wIrf |= BIT(bit);

    // Perform additional actions for beacon interrupts
    if (bit == 14) {
        wPostBeacon = 0xFFFF;
        wTxreqRead &= 0xFFF2;
    }
}

void Wifi::receivePackets() {
    // Start receiving packets
    sendInterrupt(6);
    mutex.lock();

    // Write all queued packets to the circular buffer
    for (uint32_t i = 0; i < packets.size(); i++) {
        uint16_t size = (packets[i][4] + 12) / 2;
        for (uint32_t j = 0; j < size; j++) {
            // Write a half-word of the packet to memory
            core->memory.write<uint16_t>(1, 0x4804000 + wRxbufWrcsr, packets[i][j]);

            // Increment the circular buffer address
            wRxbufWrcsr += 2;
            if (int bufSize = (wRxbufEnd & 0x1FFE) - (wRxbufBegin & 0x1FFE))
                wRxbufWrcsr = ((wRxbufBegin & 0x1FFE) + (wRxbufWrcsr - (wRxbufBegin & 0x1FFE)) % bufSize) & 0x1FFE;
        }

        // Schedule a CMD reply or ack shortly after a packet is received
        if (packets[i][6] == 0x0228) // CMD frame
            core->schedule(WIFI_TRANS_REPLY, 2048);
        else if ((packets[i][6] == 0x0118 || packets[i][6] == 0x0158) && wCmdCount) // CMD reply
            core->schedule(WIFI_TRANS_ACK, 2048);
        delete[] packets[i];
    }

    // Finish receiving packets
    packets.clear();
    mutex.unlock();
    sendInterrupt(0);
}

void Wifi::transmitPacket(PacketType type) {
    // Set the packet address and size, and modify the TX header
    uint16_t address, size;
    if (type == CMD_ACK) {
        // Set the size for a generated CMD ack
        address = 0;
        size = 40;
    }
    else {
        if (type == CMD_REPLY) {
            if (wTxbufReply1 & BIT(15)) {
                // Increment the retry count and swap the CMD reply address if set
                address = (wTxbufReply1 & 0xFFF) << 1;
                uint8_t value = core->memory.read<uint8_t>(1, 0x4804004 + address) + 1;
                if (value) core->memory.write<uint8_t>(1, 0x4804004 + address, value);
                wTxbufReply2 = wTxbufReply1;
                wTxbufReply1 = 0;
            }
            else {
                // Set the size for a generated empty CMD reply
                address = 0;
                size = 36;
                goto start;
            }
        }
        else {
            // Set the address for a regular packet
            address = (wTxbufLoc[type] & 0xFFF) << 1;
        }

        // Get the packet size and update the TX header in memory
        size = (core->memory.read<uint16_t>(1, 0x480400A + address) + 11) & ~0x3;
        core->memory.write<uint16_t>(1, 0x4804000 + address, 0x0001);
        core->memory.write<uint16_t>(1, 0x4804002 + address, 0x0000);
        core->memory.write<uint8_t>(1, 0x4804005 + address, 0x00);
    }

start:
    // Start transmitting a packet
    LOG_INFO("Instance %d sending packet of type %d with size 0x%X\n", core->id, type, size);
    sendInterrupt(7);
    mutex.lock();

    for (uint32_t i = 0; i < connections.size(); i++) {
        // Create packet data and fill out the RX header
        const uint16_t fts[] = { 0x8010, 0x801C, 0x8010, 0x8010, 0x8011, 0x801E, 0x801D };
        uint16_t *data = new uint16_t[size / 2];
        data[0] = fts[type]; // Frame type
        data[1] = 0x0040; // Something?
        data[2] = 0x0000; // Nothing
        data[3] = 0x0010; // Transfer rate
        data[4] = size - 12; // Data length
        data[5] = 0x00FF; // Signal strength

        if (type == CMD_ACK) {
            // Fill out the IEEE header and body for a CMD ack
            data[6] = 0x0218; // Frame control
            data[7] = 0x7FFF; // Duration
            data[8] = 0x0903; // Address 1
            data[9] = 0x00BF; // Address 1
            data[10] = 0x0003; // Address 1
            data[11] = wMacaddr[0]; // Address 2
            data[12] = wMacaddr[1]; // Address 2
            data[13] = wMacaddr[2]; // Address 2
            data[14] = wMacaddr[0]; // Address 3
            data[15] = wMacaddr[1]; // Address 3
            data[16] = wMacaddr[2]; // Address 3
            data[17] = 0x0000; // Sequence control
            data[18] = 0x0046; // Something?
            data[19] = 0x0000; // Error flags
        }
        else if (type == CMD_REPLY && !(wTxbufReply1 & BIT(15))) {
            // Fill out the IEEE header for an empty CMD reply
            data[6] = 0x0158; // Frame control
            data[7] = 0x7FFF; // Duration
            data[8] = 0x0903; // Address 1
            data[9] = 0x00BF; // Address 1
            data[10] = 0x0010; // Address 1
            data[11] = wMacaddr[0]; // Address 2
            data[12] = wMacaddr[1]; // Address 2
            data[13] = wMacaddr[2]; // Address 2
            data[14] = wMacaddr[0]; // Address 3
            data[15] = wMacaddr[1]; // Address 3
            data[16] = wMacaddr[2]; // Address 3
            data[17] = 0x0000; // Sequence control
        }
        else {
            // Read the rest of the packet from memory
            for (uint32_t j = 12; j < size; j += 2)
                data[j / 2] = core->memory.read<uint16_t>(1, 0x4804000 + address + j);
        }

        // Set and update the IEEE sequence number if enabled
        if (type >= BEACON_FRAME || !(wTxbufLoc[type] & BIT(13)))
            data[17] = (wTxSeqno++) << 4;

        // Add the packet to the queue
        connections[i]->mutex.lock();
        connections[i]->packets.push_back(data);
        connections[i]->mutex.unlock();
    }

    // Finish transmitting a packet
    mutex.unlock();
    sendInterrupt(1);

    // Handle special end events for certain packets
    if (type < BEACON_FRAME)
        wTxbufLoc[type] &= ~BIT(15);
    else if (type == CMD_ACK)
        sendInterrupt(12);

    // Update transmission status based on type and certain bits
    switch (type) {
        case LOC1_FRAME: wTxstat = (wTxbufLoc[0] & BIT(12)) ? 0x0701 : 0x0001; break;
        case CMD_FRAME: wTxstat = (wTxstatCnt & BIT(14)) ? 0x0801 : 0x0001; break;
        case LOC2_FRAME: wTxstat = (wTxbufLoc[2] & BIT(12)) ? 0x1701 : 0x1001; break;
        case LOC3_FRAME: wTxstat = (wTxbufLoc[3] & BIT(12)) ? 0x2701 : 0x2001; break;
        case BEACON_FRAME: wTxstat = (wTxstatCnt & BIT(15)) ? 0x0301 : 0x0001; break;
        case CMD_REPLY: wTxstat = (wTxstatCnt & BIT(12)) ? 0x0401 : 0x0001; break;
        case CMD_ACK: wTxstat = (wTxstatCnt & BIT(13)) ? 0x0B01 : 0x0001; break;
    }
}

void Wifi::writeWModeWep(uint16_t mask, uint16_t value) {
    // Write to the W_MODE_WEP register
    wModeWep = (wModeWep & ~mask) | (value & mask);
}

void Wifi::writeWTxstatCnt(uint16_t mask, uint16_t value) {
    // Write to the W_TXSTAT_CNT register
    mask &= 0xF000;
    wTxstatCnt = (wTxstatCnt & ~mask) | (value & mask);
}

void Wifi::writeWIrf(uint16_t mask, uint16_t value) {
    // Write to the W_IF register
    // Setting a bit actually clears it to acknowledge an interrupt
    wIrf &= ~(value & mask);
}

void Wifi::writeWIe(uint16_t mask, uint16_t value) {
    // Trigger a WiFi interrupt if W_IF & W_IE changes from zero
    if (!(wIe & wIrf) && (value & mask & wIrf))
        core->interpreter[1].sendInterrupt(24);

    // Write to the W_IE register
    mask &= 0xFBFF;
    wIe = (wIe & ~mask) | (value & mask);
}

void Wifi::writeWMacaddr(int index, uint16_t mask, uint16_t value) {
    // Write to one of the W_MACADDR registers
    wMacaddr[index] = (wMacaddr[index] & ~mask) | (value & mask);
}

void Wifi::writeWBssid(int index, uint16_t mask, uint16_t value) {
    // Write to one of the W_BSSID registers
    wBssid[index] = (wBssid[index] & ~mask) | (value & mask);
}

void Wifi::writeWAidFull(uint16_t mask, uint16_t value) {
    // Write to the W_AID_FULL register
    mask &= 0x07FF;
    wAidFull = (wAidFull & ~mask) | (value & mask);
}

void Wifi::writeWRxcnt(uint16_t mask, uint16_t value) {
    // Write to the W_RXCNT register
    mask &= 0xFF0E;
    wRxcnt = (wRxcnt & ~mask) | (value & mask);

    // Latch W_RXBUF_WR_ADDR to W_RXBUF_WRCSR
    if (value & BIT(0))
        wRxbufWrcsr = wRxbufWrAddr << 1;
}

void Wifi::writeWPowerstate(uint16_t mask, uint16_t value) {
    // Write to the W_POWERSTATE register
    mask &= 0x0003;
    wPowerstate = (wPowerstate & ~mask) | (value & mask);

    // Set the power state to enabled if requested
    if (wPowerstate & BIT(1))
        wPowerstate &= ~BIT(9);
}

void Wifi::writeWPowerforce(uint16_t mask, uint16_t value) {
    // Write to the W_POWERFORCE register
    mask &= 0x8001;
    wPowerforce = (wPowerforce & ~mask) | (value & mask);

    // Force set the power state if requested
    if (wPowerforce & BIT(15))
        wPowerstate = (wPowerstate & ~BIT(9)) | ((wPowerforce & BIT(0)) << 9);
}

void Wifi::writeWRxbufBegin(uint16_t mask, uint16_t value) {
    // Write to the W_RXBUF_BEGIN register
    wRxbufBegin = (wRxbufBegin & ~mask) | (value & mask);
}

void Wifi::writeWRxbufEnd(uint16_t mask, uint16_t value) {
    // Write to the W_RXBUF_END register
    wRxbufEnd = (wRxbufEnd & ~mask) | (value & mask);
}

void Wifi::writeWRxbufWrAddr(uint16_t mask, uint16_t value) {
    // Write to the W_RXBUF_WR_ADDR register
    mask &= 0x0FFF;
    wRxbufWrAddr = (wRxbufWrAddr & ~mask) | (value & mask);
}

void Wifi::writeWRxbufRdAddr(uint16_t mask, uint16_t value) {
    // Write to the W_RXBUF_RD_ADDR register
    mask &= 0x1FFE;
    wRxbufRdAddr = (wRxbufRdAddr & ~mask) | (value & mask);
}

void Wifi::writeWRxbufReadcsr(uint16_t mask, uint16_t value) {
    // Write to the W_RXBUF_READCSR register
    mask &= 0x0FFF;
    wRxbufReadcsr = (wRxbufReadcsr & ~mask) | (value & mask);
}

void Wifi::writeWRxbufGap(uint16_t mask, uint16_t value) {
    // Write to the W_RXBUF_GAP register
    mask &= 0x1FFE;
    wRxbufGap = (wRxbufGap & ~mask) | (value & mask);
}

void Wifi::writeWRxbufGapdisp(uint16_t mask, uint16_t value) {
    // Write to the W_RXBUF_GAPDISP register
    mask &= 0x0FFF;
    wRxbufGapdisp = (wRxbufGapdisp & ~mask) | (value & mask);
}

void Wifi::writeWRxbufCount(uint16_t mask, uint16_t value) {
    // Write to the W_RXBUF_COUNT register
    mask &= 0x0FFF;
    wRxbufCount = (wRxbufCount & ~mask) | (value & mask);
}

void Wifi::writeWTxbufWrAddr(uint16_t mask, uint16_t value) {
    // Write to the W_TXBUF_WR_ADDR register
    mask &= 0x1FFE;
    wTxbufWrAddr = (wTxbufWrAddr & ~mask) | (value & mask);
}

void Wifi::writeWTxbufCount(uint16_t mask, uint16_t value) {
    // Write to the W_TXBUF_COUNT register
    mask &= 0x0FFF;
    wTxbufCount = (wTxbufCount & ~mask) | (value & mask);
}

void Wifi::writeWTxbufWrData(uint16_t mask, uint16_t value) {
    // Write a value to WiFi RAM
    core->memory.write<uint16_t>(1, 0x4804000 + wTxbufWrAddr, value & mask);

    // Increment the write address
    wTxbufWrAddr += 2;
    if (wTxbufWrAddr == wTxbufGap)
        wTxbufWrAddr += wTxbufGapdisp << 1;
    wTxbufWrAddr &= 0x1FFF;

    // Decrement the write counter and trigger an interrupt at the end
    if (wTxbufCount > 0 && --wTxbufCount == 0)
        sendInterrupt(8);
}

void Wifi::writeWTxbufGap(uint16_t mask, uint16_t value) {
    // Write to the W_TXBUF_GAP register
    mask &= 0x1FFE;
    wTxbufGap = (wTxbufGap & ~mask) | (value & mask);
}

void Wifi::writeWTxbufGapdisp(uint16_t mask, uint16_t value) {
    // Write to the W_TXBUF_GAPDISP register
    mask &= 0x0FFF;
    wTxbufGapdisp = (wTxbufGapdisp & ~mask) | (value & mask);
}

void Wifi::writeWTxbufLoc(PacketType type, uint16_t mask, uint16_t value) {
    // Write to one of the W_TXBUF_[BEACON,CMD,LOC1,LOC2,LOC3] registers
    wTxbufLoc[type] = (wTxbufLoc[type] & ~mask) | (value & mask);

    // Send a packet to connected cores if triggered for non-beacons
    if (type != BEACON_FRAME && (wTxbufLoc[type] & BIT(15)) && (wTxreqRead & BIT(type)))
        transmitPacket(type);
}

void Wifi::writeWBeaconInt(uint16_t mask, uint16_t value) {
    // Write to the W_BEACON_INT register
    mask &= 0x03FF;
    wBeaconInt = (wBeaconInt & ~mask) | (value & mask);

    // Reload the beacon millisecond counter
    wBeaconCount = wBeaconInt;
}

void Wifi::writeWTxbufReply1(uint16_t mask, uint16_t value) {
    // Write to the W_TXBUF_REPLY1 register
    wTxbufReply1 = (wTxbufReply1 & ~mask) | (value & mask);
}

void Wifi::writeWTxreqReset(uint16_t mask, uint16_t value) {
    // Clear bits in W_TXREQ_READ
    mask &= 0x000F;
    wTxreqRead &= ~(value & mask);
}

void Wifi::writeWTxreqSet(uint16_t mask, uint16_t value) {
    // Set bits in W_TXREQ_READ
    mask &= 0x000F;
    wTxreqRead |= (value & mask);

    // Send a packet to connected cores if triggered for non-beacons
    for (int i = 0; i < 4; i++)
        if ((wTxbufLoc[i] & BIT(15)) && (wTxreqRead & BIT(i)))
            transmitPacket(PacketType(i));
}

void Wifi::writeWUsCountcnt(uint16_t mask, uint16_t value) {
    // Write to the W_US_COUNTCNT register
    mask &= 0x0001;
    wUsCountcnt = (wUsCountcnt & ~mask) | (value & mask);
}

void Wifi::writeWUsComparecnt(uint16_t mask, uint16_t value) {
    // Write to the W_US_COMPARECNT register
    mask &= 0x0001;
    wUsComparecnt = (wUsComparecnt & ~mask) | (value & mask);

    // Trigger an immediate beacon interrupt if requested
    if (value & BIT(1))
        sendInterrupt(14);
}

void Wifi::writeWCmdCountcnt(uint16_t mask, uint16_t value) {
    // Write to the W_CMD_COUNTCNT register
    mask &= 0x0001;
    wCmdCountcnt = (wCmdCountcnt & ~mask) | (value & mask);
}

void Wifi::writeWUsCompare(int index, uint16_t mask, uint16_t value) {
    // Write to part of the W_US_COMPARE register
    int shift = index * 16;
    mask &= (index ? 0xFFFF : 0xFC00);
    wUsCompare = (wUsCompare & ~(uint64_t(mask) << shift)) | (uint64_t(value & mask) << shift);
}

void Wifi::writeWUsCount(int index, uint16_t mask, uint16_t value) {
    // Write to part of the W_US_COUNT register
    int shift = index * 16;
    wUsCount = (wUsCount & ~(uint64_t(mask) << shift)) | (uint64_t(value & mask) << shift);
}

void Wifi::writeWPreBeacon(uint16_t mask, uint16_t value) {
    // Write to the W_PRE_BEACON register
    wPreBeacon = (wPreBeacon & ~mask) | (value & mask);
}

void Wifi::writeWCmdCount(uint16_t mask, uint16_t value) {
    // Write to the W_CMD_COUNT register
    wCmdCount = (wCmdCount & ~mask) | (value & mask);
}

void Wifi::writeWBeaconCount(uint16_t mask, uint16_t value) {
    // Write to the W_BEACON_COUNT register
    wBeaconCount = (wBeaconCount & ~mask) | (value & mask);
}

void Wifi::writeWConfig(int index, uint16_t mask, uint16_t value) {
    const uint16_t masks[] = {
        0x81FF, 0xFFFF, 0xFFFF, 0xFFFF, 0x0FFF,
        0x8FFF, 0xFFFF, 0xFFFF, 0x00FF, 0x00FF,
        0x00FF, 0x00FF, 0xFFFF, 0xFF3F, 0x7A7F
    };

    // Write to one of the W_CONFIG registers
    mask &= masks[index];
    wConfig[index] = (wConfig[index] & ~mask) | (value & mask);
}

void Wifi::writeWPostBeacon(uint16_t mask, uint16_t value) {
    // Write to the W_POST_BEACON register
    wPostBeacon = (wPostBeacon & ~mask) | (value & mask);
}

void Wifi::writeWBbCnt(uint16_t mask, uint16_t value) {
    // Perform a BB register transfer
    int index = value & 0xFF;
    switch ((value & 0xF000) >> 12) {
    case 5: // Write
        if ((index >= 0x01 && index <= 0x0C) || (index >= 0x13 && index <= 0x15) || (index >= 0x1B && index <= 0x26) ||
            (index >= 0x28 && index <= 0x4C) || (index >= 0x4E && index <= 0x5C) || (index >= 0x62 && index <= 0x63) ||
            (index == 0x65) || (index >= 0x67 && index <= 0x68)) // Writable registers
            bbRegisters[index] = wBbWrite;
        break;

    case 6: // Read
        wBbRead = bbRegisters[index];
        break;
    }
}

void Wifi::writeWBbWrite(uint16_t mask, uint16_t value) {
    // Write to the W_BB_WRITE register
    wBbWrite = (wBbWrite & ~mask) | (value & mask);
}

void Wifi::writeWIrfSet(uint16_t mask, uint16_t value) {
    // Trigger a WiFi interrupt if W_IF & W_IE changes from zero
    if (!(wIe & wIrf) && (wIe & value & mask))
        core->interpreter[1].sendInterrupt(24);

    // Set bits in the W_IF register
    mask &= 0xFBFF;
    wIrf |= (value & mask);
}

uint16_t Wifi::readWRxbufRdData() {
    // Read a value from WiFi RAM
    uint16_t value = core->memory.read<uint16_t>(1, 0x4804000 + wRxbufRdAddr);

    // Increment the read address
    if ((wRxbufRdAddr += 2) == wRxbufGap)
        wRxbufRdAddr += wRxbufGapdisp << 1;
    if (int bufSize = (wRxbufEnd & 0x1FFE) - (wRxbufBegin & 0x1FFE))
        wRxbufRdAddr = ((wRxbufBegin & 0x1FFE) + (wRxbufRdAddr - (wRxbufBegin & 0x1FFE)) % bufSize) & 0x1FFE;

    // Decrement the read counter and trigger an interrupt at the end
    if (wRxbufCount > 0 && --wRxbufCount == 0)
        sendInterrupt(9);
    return value;
}
