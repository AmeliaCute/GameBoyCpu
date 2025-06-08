#include "../includes/ppu.h"
#include "../includes/log.h"
#include <stdlib.h>
#include <string.h>

static uint32_t palette[4] = 
{
    0x000000FF, // Black
    0x555555FF, // Dark Gray
    0xAAAAAAFF, // Light Gray
    0xFFFFFFFF  // White
};

int initPPU(PPU *ppu)
{
    if(SDL_Init(SDL_INIT_VIDEO))
    {
        LOG("SDL initialization failed: %s", SDL_GetError());
        return 1; // SDL initialization error
    }

    ppu->window = SDL_CreateWindow("Gameboy", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                    SCREEN_WIDTH * 8, SCREEN_HEIGHT * 8, SDL_WINDOW_SHOWN);
    if(!ppu->window)
    {
        LOG("Failed to create SDL window: %s", SDL_GetError());
        SDL_Quit();
        return 2; // Window creation error
    }

    ppu->renderer = SDL_CreateRenderer(ppu->window, -1, SDL_RENDERER_ACCELERATED);
    if(!ppu->renderer)
    {
        LOG("Failed to create SDL renderer: %s", SDL_GetError());
        SDL_DestroyWindow(ppu->window);
        SDL_Quit();
        return 3; // Renderer creation error
    }

    ppu->texture = SDL_CreateTexture(ppu->renderer, SDL_PIXELFORMAT_ARGB8888, 
                                      SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    if(!ppu->texture)
    {
        LOG("Failed to create SDL texture: %s", SDL_GetError());
        SDL_DestroyRenderer(ppu->renderer);
        SDL_DestroyWindow(ppu->window);
        SDL_Quit();
        return 4; // Texture creation error
    }

    resetPPU(ppu);
    LOG("PPU initialized successfully");
    return 0; // Success
}

void freePPU(PPU *ppu)
{
    if(ppu->texture) SDL_DestroyTexture(ppu->texture);
    if(ppu->renderer) SDL_DestroyRenderer(ppu->renderer);
    if(ppu->window) SDL_DestroyWindow(ppu->window);
    SDL_Quit();
    LOG("PPU resources freed");
} 

void resetPPU(PPU *ppu)
{
    memset(ppu->frameBuffer, 0, sizeof(ppu->frameBuffer));
    ppu->LCDC = 0x91;
    ppu->STAT = 0x85;
    ppu->SCY = ppu->SCX = 0;
    ppu->LY = ppu->LYC = 0;
    ppu->BGP = 0xFC; // Default background palette
    ppu->OBP0 = ppu->OBP1 = 0xFF; // Default object palettes
    ppu->modeClock = 0;
    ppu->mode = PPU_MODE_OAM; // Start in OAM mode
    memset(ppu->frameBuffer, 0, sizeof(ppu->frameBuffer));
    LOG("PPU reset to default state");
}

void ppuStep(PPU *ppu, MMU *mmu, int cycles)
{
    ppu->modeClock += cycles;

    switch(ppu->mode)
    {
        case PPU_MODE_OAM:
        {
            if(ppu->modeClock < 80) break;
            ppu->modeClock = -80;
            ppu->mode = PPU_MODE_VRAM;
            break;
        }
        case PPU_MODE_VRAM:
        {
            if(ppu->modeClock < 172) break;
            uint8_t y = ppu->LY;
            for (int x = 0; x < SCREEN_WIDTH; ++x)
            {
                uint16_t map_base = 0x9800;
                uint8_t scy = ppu->SCY;
                uint8_t scx = ppu->SCX;
                uint8_t tile_y = ((y + scy) / 8) & 0x1F;
                uint8_t tile_x = ((x + scx) / 8) & 0x1F;
                int16_t tile_index = mmuReadByte(mmu, map_base + tile_y * 32 + tile_x);
                uint32_t tile_adress = 0x8000 + (tile_index * 16);
                uint8_t line = (y + scy) % 8;
                uint8_t b1 = mmuReadByte(mmu, tile_adress + line * 2);
                uint8_t b2 = mmuReadByte(mmu, tile_adress + line * 2 + 1);
                int bit = 7 - ((x + scx) % 8);
                uint8_t color_index = ((b2 >> bit)&1) << 1 | ((b1 >> bit) & 1);
                uint32_t color = palette[(ppu->BGP >> (color_index * 2)) & 0x03];
                ppu->frameBuffer[y][x] = color;
            }
            ppu->mode = PPU_MODE_HBLANK;
            break;
        }
        case PPU_MODE_HBLANK:
        {
            if(ppu->modeClock < 204) break;
            ppu->modeClock = -204;
            ppu->LY++;
            if(ppu->LY >= SCREEN_HEIGHT)
                ppu->mode = PPU_MODE_VBLANK;
            else
                ppu->mode = PPU_MODE_OAM;
            break;
        }
        case PPU_MODE_VBLANK:
        {
            if(ppu->modeClock < 456) break;
            ppu->modeClock = -456;
            ppu->LY++;
            if(ppu->LY >= 153)
            {
                ppu->LY = 0;
                ppu->mode = PPU_MODE_OAM;
            }
            break;
        }
    }

    ppu->STAT = (ppu->STAT & 0xFC) | (ppu->mode & 0x03);
    ppu->STAT = (ppu->STAT & 0xF8) | ((ppu->LY == ppu->LYC) ? 0x04 : 0x00); 
}


void ppuRender(PPU *ppu) 
{
    SDL_UpdateTexture(ppu->texture, NULL, ppu->frameBuffer, SCREEN_WIDTH * sizeof(uint32_t));
    SDL_RenderClear(ppu->renderer);
    SDL_RenderCopy(ppu->renderer, ppu->texture, NULL, NULL);
    SDL_RenderPresent(ppu->renderer);
}