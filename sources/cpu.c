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

static uint8_t popByte(CPU *cpu, MMU *mmu) 
{
    uint16_t addr = cpu->sp;
    cpu->sp++;
    return mmuReadByte(mmu, addr);
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
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->b & 0x0F) == 0x0F);
            cpu->b = result;
            cycles = 1;
            break;
        }
        case 0x05: // DEC B
        {
            uint8_t result = cpu->b - 1;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

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
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->c & 0x0F) == 0x0F);
            cpu->c = result;
            cycles = 1;
            break;
        }
        case 0x0D: // DEC C
        {
            uint8_t result = cpu->c - 1;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);
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
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->d & 0x0F) == 0x0F);
            cpu->d = result;
            cycles = 1;
            break;
        }
        case 0x15: // DEC D
        {
            uint8_t result = cpu->d - 1;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);
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
        case 0x19: // ADD HL, DE
        {
            uint16_t hl = cpu->hl;
            uint16_t de = cpu->de;
            uint32_t result = hl + de;

            cpu->f = ~(FLAG_N | FLAG_H | FLAG_C);
            if(((hl & 0x0FFF) + (de & 0x0FFF)) > 0x0FFF)
                cpu->f |= FLAG_H; // Half carry
            if(result > 0xFFFF)
                cpu->f |= FLAG_C; // Carry

            cpu->hl = result & 0xFFFF; // Store result in HL
            cycles = 2;
            break;
        }
        case 0x1A: // LD A, (DE)
        {
            cpu->a = mmuReadByte(mmu, cpu->de);
            cycles = 2;
            break;
        }
        case 0x1B: // DEC DE
        {
            cpu->de--;
            cycles = 2;
            break;
        }
        case 0x1C: // INC E
        {
            uint8_t result = cpu->e + 1;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->e & 0x0F) == 0x0F);
            cpu->e = result;
            cycles = 1;
            break;
        }
        case 0x1D: // DEC E
        {
            uint8_t result = cpu->e - 1;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->e & 0x0F) == 0x0F);
            cpu->e = result;
            cycles = 1;
            break;
        }
        case 0x1E: // LD E, d8
        {
            cpu->e = fetchByte(cpu, mmu);
            cycles = 2;
            break;
        }
        case 0x1F: // RRA
        {
            uint8_t carryIn = (cpu->f & FLAG_C) ? 1 : 0;
            uint8_t bit0 = cpu->a & 0x01;

            cpu->a = (carryIn << 7) | (cpu->a >> 1);
            
            setFlags(cpu, FLAG_C, bit0);
            setFlags(cpu, FLAG_N, 0);
            setFlags(cpu, FLAG_H, 0);
            cycles = 1;
            break;
        }
        case 0x20: // JR NZ, s8
        {
            int8_t offset = fetchSignedByte(cpu, mmu);
            if(!(cpu->f & FLAG_Z)) // If Z flag is not set
            {
                cpu->pc += offset;
                cycles = 3;
            }
            else
                cycles = 2; // No jump, just increment PC
            break;
        }
        case 0x21: // LD HL, d16
        {
            uint16_t value = fetchWord(cpu, mmu);
            cpu->h = value >> 8;
            cpu->l = value & 0xFF;
            cycles = 3;
            break;
        }
        case 0x22: // LD (HL+), A
        {
            mmuWriteByte(mmu, cpu->hl, cpu->a);
            cpu->hl++;
            cycles = 2;
            break;
        }
        case 0x23: // INC HL
        {
            cpu->hl++;
            cycles = 2;
            break;
        }
        case 0x24: // INC H
        {
            uint8_t result = cpu->h + 1;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->h & 0x0F) == 0x0F);
            cpu->h = result;
            cycles = 1;
            break;
        }
        case 0x25: // DEC H
        {
            uint8_t result = cpu->h - 1;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->h & 0x0F) == 0x0F);
            cpu->h = result;
            cycles = 1;
            break;
        }
        case 0x26: // LD H, d8
        {
            cpu->h = fetchByte(cpu, mmu);
            cycles = 2;
            break;
        }
        case 0x27: // DAA
        {
            uint8_t a = cpu->a;
            uint8_t carry = (cpu->f & FLAG_C) ? 1 : 0;
            uint8_t half = (cpu->f & FLAG_H) ? 1 : 0;
            uint8_t sub  = (cpu->f & FLAG_N) ? 1 : 0;

            if(!sub)
            {
                if(half || (a & 0x0F) > 9)
                    a += 0x06; // Adjust for half carry
                if(carry || a > 0x9F)
                {
                    a += 0x60;
                    carry = 1; // Adjust for carry
                }
                else
                    carry = 0; // Clear carry
            } else
            {
                if(half)
                {
                    a -= 0x06; // Adjust for half carry
                }
                if(carry)
                    a -= 0x60; // Adjust for carry                
            }

            cpu->a = a;

            setFlags(cpu, FLAG_Z, a == 0);
            setFlags(cpu, FLAG_N, sub);
            setFlags(cpu, FLAG_C, carry);
            cycles = 1;
            break;
        }
        case 0x28: // JR Z, s8
        {
            int8_t offset = fetchSignedByte(cpu, mmu);
            if(cpu->f & FLAG_Z) // If Z flag is set
            {
                cpu->pc += offset;
                cycles = 3;
            }
            else
                cycles = 2; // No jump, just increment PC
            break;
        }
        case 0x29: // ADD HL, HL
        {
            uint16_t hl = cpu->hl;
            uint32_t result = hl + hl;

            cpu->f = ~(FLAG_N | FLAG_H | FLAG_C);
            if(((hl & 0x0FFF) + (hl & 0x0FFF)) > 0x0FFF)
                cpu->f |= FLAG_H; // Half carry
            if(result > 0xFFFF)
                cpu->f |= FLAG_C; // Carry

            cpu->hl = result & 0xFFFF; // Store result in HL
            cycles = 2;
            break;
        }
        case 0x2A: // LD A, (HL+)
        {
            cpu->a = mmuReadByte(mmu, cpu->hl);
            cpu->hl++;
            cycles = 2;
            break;
        }
        case 0x2B: // DEC HL
        {
            cpu->hl--;
            cycles = 2;
            break;
        }
        case 0x2C: // INC L
        {
            uint8_t result = cpu->l + 1;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->l & 0x0F) == 0x0F);
            cpu->l = result;
            cycles = 1;
            break;
        }
        case 0x2D: // DEC L
        {
            uint8_t result = cpu->l - 1;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->l & 0x0F) == 0x0F);
            cpu->l = result;
            cycles = 1;
            break;
        }
        case 0x2E: // LD L, d8
        {
            cpu->l = fetchByte(cpu, mmu);
            cycles = 2;
            break;
        }
        case 0x2F: // CPL
        {
            cpu->a  = ~cpu->a;
            setFlags(cpu, FLAG_N, 1);
            setFlags(cpu, FLAG_H, 1);
            return 1;
        }
        case 0x30: // JR NC, s8
        {
            int8_t offset = fetchSignedByte(cpu, mmu);
            if(!(cpu->f & FLAG_C)) // If C flag is not set
            {
                cpu->pc += offset;
                cycles = 3;
            }
            else
                cycles = 2; // No jump, just increment PC
            break;
        }
        case 0x31: // LD SP, d16
        {
            uint16_t value = fetchWord(cpu, mmu);
            cpu->sp = value;
            cycles = 3;
            break;
        }
        case 0x32: // LD (HL-), A
        {
            mmuWriteByte(mmu, cpu->hl, cpu->a);
            cpu->hl--;
            cycles = 2;
            break;
        }
        case 0x33: // INC SP
        {
            cpu->sp++;
            cycles = 2;
            break;
        }
        case 0x34: // INC (HL)
        {
            uint8_t value = mmuReadByte(mmu, cpu->hl);
            uint8_t result = value + 1;
            mmuWriteByte(mmu, cpu->hl, result);

            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (value & 0x0F) == 0x0F);
            cycles = 3;
            break;
        }
        case 0x35: // DEC (HL)
        {
            uint8_t value = mmuReadByte(mmu, cpu->hl);
            uint8_t result = value - 1;
            mmuWriteByte(mmu, cpu->hl, result);

            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (value & 0x0F) == 0x0F);
            cycles = 3;
            break;
        }
        case 0x36: // LD (HL), d8
        {
            uint8_t value = fetchByte(cpu, mmu);
            mmuWriteByte(mmu, cpu->hl, value);
            cycles = 3;
            break;
        }
        case 0x37: // SCF
        {
            cpu->f &= ~(FLAG_Z | FLAG_N | FLAG_H); // Clear Z, N, H flags
            setFlags(cpu, FLAG_C, 1); // Set C flag
            cycles = 1;
            break;
        }
        case 0x38: // JR C, s8
        {
            int8_t offset = fetchSignedByte(cpu, mmu);
            if(cpu->f & FLAG_C) // If C flag is set
            {
                cpu->pc += offset;
                cycles = 3;
            }
            else
                cycles = 2; // No jump, just increment PC
            break;
        }
        case 0x39: // ADD HL, SP
        {
            uint16_t hl = cpu->hl;
            uint16_t sp = cpu->sp;
            uint32_t result = hl + sp;

            cpu->f = ~(FLAG_N | FLAG_H | FLAG_C);
            if(((hl & 0x0FFF) + (sp & 0x0FFF)) > 0x0FFF)
                cpu->f |= FLAG_H; // Half carry
            if(result > 0xFFFF)
                cpu->f |= FLAG_C; // Carry

            cpu->hl = result & 0xFFFF; // Store result in HL
            cycles = 2;
            break;
        }
        case 0x3A: // LD A, (HL-)
        {
            cpu->a = mmuReadByte(mmu, cpu->hl);
            cpu->hl--;
            cycles = 2;
            break;
        }
        case 0x3B: // DEC SP
        {
            cpu->sp--;
            cycles = 2;
            break;
        }
        case 0x3C: // INC A
        {
            uint8_t result = cpu->a + 1;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) == 0x0F);
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x3D: // DEC A
        {
            uint8_t result = cpu->a - 1;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) == 0x0F);
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x3E: // LD A, d8
        {
            cpu->a = fetchByte(cpu, mmu);
            cycles = 2;
            break;
        }
        case 0x3F: // CCF
        {
            uint8_t carry = (cpu->f & FLAG_C) ? 1 : 0;
            setFlags(cpu, FLAG_Z, !carry);

            setFlags(cpu, FLAG_N, 0);
            setFlags(cpu, FLAG_H, 0);

            cycles = 1;
            break;
        }
        case 0x40: // LD B, B
        {
            cpu->b = cpu->b; // No operation, just a copy
            cycles = 1;
            break;
        }
        case 0x41: // LD B, C
        {
            cpu->b = cpu->c;
            cycles = 1;
            break;
        }
        case 0x42: // LD B, D
        {
            cpu->b = cpu->d;
            cycles = 1;
            break;
        }
        case 0x43: // LD B, E
        {
            cpu->b = cpu->e;
            cycles = 1;
            break;
        }
        case 0x44: // LD B, H
        {
            cpu->b = cpu->h;
            cycles = 1;
            break;
        }
        case 0x45: // LD B, L
        {
            cpu->b = cpu->l;
            cycles = 1;
            break;
        }
        case 0x46: // LD B, (HL)
        {
            cpu->b = mmuReadByte(mmu, cpu->hl);
            cycles = 2;
            break;
        }
        case 0x47: // LD B, A
        {
            cpu->b = cpu->a;
            cycles = 1;
            break;
        }
        case 0x48: // LD C, B
        {
            cpu->c = cpu->b;
            cycles = 1;
            break;
        }
        case 0x49: // LD C, C
        {
            cpu->c = cpu->c; // No operation, just a copy
            cycles = 1;
            break;
        }
        case 0x4A: // LD C, D
        {
            cpu->c = cpu->d;
            cycles = 1;
            break;
        }
        case 0x4B: // LD C, E
        {
            cpu->c = cpu->e;
            cycles = 1;
            break;
        }
        case 0x4C: // LD C, H
        {
            cpu->c = cpu->h;
            cycles = 1;
            break;
        }
        case 0x4D: // LD C, L
        {
            cpu->c = cpu->l;
            cycles = 1;
            break;
        }
        case 0x4E: // LD C, (HL)
        {
            cpu->c = mmuReadByte(mmu, cpu->hl);
            cycles = 2;
            break;
        }
        case 0x4F: // LD C, A
        {
            cpu->c = cpu->a;
            cycles = 1;
            break;
        }
        case 0x50: // LD D, B
        {
            cpu->d = cpu->b;
            cycles = 1;
            break;
        }
        case 0x51: // LD D, C
        {
            cpu->d = cpu->c;
            cycles = 1;
            break;
        }
        case 0x52: // LD D, D
        {
            cpu->d = cpu->d; // No operation, just a copy
            cycles = 1;
            break;
        }
        case 0x53: // LD D, E
        {
            cpu->d = cpu->e;
            cycles = 1;
            break;
        }
        case 0x54: // LD D, H
        {
            cpu->d = cpu->h;
            cycles = 1;
            break;
        }
        case 0x55: // LD D, L
        {
            cpu->d = cpu->l;
            cycles = 1;
            break;
        }
        case 0x56: // LD D, (HL)
        {
            cpu->d = mmuReadByte(mmu, cpu->hl);
            cycles = 2;
            break;
        }
        case 0x57: // LD D, A
        {
            cpu->d = cpu->a;
            cycles = 1;
            break;
        }
        case 0x58: // LD E, B
        {
            cpu->e = cpu->b;
            cycles = 1;
            break;
        }
        case 0x59: // LD E, C
        {
            cpu->e = cpu->c;
            cycles = 1;
            break;
        }
        case 0x5A: // LD E, D
        {
            cpu->e = cpu->d;
            cycles = 1;
            break;
        }
        case 0x5B: // LD E, E
        {
            cpu->e = cpu->e; // No operation, just a copy
            cycles = 1;
            break;
        }
        case 0x5C: // LD E, H
        {
            cpu->e = cpu->h;
            cycles = 1;
            break;
        }
        case 0x5D: // LD E, L
        {
            cpu->e = cpu->l;
            cycles = 1;
            break;
        }
        case 0x5E: // LD E, (HL)
        {
            cpu->e = mmuReadByte(mmu, cpu->hl);
            cycles = 2;
            break;
        }
        case 0x5F: // LD E, A
        {
            cpu->e = cpu->a;
            cycles = 1;
            break;
        }
        case 0x60: // LD H, B
        {
            cpu->h = cpu->b;
            cycles = 1;
            break;
        }
        case 0x61: // LD H, C
        {
            cpu->h = cpu->c;
            cycles = 1;
            break;
        }
        case 0x62: // LD H, D
        {
            cpu->h = cpu->d;
            cycles = 1;
            break;
        }
        case 0x63: // LD H, E
        {
            cpu->h = cpu->e;
            cycles = 1;
            break;
        }
        case 0x64: // LD H, H
        {
            cpu->h = cpu->h; // No operation, just a copy
            cycles = 1;
            break;
        }
        case 0x65: // LD H, L
        {
            cpu->h = cpu->l;
            cycles = 1;
            break;
        }
        case 0x66: // LD H, (HL)
        {
            cpu->h = mmuReadByte(mmu, cpu->hl);
            cycles = 2;
            break;
        }
        case 0x67: // LD H, A
        {
            cpu->h = cpu->a;
            cycles = 1;
            break;
        }
        case 0x68: // LD L, B
        {
            cpu->l = cpu->b;
            cycles = 1;
            break;
        }
        case 0x69: // LD L, C
        {
            cpu->l = cpu->c;
            cycles = 1;
            break;
        }
        case 0x6A: // LD L, D
        {
            cpu->l = cpu->d;
            cycles = 1;
            break;
        }
        case 0x6B: // LD L, E
        {
            cpu->l = cpu->e;
            cycles = 1;
            break;
        }
        case 0x6C: // LD L, H
        {
            cpu->l = cpu->h;
            cycles = 1;
            break;
        }
        case 0x6D: // LD L, L
        {
            cpu->l = cpu->l; // No operation, just a copy
            cycles = 1;
            break;
        }
        case 0x6E: // LD L, (HL)
        {
            cpu->l = mmuReadByte(mmu, cpu->hl);
            cycles = 2;
            break;
        }
        case 0x6F: // LD L, A
        {
            cpu->l = cpu->a;
            cycles = 1;
            break;
        }
        case 0x70: // LD (HL), B
        {
            mmuWriteByte(mmu, cpu->hl, cpu->b);
            cycles = 2;
            break;
        }
        case 0x71: // LD (HL), C
        {
            mmuWriteByte(mmu, cpu->hl, cpu->c);
            cycles = 2;
            break;
        }
        case 0x72: // LD (HL), D
        {
            mmuWriteByte(mmu, cpu->hl, cpu->d);
            cycles = 2;
            break;
        }
        case 0x73: // LD (HL), E
        {
            mmuWriteByte(mmu, cpu->hl, cpu->e);
            cycles = 2;
            break;
        }
        case 0x74: // LD (HL), H
        {
            mmuWriteByte(mmu, cpu->hl, cpu->h);
            cycles = 2;
            break;
        }
        case 0x75: // LD (HL), L
        {
            mmuWriteByte(mmu, cpu->hl, cpu->l);
            cycles = 2;
            break;
        }
        case 0x76: // HALT
        {
            LOG("HALT instruction encountered at PC: 0x%04X", pcBefore);
            // HALT instruction does not change PC, just stops CPU execution
            return -1;
        }
        case 0x77: // LD (HL), A
        {
            mmuWriteByte(mmu, cpu->hl, cpu->a);
            cycles = 2;
            break;
        }
        case 0x78: // LD A, B
        {
            cpu->a = cpu->b;
            cycles = 1;
            break;
        }
        case 0x79: // LD A, C
        {
            cpu->a = cpu->c;
            cycles = 1;
            break;
        }
        case 0x7A: // LD A, D
        {
            cpu->a = cpu->d;
            cycles = 1;
            break;
        }
        case 0x7B: // LD A, E
        {
            cpu->a = cpu->e;
            cycles = 1;
            break;
        }
        case 0x7C: // LD A, H
        {
            cpu->a = cpu->h;
            cycles = 1;
            break;
        }
        case 0x7D: // LD A, L
        {
            cpu->a = cpu->l;
            cycles = 1;
            break;
        }
        case 0x7E: // LD A, (HL)
        {
            cpu->a = mmuReadByte(mmu, cpu->hl);
            cycles = 2;
            break;
        }
        case 0x7F: // LD A, A
        {
            cpu->a = cpu->a; // No operation, just a copy
            cycles = 1;
            break;
        }
        case 0x80: // ADD A, B
        {
            uint8_t result = cpu->a + cpu->b;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, ((cpu->a & 0x0F) + (cpu->b & 0x0F)) > 0x0F);
            setFlags(cpu, FLAG_C, result < cpu->a); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x81: // ADD A, C
        {
            uint8_t result = cpu->a + cpu->c;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, ((cpu->a & 0x0F) + (cpu->c & 0x0F)) > 0x0F);
            setFlags(cpu, FLAG_C, result < cpu->a); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x82: // ADD A, D
        {
            uint8_t result = cpu->a + cpu->d;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, ((cpu->a & 0x0F) + (cpu->d & 0x0F)) > 0x0F);
            setFlags(cpu, FLAG_C, result < cpu->a); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x83: // ADD A, E
        {
            uint8_t result = cpu->a + cpu->e;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, ((cpu->a & 0x0F) + (cpu->e & 0x0F)) > 0x0F);
            setFlags(cpu, FLAG_C, result < cpu->a); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x84: // ADD A, H
        {
            uint8_t result = cpu->a + cpu->h;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, ((cpu->a & 0x0F) + (cpu->h & 0x0F)) > 0x0F);
            setFlags(cpu, FLAG_C, result < cpu->a); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x85: // ADD A, L
        {
            uint8_t result = cpu->a + cpu->l;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, ((cpu->a & 0x0F) + (cpu->l & 0x0F)) > 0x0F);
            setFlags(cpu, FLAG_C, result < cpu->a); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x86: // ADD A, (HL)
        {
            uint8_t value = mmuReadByte(mmu, cpu->hl);
            uint8_t result = cpu->a + value;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, ((cpu->a & 0x0F) + (value & 0x0F)) > 0x0F);
            setFlags(cpu, FLAG_C, result < cpu->a); // Check for carry
            cpu->a = result;
            cycles = 2;
            break;
        }
        case 0x87: // ADD A, A
        {
            uint8_t result = cpu->a + cpu->a;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, ((cpu->a & 0x0F) + (cpu->a & 0x0F)) > 0x0F);
            setFlags(cpu, FLAG_C, result < cpu->a); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x88: // ADC A, B
        {
            uint8_t carry = (cpu->f & FLAG_C) ? 1 : 0;
            uint8_t result = cpu->a + cpu->b + carry;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, ((cpu->a & 0x0F) + (cpu->b & 0x0F) + carry) > 0x0F);
            setFlags(cpu, FLAG_C, result < cpu->a || (carry && result <= cpu->a)); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x89: // ADC A, C
        {
            uint8_t carry = (cpu->f & FLAG_C) ? 1 : 0;
            uint8_t result = cpu->a + cpu->c + carry;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, ((cpu->a & 0x0F) + (cpu->c & 0x0F) + carry) > 0x0F);
            setFlags(cpu, FLAG_C, result < cpu->a || (carry && result <= cpu->a)); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x8A: // ADC A, D
        {
            uint8_t carry = (cpu->f & FLAG_C) ? 1 : 0;
            uint8_t result = cpu->a + cpu->d + carry;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, ((cpu->a & 0x0F) + (cpu->d & 0x0F) + carry) > 0x0F);
            setFlags(cpu, FLAG_C, result < cpu->a || (carry && result <= cpu->a)); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x8B: // ADC A, E
        {
            uint8_t carry = (cpu->f & FLAG_C) ? 1 : 0;
            uint8_t result = cpu->a + cpu->e + carry;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, ((cpu->a & 0x0F) + (cpu->e & 0x0F) + carry) > 0x0F);
            setFlags(cpu, FLAG_C, result < cpu->a || (carry && result <= cpu->a)); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x8C: // ADC A, H
        {
            uint8_t carry = (cpu->f & FLAG_C) ? 1 : 0;
            uint8_t result = cpu->a + cpu->h + carry;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, ((cpu->a & 0x0F) + (cpu->h & 0x0F) + carry) > 0x0F);
            setFlags(cpu, FLAG_C, result < cpu->a || (carry && result <= cpu->a)); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x8D: // ADC A, L
        {
            uint8_t carry = (cpu->f & FLAG_C) ? 1 : 0;
            uint8_t result = cpu->a + cpu->l + carry;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, ((cpu->a & 0x0F) + (cpu->l & 0x0F) + carry) > 0x0F);
            setFlags(cpu, FLAG_C, result < cpu->a || (carry && result <= cpu->a)); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x8E: // ADC A, (HL)
        {
            uint8_t carry = (cpu->f & FLAG_C) ? 1 : 0;
            uint8_t value = mmuReadByte(mmu, cpu->hl);
            uint8_t result = cpu->a + value + carry;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, ((cpu->a & 0x0F) + (value & 0x0F) + carry) > 0x0F);
            setFlags(cpu, FLAG_C, result < cpu->a || (carry && result <= cpu->a)); // Check for carry
            cpu->a = result;
            cycles = 2;
            break;
        }
        case 0x8F: // ADC A, A
        {
            uint8_t carry = (cpu->f & FLAG_C) ? 1 : 0;
            uint8_t result = cpu->a + cpu->a + carry;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, ((cpu->a & 0x0F) + (cpu->a & 0x0F) + carry) > 0x0F);
            setFlags(cpu, FLAG_C, result < cpu->a || (carry && result <= cpu->a)); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x90: // SUB B
        {
            uint8_t result = cpu->a - cpu->b;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) < (cpu->b & 0x0F));
            setFlags(cpu, FLAG_C, result > cpu->a); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x91: // SUB C
        {
            uint8_t result = cpu->a - cpu->c;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) < (cpu->c & 0x0F));
            setFlags(cpu, FLAG_C, result > cpu->a); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x92: // SUB D
        {
            uint8_t result = cpu->a - cpu->d;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) < (cpu->d & 0x0F));
            setFlags(cpu, FLAG_C, result > cpu->a); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x93: // SUB E
        {
            uint8_t result = cpu->a - cpu->e;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) < (cpu->e & 0x0F));
            setFlags(cpu, FLAG_C, result > cpu->a); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x94: // SUB H
        {
            uint8_t result = cpu->a - cpu->h;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) < (cpu->h & 0x0F));
            setFlags(cpu, FLAG_C, result > cpu->a); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x95: // SUB L
        {
            uint8_t result = cpu->a - cpu->l;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) < (cpu->l & 0x0F));
            setFlags(cpu, FLAG_C, result > cpu->a); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x96: // SUB (HL)
        {
            uint8_t value = mmuReadByte(mmu, cpu->hl);
            uint8_t result = cpu->a - value;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) < (value & 0x0F));
            setFlags(cpu, FLAG_C, result > cpu->a); // Check for carry
            cpu->a = result;
            cycles = 2;
            break;
        }
        case 0x97: // SUB A
        {
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            setFlags(cpu, FLAG_H, 0); // No half carry
            setFlags(cpu, FLAG_C, 0); // No carry
            cpu->a = 0; // Result is always zero
            cycles = 1;
            break;
        }
        case 0x98: // SBC A, B
        {
            uint8_t carry = (cpu->f & FLAG_C) ? 1 : 0;
            uint8_t result = cpu->a - cpu->b - carry;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) < ((cpu->b & 0x0F) + carry));
            setFlags(cpu, FLAG_C, result > cpu->a || (carry && result >= cpu->a)); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x99: // SBC A, C
        {
            uint8_t carry = (cpu->f & FLAG_C) ? 1 : 0;
            uint8_t result = cpu->a - cpu->c - carry;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) < ((cpu->c & 0x0F) + carry));
            setFlags(cpu, FLAG_C, result > cpu->a || (carry && result >= cpu->a)); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x9A: // SBC A, D
        {
            uint8_t carry = (cpu->f & FLAG_C) ? 1 : 0;
            uint8_t result = cpu->a - cpu->d - carry;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) < ((cpu->d & 0x0F) + carry));
            setFlags(cpu, FLAG_C, result > cpu->a || (carry && result >= cpu->a)); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x9B: // SBC A, E
        {
            uint8_t carry = (cpu->f & FLAG_C) ? 1 : 0;
            uint8_t result = cpu->a - cpu->e - carry;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) < ((cpu->e & 0x0F) + carry));
            setFlags(cpu, FLAG_C, result > cpu->a || (carry && result >= cpu->a)); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x9C: // SBC A, H
        {
            uint8_t carry = (cpu->f & FLAG_C) ? 1 : 0;
            uint8_t result = cpu->a - cpu->h - carry;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) < ((cpu->h & 0x0F) + carry));
            setFlags(cpu, FLAG_C, result > cpu->a || (carry && result >= cpu->a)); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x9D: // SBC A, L
        {
            uint8_t carry = (cpu->f & FLAG_C) ? 1 : 0;
            uint8_t result = cpu->a - cpu->l - carry;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) < ((cpu->l & 0x0F) + carry));
            setFlags(cpu, FLAG_C, result > cpu->a || (carry && result >= cpu->a)); // Check for carry
            cpu->a = result;
            cycles = 1;
            break;
        }
        case 0x9E: // SBC A, (HL)
        {
            uint8_t carry = (cpu->f & FLAG_C) ? 1 : 0;
            uint8_t value = mmuReadByte(mmu, cpu->hl);
            uint8_t result = cpu->a - value - carry;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) < ((value & 0x0F) + carry));
            setFlags(cpu, FLAG_C, result > cpu->a || (carry && result >= cpu->a)); // Check for carry
            cpu->a = result;
            cycles = 2;
            break;
        }
        case 0x9F: // SBC A, A
        {
            uint8_t carry = (cpu->f & FLAG_C) ? 1 : 0;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            setFlags(cpu, FLAG_H, 0); // No half carry
            setFlags(cpu, FLAG_C, carry); // Carry is set if there was a borrow
            cpu->a = 0; // Result is always zero
            cycles = 1;
            break;
        }
        case 0xA0: // AND B
        {
            cpu->a &= cpu->b;
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            setFlags(cpu, FLAG_H, 1); // Half carry is always set for AND
            cycles = 1;
            break;
        }
        case 0xA1: // AND C
        {
            cpu->a &= cpu->c;
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            setFlags(cpu, FLAG_H, 1); // Half carry is always set for AND
            cycles = 1;
            break;
        }
        case 0xA2: // AND D
        {
            cpu->a &= cpu->d;
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            setFlags(cpu, FLAG_H, 1); // Half carry is always set for AND
            cycles = 1;
            break;
        }
        case 0xA3: // AND E
        {
            cpu->a &= cpu->e;
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            setFlags(cpu, FLAG_H, 1); // Half carry is always set for AND
            cycles = 1;
            break;
        }
        case 0xA4: // AND H
        {
            cpu->a &= cpu->h;
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            setFlags(cpu, FLAG_H, 1); // Half carry is always set for AND
            cycles = 1;
            break;
        }
        case 0xA5: // AND L
        {
            cpu->a &= cpu->l;
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            setFlags(cpu, FLAG_H, 1); // Half carry is always set for AND
            cycles = 1;
            break;
        }
        case 0xA6: // AND (HL)
        {
            uint8_t value = mmuReadByte(mmu, cpu->hl);
            cpu->a &= value;
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            setFlags(cpu, FLAG_H, 1); // Half carry is always set for AND
            cycles = 2;
            break;
        }
        case 0xA7: // AND A
        {
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            setFlags(cpu, FLAG_H, 1); // Half carry is always set for AND
            cycles = 1;
            break;
        }
        case 0xA8: // XOR B
        {
            cpu->a ^= cpu->b;
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H | FLAG_C);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            cycles = 1;
            break;
        }
        case 0xA9: // XOR C
        {
            cpu->a ^= cpu->c;
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H | FLAG_C);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            cycles = 1;
            break;
        }
        case 0xAA: // XOR D
        {
            cpu->a ^= cpu->d;
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H | FLAG_C);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            cycles = 1;
            break;
        }
        case 0xAB: // XOR E
        {
            cpu->a ^= cpu->e;
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H | FLAG_C);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            cycles = 1;
            break;
        }
        case 0xAC: // XOR H
        {
            cpu->a ^= cpu->h;
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H | FLAG_C);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            cycles = 1;
            break;
        }
        case 0xAD: // XOR L
        {
            cpu->a ^= cpu->l;
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H | FLAG_C);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            cycles = 1;
            break;
        }
        case 0xAE: // XOR (HL)
        {
            uint8_t value = mmuReadByte(mmu, cpu->hl);
            cpu->a ^= value;
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H | FLAG_C);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            cycles = 2;
            break;
        }
        case 0xAF: // XOR A
        {
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H | FLAG_C);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            cycles = 1;
            break;
        }
        case 0xB0: // OR B
        {
            cpu->a |= cpu->b;
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H | FLAG_C);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            cycles = 1;
            break;
        }
        case 0xB1: // OR C
        {
            cpu->a |= cpu->c;
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H | FLAG_C);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            cycles = 1;
            break;
        }
        case 0xB2: // OR D
        {
            cpu->a |= cpu->d;
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H | FLAG_C);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            cycles = 1;
            break;
        }
        case 0xB3: // OR E
        {
            cpu->a |= cpu->e;
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H | FLAG_C);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            cycles = 1;
            break;
        }
        case 0xB4: // OR H
        {
            cpu->a |= cpu->h;
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H | FLAG_C);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            cycles = 1;
            break;
        }
        case 0xB5: // OR L
        {
            cpu->a |= cpu->l;
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H | FLAG_C);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            cycles = 1;
            break;
        }
        case 0xB6: // OR (HL)
        {
            uint8_t value = mmuReadByte(mmu, cpu->hl);
            cpu->a |= value;
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H | FLAG_C);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            cycles = 2;
            break;
        }
        case 0xB7: // OR A
        {
            cpu->f = (uint8_t)~(FLAG_N | FLAG_H | FLAG_C);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            cycles = 1;
            break;
        }
        case 0xB8: // CP B
        {
            uint8_t result = cpu->a - cpu->b;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) < (cpu->b & 0x0F));
            setFlags(cpu, FLAG_C, result > cpu->a); // Check for carry
            cycles = 1;
            break;
        }
        case 0xB9: // CP C
        {
            uint8_t result = cpu->a - cpu->c;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) < (cpu->c & 0x0F));
            setFlags(cpu, FLAG_C, result > cpu->a); // Check for carry
            cycles = 1;
            break;
        }
        case 0xBA: // CP D
        {
            uint8_t result = cpu->a - cpu->d;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) < (cpu->d & 0x0F));
            setFlags(cpu, FLAG_C, result > cpu->a); // Check for carry
            cycles = 1;
            break;
        }
        case 0xBB: // CP E
        {
            uint8_t result = cpu->a - cpu->e;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) < (cpu->e & 0x0F));
            setFlags(cpu, FLAG_C, result > cpu->a); // Check for carry
            cycles = 1;
            break;
        }
        case 0xBC: // CP H
        {
            uint8_t result = cpu->a - cpu->h;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) < (cpu->h & 0x0F));
            setFlags(cpu, FLAG_C, result > cpu->a); // Check for carry
            cycles = 1;
            break;
        }
        case 0xBD: // CP L
        {
            uint8_t result = cpu->a - cpu->l;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) < (cpu->l & 0x0F));
            setFlags(cpu, FLAG_C, result > cpu->a); // Check for carry
            cycles = 1;
            break;
        }
        case 0xBE: // CP (HL)
        {
            uint8_t value = mmuReadByte(mmu, cpu->hl);
            uint8_t result = cpu->a - value;
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);

            setFlags(cpu, FLAG_Z, result == 0);
            setFlags(cpu, FLAG_H, (cpu->a & 0x0F) < (value & 0x0F));
            setFlags(cpu, FLAG_C, result > cpu->a); // Check for carry
            cycles = 2;
            break;
        }
        case 0xBF: // CP A
        {
            cpu->f = (uint8_t)~(FLAG_Z | FLAG_N | FLAG_H);
            setFlags(cpu, FLAG_Z, cpu->a == 0);
            setFlags(cpu, FLAG_H, 0); // No half carry
            setFlags(cpu, FLAG_C, 0); // No carry
            cycles = 1;
            break;
        }
        case 0xC0: // RET NZ
        {
            if((cpu->f & FLAG_Z) == 0) {
                uint8_t low = popByte(cpu, mmu);
                uint8_t high = popByte(cpu, mmu);
                cpu->pc = (high << 8) | low;
                cycles = 5;
            } else 
                cycles = 2;
        }
        case 0xC1: // POP BC
        {
            cpu->c = popByte(cpu, mmu);
            cpu->b = popByte(cpu, mmu);
            cycles = 3;
            break;
        }
    }

    return cycles;
}