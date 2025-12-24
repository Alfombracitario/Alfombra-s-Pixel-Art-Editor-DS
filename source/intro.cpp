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

//macros para el fondo
#define fixAlpha 3
#define MAX_STARS   64
#define FP_SHIFT   7       // 7 bits fracción
#define STAR_PAL_COUNT 16

//centro de la pantalla (depende del dispositivo)
#define CENTER_X   128      
#define CENTER_Y   96
#define SCREEN_H   (CENTER_Y<<1)
#define SCREEN_W   (CENTER_X<<1)

static u16* gfx = SPRITE_GFX;

static u16 bg0ofs_table[192];
static u16 bg0vofs_table[192];
static u16 gradientTable[192];

static int fixedXoffset = 64;
static s16 starDX[MAX_STARS];
static s16 starDY[MAX_STARS];
static u8  starZ [MAX_STARS];
static u16 spriteX[MAX_STARS];
static u16 spriteY[MAX_STARS];

static void vblank_handler(void)
{
    // Stop the previous DMA copy
    dmaStopSafe(0);
    dmaStopSafe(1);
    dmaStopSafe(2);
    // Set the horizontal scroll for the first line. The first horizontal blank
    // happens after line 0 has been drawn, so we need to set the scroll of line
    // 0 now.
    REG_BG0HOFS = bg0ofs_table[0];
    REG_BG0VOFS = bg0vofs_table[0];
    BG_PALETTE[0] = gradientTable[0];

    // Make sure that DMA can see the updated values of the arrays and the
    // updated values don't stay in the data cache.
    DC_FlushRange(bg0ofs_table, sizeof(bg0ofs_table));
    DC_FlushRange(bg0vofs_table, sizeof(bg0vofs_table));
    
    // Restart the DMA copy
    dmaSetParams(0,
                 &bg0ofs_table[1], // Skip first entry (we have just used it)
                 (void *)&REG_BG0HOFS, // Write to horizontal scroll register
                 DMA_SRC_INC | // Autoincrement source after each copy
                 DMA_DST_FIX | // Keep destination fixed
                 DMA_START_HBL | // Start copy at the start of horizontal blank
                 DMA_REPEAT | // Don't stop DMA after the first copy.
                 DMA_COPY_HALFWORDS | 1 | // Copy one halfword each time
                 DMA_ENABLE);

    dmaSetParams(1,
                 &bg0vofs_table[1], // Skip first entry (we have just used it)
                 (void *)&REG_BG0VOFS, // Write to vertical scroll register
                 DMA_SRC_INC | // Autoincrement source after each copy
                 DMA_DST_FIX | // Keep destination fixed
                 DMA_START_HBL | // Start copy at the start of horizontal blank
                 DMA_REPEAT | // Don't stop DMA after the first copy.
                 DMA_COPY_HALFWORDS | 1 | // Copy one halfword each time
                 DMA_ENABLE);

    dmaSetParams(2,
                 &gradientTable[1],
                 &BG_PALETTE[0], // Write to the background color
                 DMA_SRC_INC | // Autoincrement source after each copy
                 DMA_DST_FIX | // Keep destination fixed
                 DMA_START_HBL | // Start copy at the start of horizontal blank
                 DMA_REPEAT | // Don't stop DMA after the first copy.
                 DMA_COPY_HALFWORDS | 1 | // Copy one halfword each time
                 DMA_ENABLE);
}
static void doNothing(void)
{

}
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
    setBrightness(3, -16);
    irqSet(IRQ_VBLANK, vblank_handler);//configurar HDMA
    irqEnable(IRQ_VBLANK);

    mmInitDefaultMem((mm_addr)soundbank_bin);
    mmLoad(MOD_INTRO);
    mmStart(MOD_INTRO, MM_PLAY_ONCE);

    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_SPRITE);

    videoSetMode(MODE_5_2D);

    int bg0 = bgInit(0,BgType_Text4bpp,BgSize_T_256x256,4, 1);

    //limpiar VRAM
    memset(bgGetGfxPtr(2), 0, 192 << 8);
    memset(bgGetGfxPtr(0), 0, 128 << 7);

    dmaCopy(GFXalfPresentsPal, &BG_PALETTE[0], GFXalfPresentsPalLen);
    dmaCopy(GFXalfPresentsMap, bgGetMapPtr(bg0), GFXalfPresentsMapLen);
    dmaCopy(GFXalfPresentsTiles, bgGetGfxPtr(bg0), GFXalfPresentsTilesLen);

    initStars();

    bgSetPriority(bg0, 0);//más adelante posible

    // --- Pantalla inferior ---
    videoSetModeSub(MODE_5_2D);
    bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    decompress(GFXintroBitmap, BG_GFX_SUB, LZ77Vram);
    
    s16 xOffset = 1024;
    int brightness = -16;

    for (int i = 0; i < 32; i++) {
        int r = 15 - i;
        if (r < 0) r = 0;

        int b = (31 - i) >> 1;
        u16 color = (b << 10) | r;
        gradientTable[i] = color;
        gradientTable[SCREEN_H-i] = color;
    }

    while (1) {//loop
        scanKeys();
        if (keysDown()) break;//si se apreta cualquier tecla, salir de la intro

        if(brightness < 0){
            brightness++;
        }
        for (int i = 0; i < MAX_STARS; i++) {
            updateStars(i);
            oamSet(&oamMain, i,
                spriteX[i]>>FP_SHIFT,
                spriteY[i]>>FP_SHIFT,
                3,//prioridad
                (starZ[i]>>fixAlpha),
                SpriteSize_8x8,
                SpriteColorFormat_16Color,
                gfx,
                -1,
                false, false, false, false, false
            );
        }
        setBrightness(3, brightness);
        oamUpdate(&oamMain);
        bgUpdate();
        //Administrar HDMA
        //desde el scanline 58 hasta 83 ALFOMBRA de
        if(fixedXoffset != 0){
            xOffset -= fixedXoffset;

            //fixear
            fixedXoffset = xOffset>>4;

            //programado de manera no optimizada para hacerlo, luego optimizar
            int start = fixedXoffset - 58;
            if (start < 0) start = -start;
            if (start > 82) start = 83;

            int end = start + 24;
            if (end > 82) end = 82;

            // limpiar
            for (int i = 0; i <= 82; i++)
                bg0vofs_table[i] = 133;

            // escribir ventana
            for (int i = start; i <= end; i++)
                bg0vofs_table[i] = fixedXoffset;

            for (int i = 83; i <= 108; i++)//MADERA
            {
                bg0ofs_table[i] = fixedXoffset;
            }
            for (int i = 113; i <= 132; i++)//PRESENTS
            {
                bg0ofs_table[i] = -fixedXoffset;
            }
        }
        swiWaitForVBlank();
    }
    //se rompió el loop

    for(int j = 0; j < 16; j++){//ultimos 16 frames antes de iniciar
        //seguimos actualizando las estrellas
        for (int i = 0; i < MAX_STARS; i++) {
            updateStars(i);
            oamSet(&oamMain, i,
                spriteX[i]>>FP_SHIFT,
                spriteY[i]>>FP_SHIFT,
                3,//prioridad
                (starZ[i]>>fixAlpha),
                SpriteSize_8x8,
                SpriteColorFormat_16Color,
                gfx,
                -1,
                false, false, false, false, false
            );
        }
        //efecto genial de fade out
        int offset = j<<2;
        for(int i = 0; i < 192; i++){
            bg0ofs_table[i] = (i & 1) ? offset : -offset;
        }
        //oscurecer
        setBrightness(3, -j);

        swiWaitForVBlank();//esperamos un frame
    }
    //al salir
    oamClear(&oamMain, 0, 128);
    mmStop();
    oamUpdate(&oamMain);
    dmaStopSafe(0);
    dmaStopSafe(1);
    dmaStopSafe(2);
    irqSet(IRQ_VBLANK, doNothing);
    BG_PALETTE[0] = 0;
}
