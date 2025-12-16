#include <nds.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "GFXintro.h"
#include "GFXalfPresents.h"
#include <maxmod9.h>
#include "soundbank_bin.h"
#include "soundbank.h"

#define usesOAM true

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

static u16* gfx = SPRITE_GFX;
void topBackground8() {
    u8* v = (u8*)bgGetGfxPtr(2);

    for (int i = 0; i < 31; i++) {

        u8* row = &v[i << 8];
        for (int x = 0; x < 256; x++)
            row[x] = 17 + i;
    }

    for (int i = 0; i < 32; i++) {
        u8* row = &v[(i << 8) + 256*161];

        for (int x = 0; x < 256; x++)
            row[x] = 47-i;
    }
}

void generateGradientPalette() {
    BG_PALETTE[16] = 0;
    u8 r = 16;
    u8 b = 16;
    for (int i = 0; i < 31; i++) {
        int temp = ((31-i)-16);
        r = temp > 0 ? temp : 0;
        b = (31-i)>>1;
        BG_PALETTE[i+17] = (b << 10) | r | 0x8000;
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

//----------------- setup estrellas (oam)-----------------------
#define MAX_STARS   32
#define FP_SHIFT   7       // 7 bits fracción

//centro de la pantalla (depende del dispositivo)
#define CENTER_X   128      
#define CENTER_Y   96
#define SCREEN_H   (CENTER_Y<<1)
#define SCREEN_W   (CENTER_X<<1)
static s16 starDX[MAX_STARS];
static s16 starDY[MAX_STARS];
static u8  starZ [MAX_STARS];
static u16  spriteX[MAX_STARS];
static u16   spriteY[MAX_STARS];
//8kb usados
static inline void resetStar(int i) {
    // posición inicial aleatoria
    s16 x = rand() & (SCREEN_W-1);
    s16 y = rand() & (SCREEN_H-1);
    //s16 y = (rand() & 127)+32;//hardcodeado para DS

    spriteX[i] = x<<FP_SHIFT;
    spriteY[i] = y<<FP_SHIFT;

    // vector desde el centro
    starDX[i] = x - CENTER_X;
    starDY[i] = y - CENTER_Y;

    starZ[i] = 1; // velocidad inicial
}
#define STAR_PAL_COUNT 16

static void initStars() {
    //hardware
    oamInit(&oamMain, SpriteMapping_1D_32, false);
    //crear un sprite para las estrellas
    for (int i = 0; i < 8*8; i++) gfx[i] = 0; // transparente
    gfx[0] = 0x0001;//primer pixel de color blanco

    //generar paleta 4bpp
    for (int i = 0; i < STAR_PAL_COUNT; i++) {
        int v = i<<(32/STAR_PAL_COUNT);
        if (v > 31) v = 31;

        SPRITE_PALETTE[(i<<4)+1] = 0x8000 | (v<<10) | (v<<5) | v;
    }

    for (int i = 0; i < MAX_STARS; i++) {
        resetStar(i);
    }
}
#define fixAlpha 4
void updateStars(int i) {
    int tempZ = starZ[i] + 1;
    starZ[i] = MIN(tempZ,255);//limitar

    int speed = MAX(starZ[i]>>4,1);

    // posición actual
    s16 x = spriteX[i];
    s16 y = spriteY[i];

    // movimiento incremental
    x += ((starDX[i]>>1) * speed);
    y += ((starDY[i]>>1) * speed);

    s16 fx = x>>FP_SHIFT;
    s16 fy = y>>FP_SHIFT;

    // fuera de pantalla
    if (fx < 0 || fx >= SCREEN_W || fy < 0 || fy >= SCREEN_H) {
        resetStar(i);
        return;
    }
    // guardar nueva posición
    spriteX[i] = x;
    spriteY[i] = y;
}


void intro() {
    swiWaitForVBlank();

    mmInitDefaultMem((mm_addr)soundbank_bin);
    mmLoad(MOD_INTRO);
    mmStart(MOD_INTRO, MM_PLAY_ONCE);

    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_SPRITE);

    videoSetMode(MODE_5_2D);

    int bg2 = bgInit(2, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
    int bg3 = bgInit(3, BgType_Bmp8, BgSize_B8_128x128, 4, 0);

    //limpiar VRAM
    memset(bgGetGfxPtr(2), 0, 192 << 8);
    memset(bgGetGfxPtr(3), 0, 128 << 7);

    decompress(GFXalfPresentsBitmap, bgGetGfxPtr(bg3), LZ77Vram);
    dmaCopy(GFXalfPresentsPal, &BG_PALETTE[0], GFXalfPresentsPalLen);

    generateGradientPalette();
    topBackground8();
    if(usesOAM){
        initStars();
    }
    else{
        drawStars();//en BG
    }

    bgSetScroll(bg3, -64, -58);
    bgSetPriority(bg2, 3);
    bgSetPriority(bg3, 2);

    // --- Pantalla inferior ---
    videoSetModeSub(MODE_5_2D);
    bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    decompress(GFXintroBitmap, BG_GFX_SUB, LZ77Vram);
    
    while (1) {//loop
        scanKeys();
        if (keysDown()) break;
        if(usesOAM){
            for (int i = 0; i < MAX_STARS; i++) {
                updateStars(i);
                oamSet(&oamMain, i,
                    spriteX[i]>>FP_SHIFT,
                    spriteY[i]>>FP_SHIFT,
                    0,
                    (starZ[i]>>fixAlpha)+1,
                    SpriteSize_8x8,
                    SpriteColorFormat_16Color,
                    gfx,
                    -1,
                    false, false, false, false, false
                );
            }
        }
        swiWaitForVBlank();
        oamUpdate(&oamMain);
    }
    //al salir
    oamClear(&oamMain, 0, 128);
    mmStop();
    oamUpdate(&oamMain);
}
