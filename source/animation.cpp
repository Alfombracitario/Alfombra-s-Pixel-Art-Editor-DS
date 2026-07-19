#include "animation.h"
#include "formatsglobals.h"
#include "timers.h"
#include <unistd.h>


#define PALETTE_SIZE (256 * 2) // 512 bytes, fijo siempre

extern void drawInfo();
extern void drawSurfaceBottom();
extern void drawSurfaceMain();
extern int  updatePal(int increment, int *palettePos);
extern void drawColorPalette();

extern int paletteBpp;
extern int palettePos;
extern u16 *pixelsTopVRAM;
extern u16 *pixelsVRAM;
extern bool accurate;
extern u32 frameStartTime;
extern u32 frameEndTime;

Animation animation;

void loadAnimFrame(u16 *surface){
    FILE *f = fopen(ANIM_TEMP, "rb");
    if (!f)
        return;

    int pixSize = 2 << surfaceXres << surfaceYres;
    int blkSize = pixSize + PALETTE_SIZE;

    fseek(f, (long)animation.pos * blkSize, SEEK_SET);
    fread(surface, 1, pixSize, f);      // píxeles
    fread(palette, 1, PALETTE_SIZE, f); // paleta del frame
    fclose(f);
}

void saveAnimFrame()
{
    FILE *f = fopen(ANIM_TEMP, "r+b");
    if (!f)
        f = fopen(ANIM_TEMP, "wb");
    if (!f)
        return;

    int pixSize = 2 << surfaceXres << surfaceYres;
    int blkSize = pixSize + PALETTE_SIZE;

    fseek(f, (long)animation.pos * blkSize, SEEK_SET);
    fwrite(surface, 1, pixSize, f);      // píxeles
    fwrite(palette, 1, PALETTE_SIZE, f); // paleta del frame
    fclose(f);
}

void nextAnimFrame()
{
    saveAnimFrame();
    if (animation.pos >= animation.frames)
    {
        animation.pos = 0;
    }
    else
    {
        animation.pos++;
    }
    loadAnimFrame(surface);
    drawColorPalette();
    updatePal(0, &palettePos);
    drawSurfaceMain();
    accurate = true;
}

void prevAnimFrame(){
    saveAnimFrame();
    if (animation.pos <= 0)
    {
        animation.pos = animation.frames;
    }
    else
    {
        animation.pos--;
    }
    loadAnimFrame(surface);
    drawColorPalette();
    updatePal(0, &palettePos);
    drawSurfaceMain();
    drawSurfaceMain();
    accurate = true;
}

void deleteAnimFrame()
{
    //primero revisemos si hay frames que eliminar
    if(animation.frames <= 0)
        return;
    //esto va a eliminar el último frame
    animation.pos = animation.frames;

    FILE *f = fopen(ANIM_TEMP, "rb");
    if (!f)
        return;

    int fd = fileno(f);

    int pixSize = 2 << surfaceXres << surfaceYres;
    int blkSize = pixSize + PALETTE_SIZE;

    ftruncate(fd, ((long)animation.pos * blkSize)-1); // deja el archivo en 1024 bytes
    fclose(f);
    animation.frames--;
    animation.pos = animation.frames;
    loadAnimFrame(surface);
    drawColorPalette();
    updatePal(0, &palettePos);
    drawSurfaceMain();
    accurate = true;
}

void insertAnimFrame()
{
    //guardamos el frame actual
    saveAnimFrame();
    //ahora saltamos al final
    animation.frames++; 
    animation.pos = animation.frames;

    //nuevo frame (no limpio la paleta porque es más cómodo así para el usuario)
    dmaFillHalfWords(0, surface, surfaceSize*2);
    //guardo para asegurarme de que no desaparezca de manera misteriosa
    saveAnimFrame();
    drawSurfaceMain();
}

void playAnimation()//solo hace un preview de la animación
{
    int animPos = animation.pos;
    if (animation.frames < 1)
        return;

    //antes de reproducir la animación debemos guardar el frame actual
    saveAnimFrame();

    int pixSize = 2 << surfaceXres << surfaceYres;
    int sw = 1 << surfaceXres;
    int sh = 1 << surfaceYres;

    while(animation.isPlaying)
    {
        animation.pos++;
        if (animation.pos > animation.frames)
            animation.pos = 0;

        loadAnimFrame(stack);
        DC_FlushRange(stack, pixSize);

        for (int i = 0; i < animation.speed; i++)
        {
            scanKeys();
            if (keysDown()){
                animation.isPlaying = false;
                animation.pos = animPos;
                drawSurfaceMain();
            }
                
            timerStop();
            swiWaitForVBlank();
            timerContinue();
        }

        frameEndTime = timerRead();
        updateFPS();
        drawInfo();
        timerReset();
        frameStartTime = timerRead();

        if (paletteBpp == 16)
        {
            for (int y = 0; y < sh; y++)
            {
                u16 *dst = pixelsTopVRAM + (y << 7);
                u16 *src = stack + (y << surfaceXres);
                dmaCopy(src, dst, sw * 2);
            }
        }
        else
        {
            // palette[] ya fue actualizado por loadAnimFrame
            for (int y = 0; y < sh; y++)
            {
                u16 *dst = pixelsTopVRAM + (y << 7);
                u16 *src = stack + (y << surfaceXres);
                for (int x = 0; x < sw; x++)
                {
                    dst[x] = palette[src[x]];
                }
            }
        }
    }
}