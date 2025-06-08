#include "../includes/mmu.h"
#include "../includes/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t gbRomSize[] = { 2, 4, 8, 32, 64, 128 };
static uint8_t gbRamSize[] = { 0, 1, 1, 4, 16, 8 };

int initMMU(MMU *mmu, const char *filename)
{
    LOG("Initializing MMU with ROM file: %s", filename);

    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        LOG("Failed to open ROM file: %s", filename);
        return 1; // Error opening file
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    mmu->romData = malloc(fileSize);
    if (!mmu->romData)
    {
        LOG("Failed to allocate memory for ROM data");
        fclose(file);
        return 1; // Memory allocation error
    }

    fread(mmu->romData, 1, fileSize, file);
    fclose(file);

    uint8_t romCode = mmu->romData[HEADER_ROM_SIZE_OFFSET];
    uint8_t ramCode = mmu->romData[HEADER_RAM_SIZE_OFFSET];

    mmu->romBankCount = gbRomSize[romCode];
    mmu->ramBankCount = gbRamSize[ramCode];


    mmu->ramEnabled = false;
    mmu->bankingMode = false;
    mmu->currentRomBank = 1; // Start with bank 1
    mmu->currentRamBank = 0; // Start with bank 0
    LOG("MMU initialized with ROM size: %d banks, RAM size: %d banks", mmu->romBankCount, mmu->ramBankCount);

    memset(mmu->vram, 0, sizeof(mmu->vram));
    memset(mmu->wram, 0, sizeof(mmu->wram));
    memset(mmu->hram, 0, sizeof(mmu->hram));
    memset(mmu->oam,  0, sizeof(mmu->oam));
    mmu->ieRegisters = 0;

    LOG("MMU initialization complete");
    return 0; // Success
}

void freeMMU(MMU *mmu)
{
    if (mmu->romData) free(mmu->romData);
}

uint8_t mmuReadByte(MMU *mmu, uint16_t adress)
{
    if (adress < 0x4000)
        return mmu->romData[adress]; // ROM area
    else if (adress < 0x8000)
    {
        uint32_t offset = 0x4000 * mmu->currentRomBank;
        return mmu->romData[offset + (adress - 0x4000)]; // ROM banked area
    }
    else if (adress < 0xA000)
        return mmu->vram[adress - 0x8000]; 
    else if (adress < 0xC000)
    {
        if(mmu->ramEnabled && mmu->ramBankCount > 0)
            return mmu->romData[0x2000* (mmu->currentRamBank + mmu->ramBankCount) + (adress - 0xA000)];
        return 0xFF;                        // RAM area, but RAM is disabled or no RAM banks
    }
    else if (adress < 0xE000)
        return mmu->wram[adress - 0xC000];  // WRAM area
    else if (adress < 0xFE00)
        return mmu->wram[adress - 0xE000];  // WRAM mirror area
    else if (adress < 0xFEA0)
        return mmu->oam[adress - 0xFE00];   // OAM area
    else if (adress < 0xFF00)
        return 0xFF;                        // Useless area i guess
    else if (adress < 0xFF80)
        return ioReadByte(adress);          // IO registers
    else if (adress < 0xFFFF)
        return mmu->hram[adress - 0xFF80];  // HRAM area
    else 
        return mmu->ieRegisters;            // IE register
}

uint8_t ioReadByte(uint16_t adress)
{
    // Placeholder for IO register read logic
    // This function should be implemented to handle specific IO registers
    LOG("Reading from IO register at address: 0x%04X", adress);
    return 0;
}

void mmuWriteByte(MMU *mmu, uint16_t adress, uint8_t value)
{
    if (adress > 0x2000)
        mmu->ramEnabled = ((value & 0x0F) == 0x0A); // Enable RAM if value is 0x0A
    else if (adress < 0x4000)
    {
        uint8_t bank = value & 0x1F;
        if(!bank) bank = 1;

        mmu->currentRomBank = (mmu->currentRomBank & 0x60) | bank;
    }
    else if (adress < 0x6000)
    {
        uint8_t v = value & 0x03;
        if(!mmu->bankingMode)
            mmu->currentRomBank = (mmu->currentRomBank & 0x1F) | (v << 5);
        else
            mmu->currentRamBank = v; 
    }
    else if (adress < 0x8000)
        mmu->bankingMode = value & 0x01; // Set banking mode
    else if (adress < 0xA000)
        mmu->vram[adress - 0x8000] = value; // VRAM area
    else if (adress < 0xC000)
    {
        if(mmu->ramEnabled && mmu->ramBankCount > 0)
            mmu->romData[0x2000 * (mmu->currentRamBank + mmu->ramBankCount) + (adress - 0xA000)] = value;
    }
    else if (adress < 0xE000)
        mmu->wram[adress - 0xC000] = value;
    else if (adress < 0xFE00)
        mmu->wram[adress - 0xE000] = value;
    else if (adress < 0xFEA0)
        mmu->oam[adress - 0xFE00] = value;
    else if (adress < 0xFF00) 
        {}
    else if (adress < 0xFF80)
        ioWriteByte(adress, value); // IO registers
    else if (adress < 0xFFFF)
        mmu->hram[adress - 0xFF80] = value; // HRAM area
    else
        mmu->ieRegisters = value; // IE register
}

void ioWriteByte(uint16_t adress, uint8_t value)
{
    // Placeholder for IO register write logic
    // This function should be implemented to handle specific IO registers
    LOG("Writing to IO register at address: 0x%04X, value: 0x%02X", adress, value);
    // Implement specific IO register handling here
}