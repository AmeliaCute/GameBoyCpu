#include "../includes/cpu.h"
#include "../includes/log.h"

static void setFlags(CPU *cpu, uint8_t flags, int condition)
{
    if(condition)   cpu->f |= flags;  // Set flag
    else            cpu->f &= ~flags; // Clear flag
}

static uint8_t fetchByte(CPU *cpu, MMU *mmu)
{
    uint8_t byte = mmuReadByte(mmu, cpu->pc);
    cpu->pc++;
    return byte;
}

static int8_t fetchSignedByte(CPU *cpu, MMU *mmu)
{
    return (int8_t)fetchByte(cpu, mmu); // already increments PC
}

static uint16_t fetchWord(CPU *cpu, MMU *mmu)
{
    uint8_t low = fetchByte(cpu, mmu);
    uint8_t high = fetchByte(cpu, mmu);
    return (high << 8) | low;
}

void cpuReset(CPU *cpu)
{
    LOG("Resetting CPU...");
    cpu->af = 0x01B0; // AF register reset
    cpu->bc = 0x0013; // BC register reset
    cpu->de = 0x00D8; // DE register reset
    cpu->hl = 0x014D; // HL register reset
    cpu->sp = 0xFFFE; // Stack Pointer reset
    cpu->pc = 0x0100; // Program Counter reset
    cpu->ime = 0;     // Interrupt Master Enable reset
}

int cpuStep(CPU *cpu, MMU *mmu)
{
    uint16_t pcBefore = cpu->pc;
    uint8_t opcode = fetchByte(cpu, mmu);
    int cycles = 0;

    switch (opcode)
    {
        case 0x00: // NOP
        {
            cycles = 1;
            break;
        }
        case 0x01: // LD BC, d16
        {
            uint8_t value = fetchWord(cpu, mmu);
            cpu->b = value >> 8;
            cpu->c = value & 0xFF;
            cycles = 3;
            break;
        }
        case 0x02: // LD (BC), A
        {
            mmuWriteByte(mmu, cpu->bc, cpu->a);
            cycles = 2;
            break;
        }
        case 0x03: // INC BC
        {
            cpu->bc++;
            cycles = 2;
            break;
        }
        case 0x04: // INC B
        {
            uint8_t result = cpu->b + 1;
            cpu->f = ~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->b & 0x0F) == 0x0F);
            cpu->b = result;
            cycles = 1;
            break;
        }
        case 0x05: // DEC B
        {
            uint8_t result = cpu->b - 1;
            cpu->f = ~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->b & 0x0F) == 0x0F);
            cpu->b = result;
            cycles = 1;
            break;
        }
        case 0x06: // LD B, d8
        {
            cpu->b = fetchByte(cpu, mmu);
            cycles = 2;
            break;
        }
        case 0x07: // RLCA
        {
            uint8_t a = cpu->a;
            uint8_t bit7 = a >> 7 & 0x01;
            cpu->a = (a << 1) | bit7;
            cpu->f = 0;
            setFlags(cpu, FLAG_C, bit7);
            cycles = 1;
            break;
        }
        case 0x08: // LD (a16), SP
        {
            uint16_t address = fetchWord(cpu, mmu);
            uint16_t sp = cpu->sp;

            mmuWriteByte(mmu, address, sp & 0xFF);             // Write low byte
            mmuWriteByte(mmu, address + 1, (sp >> 8) && 0xFF); // Write high byte
            cycles = 5;
            break;
        }
        case 0x09: // ADD HL, BC
        {
            uint16_t hl = cpu->hl;
            uint16_t bc = cpu->bc;
            uint32_t result = hl + bc;

            cpu->f = ~(FLAG_N | FLAG_H | FLAG_C);
            if(((hl & 0x0FFF) + (bc & 0x0FFF)) > 0x0FFF)
                cpu->f |= FLAG_H; // Half carry
            if(result > 0xFFFF)
                cpu->f |= FLAG_C; // Carry

            cpu->hl = result & 0xFFFF; // Store result in HL
            cycles = 2;
            break;
        }
        case 0x0A: // LD A, (BC)
        {
            cpu->a = mmuReadByte(mmu, cpu->bc);
            cycles = 2;
            break;
        }
        case 0x0B: // DEC BC
        {
            cpu->bc--;
            cycles = 2;
            break;
        }
        case 0x0C: // INC C
        {
            uint8_t result = cpu->c + 1;
            cpu->f = ~(FLAG_Z | FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->c & 0x0F) == 0x0F);
            cpu->c = result;
            cycles = 1;
            break;
        }
        case 0x0D: // DEC C
        {
            uint8_t result = cpu->c - 1;
            cpu->f = ~(FLAG_Z | FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->c & 0x0F) == 0x0F);
            cpu->c = result;
            cycles = 1;
            break;
        }
        case 0x0E: // LD C, d8
        {
            cpu->c = fetchByte(cpu, mmu);
            cycles = 2;
            break;
        }
        case 0x0F: // RRCA
        {
            uint8_t a = cpu->a;
            uint8_t bit0 = a & 0x01;

            cpu->a = (a >> 1) | (bit0 << 7);
            cpu->f = 0;
            setFlags(cpu, FLAG_C, bit0);
            cycles = 1;
            break;
        }
        case 0x10:
        {
            LOG("Unimplemented opcode 0x%02X at PC: 0x%04X", opcode, pcBefore);
            cycles = -1; // Indicate unimplemented opcode
            break;
        }
        case 0x11: // LD DE, d16
        {
            int8_t value = fetchWord(cpu, mmu);
            cpu->d = value >> 8;
            cpu->e = value & 0xFF;
            cycles = 3;
            break;
        }
        case 0x12: // LD (DE), A
        {
            mmuWriteByte(mmu, cpu->de, cpu->a);
            cycles = 2;
            break;
        }
        case 0x13: // INC DE
        {
            cpu->de++;
            cycles = 2;
            break;
        }
        case 0x14: // INC D
        {
            uint8_t result = cpu->d + 1;
            cpu->f = ~(FLAG_Z | FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->d & 0x0F) == 0x0F);
            cpu->d = result;
            cycles = 1;
            break;
        }
        case 0x15: // DEC D
        {
            uint8_t result = cpu->d - 1;
            cpu->f = ~(FLAG_Z | FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->d & 0x0F) == 0x0F);
            cpu->d = result;
            cycles = 1;
            break;
        }
        case 0x16: // LD D, d8
        {
            cpu->d = fetchByte(cpu, mmu);
            cycles = 2;
            break;
        }
        case 0x17: // RLA
        {
            uint8_t carryIn = (cpu->f & FLAG_C) ? 1 : 0;
            uint8_t bit7 = (cpu->a & 0x80) >> 7;

            cpu->a = (cpu->a << 1) | carryIn;

            cpu->f &= ~(FLAG_Z | FLAG_N | FLAG_H); // Clear Z, N, H flags
            setFlags(cpu, FLAG_C, bit7);
            cycles = 1;
            break;
        }
        case 0x18: // JR s8
        {
            int8_t offset = fetchSignedByte(cpu, mmu);
            cpu->pc += offset;
            cycles = 3;
            break;
        }
    }

    return cycles;
}