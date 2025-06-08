#include "../includes/mmu.h"
#include "../includes/log.h"
#include "../includes/cpu.h"
#include "../includes/ppu.h"


int main(int argc, char *argv[])
{
    if(!argv || argc < 2)
        return 1; // Invalid arguments

    if(logInit())
        return 2; // Failed to initialize logging

    MMU mmu;
    if(initMMU(&mmu, argv[1]))
    {
        logFree();
        LOG("Failed to initialize MMU with ROM file: %s", argv[1]);
        return 3; // Failed to initialize MMU
    }

    CPU cpu;
    cpuReset(&cpu);
    LOG("CPU initialized");

    PPU ppu;
    if(initPPU(&ppu))
    {
        freeMMU(&mmu);
        logFree();
        LOG("Failed to initialize PPU");
        return 4; // Failed to initialize PPU
    }

    LOG("PPU initialized, starting emulation...");
    while(1)
    {
        int cycles = cpuStep(&cpu, &mmu);
        ppuStep(&ppu, &mmu, cycles);

        if(ppu.LY == SCREEN_HEIGHT) ppuRender(&ppu);
    }

    freePPU(&ppu);
    LOG("Emulation finished, freeing resources...");
    freeMMU(&mmu);
    logFree();
    return 0; // Success
}