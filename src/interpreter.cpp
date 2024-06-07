/*
    Copyright 2019-2024 Hydr8gon

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

#include "interpreter.h"
#include "core.h"

Interpreter::Interpreter(Core *core, bool arm7): core(core), arm7(arm7)
{
    // Initialize the registers for user mode
    for (int i = 0; i < 32; i++)
        registers[i] = &registersUsr[i & 0xF];
}

void Interpreter::saveState(FILE *file)
{
    // Write state data to the file
    fwrite(pipeline, 4, sizeof(pipeline) / 4, file);
    fwrite(registersUsr, 4, sizeof(registersUsr) / 4, file);
    fwrite(registersFiq, 4, sizeof(registersFiq) / 4, file);
    fwrite(registersSvc, 4, sizeof(registersSvc) / 4, file);
    fwrite(registersAbt, 4, sizeof(registersAbt) / 4, file);
    fwrite(registersIrq, 4, sizeof(registersIrq) / 4, file);
    fwrite(registersUnd, 4, sizeof(registersUnd) / 4, file);
    fwrite(&cpsr, sizeof(cpsr), 1, file);
    fwrite(&spsrFiq, sizeof(spsrFiq), 1, file);
    fwrite(&spsrSvc, sizeof(spsrSvc), 1, file);
    fwrite(&spsrAbt, sizeof(spsrAbt), 1, file);
    fwrite(&spsrIrq, sizeof(spsrIrq), 1, file);
    fwrite(&spsrUnd, sizeof(spsrUnd), 1, file);
    fwrite(&halted, sizeof(halted), 1, file);
    fwrite(&cycles, sizeof(cycles), 1, file);
    fwrite(&ime, sizeof(ime), 1, file);
    fwrite(&ie, sizeof(ie), 1, file);
    fwrite(&irf, sizeof(irf), 1, file);
    fwrite(&postFlg, sizeof(postFlg), 1, file);
}

void Interpreter::loadState(FILE *file)
{
    // Read state data from the file
    fread(pipeline, 4, sizeof(pipeline) / 4, file);
    fread(registersUsr, 4, sizeof(registersUsr) / 4, file);
    fread(registersFiq, 4, sizeof(registersFiq) / 4, file);
    fread(registersSvc, 4, sizeof(registersSvc) / 4, file);
    fread(registersAbt, 4, sizeof(registersAbt) / 4, file);
    fread(registersIrq, 4, sizeof(registersIrq) / 4, file);
    fread(registersUnd, 4, sizeof(registersUnd) / 4, file);
    fread(&cpsr, sizeof(cpsr), 1, file);
    fread(&spsrFiq, sizeof(spsrFiq), 1, file);
    fread(&spsrSvc, sizeof(spsrSvc), 1, file);
    fread(&spsrAbt, sizeof(spsrAbt), 1, file);
    fread(&spsrIrq, sizeof(spsrIrq), 1, file);
    fread(&spsrUnd, sizeof(spsrUnd), 1, file);
    fread(&halted, sizeof(halted), 1, file);
    fread(&cycles, sizeof(cycles), 1, file);
    fread(&ime, sizeof(ime), 1, file);
    fread(&ie, sizeof(ie), 1, file);
    fread(&irf, sizeof(irf), 1, file);
    fread(&postFlg, sizeof(postFlg), 1, file);

    // Update mapped registers
    swapRegisters(cpsr);
}

void Interpreter::init()
{
    // Prepare to boot the BIOS
    setCpsr(0x000000D3); // Supervisor, interrupts off
    registersUsr[15] = arm7 ? 0x00000000 : 0xFFFF0000;
    flushPipeline();

    // Reset the I/O registers
    ime = 0;
    ie = irf = 0;
    postFlg = 0;
}

void Interpreter::directBoot()
{
    // Prepare to directly boot a ROM
    setCpsr(0x000000DF); // System, interrupts off
    registersUsr[15] = core->gbaMode ? 0x8000000 : core->memory.read<uint32_t>(arm7, 0x27FFE24 + (arm7 << 4));
    registersUsr[14] = registersUsr[15];
    registersUsr[12] = registersUsr[15];
    registersUsr[13] = arm7 ? (core->gbaMode ? 0x3007F00 : 0x380FD80) : 0x3002F7C;
    registersIrq[0] = arm7 ? (core->gbaMode ? 0x3007FA0 : 0x380FF80) : 0x3003F80;
    registersSvc[0] = arm7 ? (core->gbaMode ? 0x3007FE0 : 0x380FFC0) : 0x3003FC0;
    flushPipeline();
}

void Interpreter::resetCycles()
{
    // Adjust CPU cycles for a global cycle reset
    cycles -= std::min(core->globalCycles, cycles);
}

void Interpreter::runNdsFrame(Core &core)
{
    Interpreter &arm9 = core.interpreter[0];
    Interpreter &arm7 = core.interpreter[1];

    // Run a frame in NDS mode
    while (core.running.exchange(true))
    {
        // Run the CPUs until the next scheduled task
        while (core.events[0].cycles > core.globalCycles)
        {
            // Run the ARM9
            if (!arm9.halted && core.globalCycles >= arm9.cycles)
                arm9.cycles = core.globalCycles + arm9.runOpcode();

            // Run the ARM7 at half the speed of the ARM9
            if (!arm7.halted && core.globalCycles >= arm7.cycles)
                arm7.cycles = core.globalCycles + (arm7.runOpcode() << 1);

            // Count cycles up to the next soonest event
            core.globalCycles = std::min<uint32_t>((arm9.halted ? -1 : arm9.cycles), (arm7.halted ? -1 : arm7.cycles));
        }

        // Jump to the next scheduled task
        core.globalCycles = core.events[0].cycles;

        // Run all tasks that are scheduled now
        while (core.events[0].cycles <= core.globalCycles)
        {
            core.tasks[core.events[0].task]();
            core.events.erase(core.events.begin());
        }
    }
}

void Interpreter::runGbaFrame(Core &core)
{
    Interpreter &arm7 = core.interpreter[1];

    // Run a frame in GBA mode
    while (core.running.exchange(true))
    {
        // Run the ARM7 until the next scheduled task
        if (arm7.cycles > core.globalCycles) core.globalCycles = arm7.cycles;
        while (!arm7.halted && core.events[0].cycles > arm7.cycles)
            arm7.cycles = (core.globalCycles += arm7.runOpcode());

        // Jump to the next scheduled task
        core.globalCycles = core.events[0].cycles;

        // Run all tasks that are scheduled now
        while (core.events[0].cycles <= core.globalCycles)
        {
            core.tasks[core.events[0].task]();
            core.events.erase(core.events.begin());
        }
    }
}

FORCE_INLINE int Interpreter::runOpcode()
{
    // Push the next opcode through the pipeline
    uint32_t opcode = pipeline[0];
    pipeline[0] = pipeline[1];

    // Execute an instruction
    if (cpsr & BIT(5)) // THUMB mode
    {
        // Fill the pipeline, incrementing the program counter
        pipeline[1] = core->memory.read<uint16_t>(arm7, *registers[15] += 2);

        // Execute a THUMB instruction
        return (this->*thumbInstrs[(opcode >> 6) & 0x3FF])(opcode);
    }
    else // ARM mode
    {
        // Fill the pipeline, incrementing the program counter
        pipeline[1] = core->memory.read<uint32_t>(arm7, *registers[15] += 4);

        // Execute an ARM instruction based on its condition
        switch (condition[((opcode >> 24) & 0xF0) | (cpsr >> 28)])
        {
            case 0:  return 1;                      // False
            case 2:  return handleReserved(opcode); // Reserved
            default: return (this->*armInstrs[((opcode >> 16) & 0xFF0) | ((opcode >> 4) & 0xF)])(opcode);
        }
    }
}

void Interpreter::sendInterrupt(int bit)
{
    // Set the interrupt's request bit
    irf |= BIT(bit);

    // Trigger an interrupt if the conditions are met, or unhalt the CPU even if interrupts are disabled
    // The ARM9 additionally needs IME to be set for it to unhalt, but the ARM7 doesn't care
    if (ie & irf)
    {
        if (ime && !(cpsr & BIT(7)))
            core->schedule(SchedTask(ARM9_INTERRUPT + arm7), (arm7 && !core->gbaMode) + 1);
        else if (ime || arm7)
            halted &= ~BIT(0);
    }
}

void Interpreter::interrupt()
{
    // Trigger an interrupt and unhalt the CPU if the conditions still hold
    if (ime && (ie & irf) && !(cpsr & BIT(7)))
    {
        exception(0x18);
        halted &= ~BIT(0);
    }
}

int Interpreter::exception(uint8_t vector)
{
    // Forward the call to HLE BIOS if enabled, unless on ARM9 with the exception address changed
    if (bios && (arm7 || core->cp15.exceptionAddr))
        return bios->execute(vector, registers);

    // Switch the CPU mode, save the return address, and jump to the exception vector
    static const uint8_t modes[] = { 0x13, 0x1B, 0x13, 0x17, 0x17, 0x13, 0x12, 0x11 };
    setCpsr((cpsr & ~0x3F) | BIT(7) | modes[vector >> 2], true); // ARM, interrupts off, new mode
    *registers[14] = *registers[15] + ((*spsr & BIT(5)) >> 4);
    *registers[15] = (arm7 ? 0 : core->cp15.exceptionAddr) + vector;
    flushPipeline();
    return 3;
}

void Interpreter::flushPipeline()
{
    // Adjust the program counter and refill the pipeline after a jump
    if (cpsr & BIT(5)) // THUMB mode
    {
        *registers[15] = (*registers[15] & ~0x1) + 2;
        pipeline[0] = core->memory.read<uint16_t>(arm7, *registers[15] - 2);
        pipeline[1] = core->memory.read<uint16_t>(arm7, *registers[15]);
    }
    else // ARM mode
    {
        *registers[15] = (*registers[15] & ~0x3) + 4;
        pipeline[0] = core->memory.read<uint32_t>(arm7, *registers[15] - 4);
        pipeline[1] = core->memory.read<uint32_t>(arm7, *registers[15]);
    }
}

void Interpreter::swapRegisters(uint32_t value)
{
    // Swap banked registers based on a CPU mode value
    switch (value & 0x1F)
    {
        case 0x10: // User
        case 0x1F: // System
            registers[8] = &registersUsr[8];
            registers[9] = &registersUsr[9];
            registers[10] = &registersUsr[10];
            registers[11] = &registersUsr[11];
            registers[12] = &registersUsr[12];
            registers[13] = &registersUsr[13];
            registers[14] = &registersUsr[14];
            spsr = nullptr;
            break;

        case 0x11: // FIQ
            registers[8] = &registersFiq[0];
            registers[9] = &registersFiq[1];
            registers[10] = &registersFiq[2];
            registers[11] = &registersFiq[3];
            registers[12] = &registersFiq[4];
            registers[13] = &registersFiq[5];
            registers[14] = &registersFiq[6];
            spsr = &spsrFiq;
            break;

        case 0x12: // IRQ
            registers[8] = &registersUsr[8];
            registers[9] = &registersUsr[9];
            registers[10] = &registersUsr[10];
            registers[11] = &registersUsr[11];
            registers[12] = &registersUsr[12];
            registers[13] = &registersIrq[0];
            registers[14] = &registersIrq[1];
            spsr = &spsrIrq;
            break;

        case 0x13: // Supervisor
            registers[8] = &registersUsr[8];
            registers[9] = &registersUsr[9];
            registers[10] = &registersUsr[10];
            registers[11] = &registersUsr[11];
            registers[12] = &registersUsr[12];
            registers[13] = &registersSvc[0];
            registers[14] = &registersSvc[1];
            spsr = &spsrSvc;
            break;

        case 0x17: // Abort
            registers[8] = &registersUsr[8];
            registers[9] = &registersUsr[9];
            registers[10] = &registersUsr[10];
            registers[11] = &registersUsr[11];
            registers[12] = &registersUsr[12];
            registers[13] = &registersAbt[0];
            registers[14] = &registersAbt[1];
            spsr = &spsrAbt;
            break;

        case 0x1B: // Undefined
            registers[8] = &registersUsr[8];
            registers[9] = &registersUsr[9];
            registers[10] = &registersUsr[10];
            registers[11] = &registersUsr[11];
            registers[12] = &registersUsr[12];
            registers[13] = &registersUnd[0];
            registers[14] = &registersUnd[1];
            spsr = &spsrUnd;
            break;

        default:
            LOG("Unknown ARM%d CPU mode: 0x%X\n", arm7 ? 7 : 9, value & 0x1F);
            break;
    }
}

void Interpreter::setCpsr(uint32_t value, bool save)
{
    // Update registers if the CPU mode changed
    if ((value & 0x1F) != (cpsr & 0x1F))
        swapRegisters(value);

    // Set the CPSR, saving the old value if requested
    if (save && spsr) *spsr = cpsr;
    cpsr = value;

    // Trigger an interrupt if the conditions are met
    if (ime && (ie & irf) && !(cpsr & BIT(7)))
        core->schedule(SchedTask(ARM9_INTERRUPT + arm7), (arm7 && !core->gbaMode) + 1);
}

int Interpreter::handleReserved(uint32_t opcode)
{
    // The ARM9-exclusive BLX instruction uses the reserved condition code, so let it run
    if ((opcode & 0xE000000) == 0xA000000)
        return blx(opcode); // BLX label

    // If the special HLE BIOS opcode was jumped to, return from an HLE interrupt
    if (bios && opcode == 0xFF000000)
        return finishHleIrq();

    // If a DLDI function was jumped to, HLE it and return
    if (core->dldi.isPatched())
    {
        uint32_t **r = registers;
        switch (opcode)
        {
            case DLDI_START:  *r[0] = core->dldi.startup();                               break;
            case DLDI_INSERT: *r[0] = core->dldi.isInserted();                            break;
            case DLDI_READ:   *r[0] = core->dldi.readSectors(arm7, *r[0], *r[1], *r[2]);  break;
            case DLDI_WRITE:  *r[0] = core->dldi.writeSectors(arm7, *r[0], *r[1], *r[2]); break;
            case DLDI_CLEAR:  *r[0] = core->dldi.clearStatus();                           break;
            case DLDI_STOP:   *r[0] = core->dldi.shutdown();                              break;
        }
        return bx(14);
    }

    // Treat anything else as an unknown opcode
    return unkArm(opcode);
}

int Interpreter::handleHleIrq()
{
    // Switch to IRQ mode, save the return address, and push registers to the stack
    setCpsr((cpsr & ~0x3F) | BIT(7) | 0x12, true);
    *registers[14] = *registers[15] + ((*spsr & BIT(5)) ? 2 : 0);
    stmdbW((13 << 16) | BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(12) | BIT(14));

    // Set the return address to the special HLE BIOS opcode amd jump to the interrupt handler
    *registers[14] = arm7 ? 0x00000000 : 0xFFFF0000;
    *registers[15] = core->memory.read<uint32_t>(arm7, arm7 ? 0x3FFFFFC : (core->cp15.dtcmAddr + 0x3FFC));
    flushPipeline();
    return 3;
}

int Interpreter::finishHleIrq()
{
    // Update the wait flags if in the middle of an HLE IntrWait function
    if (bios->shouldCheck())
        bios->checkWaitFlags();

    // Pop registers from the stack, jump to the return address, and restore the mode
    ldmiaW((13 << 16) | BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(12) | BIT(14));
    *registers[15] = *registers[14] - 4;
    if (spsr) setCpsr(*spsr);
    flushPipeline();
    return 3;
}

int Interpreter::unkArm(uint32_t opcode)
{
    // Handle an unknown ARM opcode
    LOG("Unknown ARM%d ARM opcode: 0x%X\n", arm7 ? 7 : 9, opcode);
    return 1;
}

int Interpreter::unkThumb(uint16_t opcode)
{
    // Handle an unknown THUMB opcode
    LOG("Unknown ARM%d THUMB opcode: 0x%X\n", arm7 ? 7 : 9, opcode);
    return 1;
}

void Interpreter::writeIme(uint8_t value)
{
    // Write to the IME register
    ime = value & 0x01;

    // Trigger an interrupt if the conditions are met
    if (ime && (ie & irf) && !(cpsr & BIT(7)))
        core->schedule(SchedTask(ARM9_INTERRUPT + arm7), (arm7 && !core->gbaMode) + 1);
}

void Interpreter::writeIe(uint32_t mask, uint32_t value)
{
    // Write to the IE register
    mask &= (arm7 ? (core->gbaMode ? 0x3FFF : 0x01FF3FFF) : 0x003F3F7F);
    ie = (ie & ~mask) | (value & mask);

    // Trigger an interrupt if the conditions are met
    if (ime && (ie & irf) && !(cpsr & BIT(7)))
        core->schedule(SchedTask(ARM9_INTERRUPT + arm7), (arm7 && !core->gbaMode) + 1);
}

void Interpreter::writeIrf(uint32_t mask, uint32_t value)
{
    // Write to the IF register
    // Setting a bit actually clears it to acknowledge an interrupt
    irf &= ~(value & mask);
}

void Interpreter::writePostFlg(uint8_t value)
{
    // Write to the POSTFLG register
    // The first bit can be set, but never cleared
    // For some reason, the second bit is writable on the ARM9
    postFlg |= value & 0x01;
    if (!arm7) postFlg = (postFlg & ~0x02) | (value & 0x02);
}
