#ifndef MMU_H
    #define MMU_H
    #include <stdint.h>
    #include <stdbool.h>

    #define HEADER_ROM_SIZE_OFFSET 0x0148
    #define HEADER_RAM_SIZE_OFFSET 0x0149

typedef struct 
{
    uint8_t *romData;
    uint8_t  romBankCount;
    uint8_t  ramBankCount;

    bool     ramEnabled;
    bool     bankingMode;

    uint8_t  currentRomBank;
    uint8_t  currentRamBank;

    uint8_t  vram[8192];
    uint8_t  wram[8192];
    uint8_t  hram[127];
    uint8_t  oam [160];
    
    uint8_t  ieRegisters;
} MMU;

int     initMMU     (MMU *mmu, const char *filename);
void    freeMMU     (MMU *mmu);

uint8_t ioReadByte  (uint16_t address);
void    ioWriteByte (uint16_t address, uint8_t value);

uint8_t mmuReadByte (MMU *mmu, uint16_t address);
void    mmuWriteByte(MMU *mmu, uint16_t address, uint8_t value);



#endif //! MMU_H