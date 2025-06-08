#ifndef CPU_H
    #define CPU_H

    #include <stdint.h>
    #include "mmu.h"

    #define FLAG_Z  (1 << 7) // Zero Flag
    #define FLAG_N  (1 << 6) // Subtract Flag
    #define FLAG_H  (1 << 5) // Half Carry Flag
    #define FLAG_C  (1 << 4) // Carry Flag

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