#include <nds.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "GFXintro.h"
#include "GFXalfPresents.h"
#include <maxmod9.h>
#include "soundbank_bin.h"
#include "soundbank.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
static u16* gfx = SPRITE_GFX;
void clearVRAM() {
    dmaFillWords(0, (void*)0x06800000, 128 * 1024);
    dmaFillWords(0, (void*)0x06820000, 128 * 1024);
    dmaFillWords(0, (void*)0x06840000, 128 * 1024);
    dmaFillWords(0, (void*)0x06860000, 128 * 1024);
}

void topBackground8() {
    u8* v = (u8*)bgGetGfxPtr(2);
    for (int i = 0; i < 16; i++) {
        u8* row = &v[i << 8];
        for (int x = 0; x < 256; x++)
            row[x] = 31 - i;
    }
    for (int i = 0; i < 16; i++) {
        u8* row = &v[(i << 8) + 45056];
        for (int x = 0; x < 256; x++)
            row[x] = i+16;
    }
}

void generateGradientPalette() {
    BG_PALETTE[16] = 0;
    for (int i = 1; i < 17; i++) {
        u8 r = MAX((i << 1) - 16, 0);
        u8 b = i;
        BG_PALETTE[i+16] = (r + (b << 10)) | 0x8000;
    }
}

void drawStars() {
    int amount = (rand() & 255) + 1024;
    u8* v = (u8*)bgGetGfxPtr(2);
    for (int i = 0; i < amount; i++) {
        int pos = (rand() % (256 * 160)) + 16 * 256;
        u8 colorIndex = rand() & 15;
        v[pos] = colorIndex+16;
    }
}
void intro() {
    clearVRAM();
    mmInitDefaultMem((mm_addr)soundbank_bin);
    mmLoad(MOD_INTRO);
    mmStart(MOD_INTRO, MM_PLAY_ONCE);

    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_SPRITE);

    videoSetMode(MODE_5_2D);

    int bg2 = bgInit(2, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
    int bg3 = bgInit(3, BgType_Bmp8, BgSize_B8_128x128, 4, 0);
    decompress(GFXalfPresentsBitmap, bgGetGfxPtr(bg3), LZ77Vram);
    dmaCopy(GFXalfPresentsPal, &BG_PALETTE[0], GFXalfPresentsPalLen);

    generateGradientPalette();
    topBackground8();
    drawStars();

    bgSetScroll(bg3, -64, -58);
    bgSetPriority(bg2, 3);
    bgSetPriority(bg3, 2);

    //setup de los sprites
    oamInit(&oamMain, SpriteMapping_1D_32, false);
    //crear un sprite para las estrellas
    for (int i = 0; i < 8*8; i++) gfx[i] = 0; // transparente
    gfx[0] = RGB15(31,31,31) | 0x8000;//primer pixel de color blanco


    // --- Pantalla inferior ---
    videoSetModeSub(MODE_5_2D);
    bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    decompress(GFXintroBitmap, BG_GFX_SUB, LZ77Vram);

    while (1) {
        scanKeys();
        if (keysDown()) break;
        swiWaitForVBlank();
        oamUpdate(&oamMain);
    }
    //al salir
    oamClear(&oamMain, 0, 128);
    mmStop();
    oamUpdate(&oamMain);
}
