#include <nds.h>
#include <stdio.h>
#include <math.h>
#include <time.h>//para el rng
#include "GFXintro.h"
#include <maxmod9.h>
#include "soundbank_bin.h"		// soundbank binary reference
#include "soundbank.h"			// generated soundbank definitions

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
//a pesar de que es lento escribir en VRAM, no queremos tener problemas de RAM para los proyectos
extern u16* pixelsTopVRAM;

void topBackground() {
    u16* v = pixelsTopVRAM;
    u8 r = 16;
    u8 b = 16;

    for(int j = 0; j < 192; j++)
    {
        // curva del gradiente
        if(j < 176) {
            if(r > 0) r -= 2;
            if(b > 0) b--;
        } else {
            if(r < 16 && j >= 184) r += 2;
            if(b < 16) b++;
        }

        u16 col = 0x8000 + r + (b<<10);

        u16* row = &v[j << 8];
        for(int x = 0; x < 256; x++) {
            row[x] = col;
        }
    }
}

void intro(){
    //sonido
	mmInitDefaultMem( (mm_addr)soundbank_bin );
    mmLoad( MOD_INTRO );
	mmStart( MOD_INTRO, MM_PLAY_ONCE);
    //modos gráficos
    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_SPRITE); // sprites en VRAM B
    bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    videoSetModeSub(MODE_5_2D);
    vramSetBankC(VRAM_C_SUB_BG);
    vramSetBankD(VRAM_D_SUB_SPRITE); // sprites en VRAM D
    bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    
    decompress(GFXintroBitmap, BG_GFX_SUB, LZ77Vram);//imagen de la intro

    srand(TIMER0_DATA);
    topBackground();
    int amount = (rand() & 256)+1024;
    for(int i = 0; i<amount; i++){//dibujar el fondo
        int pos = (rand() % 40960)+4096;
        u8 b = rand();
        u8 r = MIN((rand() & 255)>>1,b) & 255;

        r = r & 15;
        b = MIN(b & 15,r);
        pixelsTopVRAM[pos] = (0x8000 | (b+(r<<10)));
    }
    
    while(1){
        //esperar a cualquier input
        scanKeys();
        if(keysDown()){break;}
    }
}