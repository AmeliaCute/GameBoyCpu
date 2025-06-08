#ifndef CPU_H
    #define CPU_H

    #include <stdint.h>
    #include "mmu.h"

    #define FLAG_Z  ((uint8_t) 0x80) // Zero Flag
    #define FLAG_N  ((uint8_t) 0x40) // Subtract Flag
    #define FLAG_H  ((uint8_t) 0x20) // Half Carry Flag
    #define FLAG_C  ((uint8_t) 0x10) // Carry Flag

typedef struct 
{
    union { struct { uint8_t f, a; }; uint16_t af; };
    union { struct { uint8_t c, b; }; uint16_t bc; };
    union { struct { uint8_t e, d; }; uint16_t de; };
    union { struct { uint8_t l, h; }; uint16_t hl; };

    uint16_t sp; // Stack Pointer
    uint16_t pc; // Program Counter

    int ime;     // Interrupt Master Enable
} CPU;

void cpuReset(CPU *cpu);
int  cpuStep(CPU *cpu, MMU *mmu);

#endif // !CPU_H