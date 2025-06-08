#ifndef PPU_H
    #define PPU_H

    #include <stdint.h>
    #include <SDL2/SDL.h>
    #include "mmu.h"

    #define SCREEN_WIDTH 160
    #define SCREEN_HEIGHT 144

typedef enum 
{
    PPU_MODE_HBLANK = 0,
    PPU_MODE_VBLANK = 1,
    PPU_MODE_OAM    = 2,
    PPU_MODE_VRAM   = 3
} PPUMode;

typedef struct 
{
    uint8_t  LCDC;        // LCD Control Register
    uint8_t  STAT;        // LCD Status Register
    uint8_t  SCY;         // Scroll Y
    uint8_t  SCX;         // Scroll X
    uint8_t  LY;          // LCD Y Coordinate
    uint8_t  LYC;         // LY Compare
    uint8_t  BGP;         // Background Palette Data
    uint8_t  OBP0;        // Object Palette 0 Data
    uint8_t  OBP1;        // Object Palette 1 Data

    int      modeClock;
    PPUMode  mode;         // Current PPU mode

    //TODO: Add upscaling
    uint32_t frameBuffer[SCREEN_HEIGHT][SCREEN_WIDTH]; // Frame buffer for rendering 

    SDL_Window *window;   // SDL window for rendering
    SDL_Renderer *renderer; // SDL renderer for drawing
    SDL_Texture *texture; // SDL texture for the frame buffer
} PPU;

int initPPU(PPU *ppu);
void freePPU(PPU *ppu);

void resetPPU(PPU *ppu);
void ppuStep(PPU *ppu, MMU *mmu, int cycles);
void ppuRender(PPU *ppu);

#endif // !PPU_H