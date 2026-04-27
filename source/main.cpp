/*
    ADVERTENCIA: este será el código con más bitshifts y comentarios inecesarios que verás, suerte tratando de entender algo!
     -Alfombracitario, Septiembre de 2025
*/

/*
    To-Do list (para v1.0):
    reordenamiento de código
    añadir figuras (dos pasos)
        rectangulo
        circulo
        línea

    select tool
    move

    añadir gradientes (16bpp)
    
    swap de pantallas
    selector desde pantalla de arriba
    
    Añadir settings
        invertir colores
        index swap (colores)
        cambiar hue global
        reducir colores (posterización)

    añadir soporte a imagenes más grandes (cargando pedazos)
*/
#include <nds.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>

#include <font.h>

#include "timers.h"
#include "files.h"
#include "avdslib.h"
#include "intro.h"//intro global para todos mis juegos


#include "GFXinput.h"
#include "GFXconsoleInput.h"
#include "GFXselector24.h"
#include "GFXselector16.h"
#include "GFXnewImageInput.h"
#include "GFXbackground.h"
#include "GFXmore.h"
#include "GFXbrushSettings.h"
#include "GFXselector8.h"
//Macros
#define SCREEN_W 256
#define SCREEN_H 192
#define SURFACE_X 64
#define SURFACE_Y 0
#define SURFACE_W 128
#define SURFACE_H 128

#define C_WHITE 65535
#define C_RED 32799
#define C_YELLOW 33791
#define C_GREEN 33760
#define C_CYAN 65504
#define C_BLUE 64512
#define C_PURPLE 64543
#define C_BLACK 32768
#define C_GRAY 48623

#define STYLUSHOLDTIME 15

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define MAX_ALPHA 63

//OAM
#define selector24oamID 64
#define brushSettingsOamId 24
#define brushSettingsSelectorOamId 22
#define selector16oamId 65

#define surfaceSize 128*128

#define APP_PATH "/_nds/apixds/"
#define CACHE_PATH  APP_PATH "cache/"
#define ANIM_TEMP  CACHE_PATH "animation.temp"
#define ANIM_TEMP_NEW CACHE_PATH "animation_new.temp"
//===================================================================Variables================================================================
static PrintConsole topConsole;
//static PrintConsole bottomConsole;

//IWRAM, 32KB usados
u16 surface[16384]__attribute__((section(".iwram"))); // 32 KB en IWRAM

//RAM 360,960 bytes usados (en arrays)
u16* pixelsTopVRAM = (u16*)BG_GFX;
u16* pixelsVRAM = (u16*)BG_GFX_SUB;
u16* bgPreviewGfx = NULL;
u16 *gfx32;
u16 *gfx16;
u16 *gfxBG;
u16 pixelsTop[surfaceSize];//copia en RAM (es más rápida)
u16 pixels[49152];
u16 palette[256];

u16 stack[surfaceSize];// para operaciones temporales
u16 backup[131072];//para undo/redo y cargar imagenes 256kb 

u16 gradientTable[SCREEN_H];

u8 paletteAlpha = MAX_ALPHA;//indicador del alpha actual, útil para 16bpp
//ideal añadir un array para guardar más frames

touchPosition touch;

int backupMax = 8;
int backupSize = surfaceSize<<1;

int backupIndex = -1;       // índice del último frame guardado
int oldestBackup = 0;       // límite inferior (el frame más antiguo que aún es válido)
int totalBackups = 0;       // cuántos backups se han llenado realmente

//paletas
int paletteSize = 256;
int palettePos = 0;
int paletteBpp = 8;

int paletteOffset = 0;
int bucketMode;
bool nesMode = false;
bool usesPages = false;
u8 palEdit[3];
const u16 nesPalette[64] = {
    0xbdef, 0xd804, 0xdc05, 0xd04c, 0xbc93, 0x9856, 0x80d4, 0x810f,
    0x8169, 0x81a7, 0x81a7, 0xa186, 0xc146, 0x8000, 0x8000, 0x8000,
    0xdef7, 0xfd88, 0xfd08, 0xf912, 0xe11b, 0xb11b, 0x815c, 0x81d8,
    0x8231, 0x828a, 0x8aa9, 0xb689, 0xe248, 0x8000, 0x8000, 0x8000,
    0xffff, 0xfe8c, 0xfe0a, 0xfdd4, 0xfd9e, 0xd99f, 0x99ff, 0x829f,
    0x935d, 0x83b3, 0xa3ce, 0xcb8e, 0xf34c, 0xb18c, 0x8000, 0x8000,
    0xffff, 0xff52, 0xfef4, 0xfed8, 0xfedc, 0xf6ff, 0xdf3f, 0xd37f,
    0xcbdf, 0xc3d9, 0xd3d4, 0xe7f4, 0xfbf4, 0xd294, 0x8000, 0x8000
};
//otras variables
int imgFormat = 0;
int prevtpx = 0;
int prevtpy = 0;

int prevx = 0;
int prevy = 0;

int subSurfaceXoffset = 0;
int subSurfaceYoffset = 0;
int subSurfaceZoom = 3;// 8 veces más cerca
int sleepTimer = 60;//frames para que se duerma (baje FPS)
int sleepingFrames = 1;//solo espera un frame
int stylusHoldTimer = STYLUSHOLDTIME;
bool stylusRepeat = false;
//Exponentes
//máximo es 7, por favor, no pongas un número mayor a 7.
int surfaceXres = 7;
int surfaceYres = 7;

int stackYres = 7;
int stackXres = 7;

int resX = 7;
int resY = 7;
u8 palEditSel = 0;
u32 kDown = 0;
u32 kHeld = 0;
u32 kUp = 0;

u32 frameStartTime = 0;
u32 frameEndTime = 0;

bool stylusPressed = false;
bool showGrid = false;
bool drew = false;
bool updated = false;
bool accurate = false;
bool both = true;//actualizar ambas pantallas
bool mayus = false;
int holdTimer = 0;
int fileOffset = 0;
int gridSkips = 0;
bool rPressed = false;
bool screensSwapped = false;
bool showBrushSettings = false;
bool preview = true;
bool redraw = true;

// Variables globales para controlar el modo actual
enum subMode { SUB_TEXT, SUB_BITMAP };
subMode currentSubMode = SUB_BITMAP;

enum consoleMode { MODE_NO, LOAD_file, SAVE_file, IMAGE_SETTINGS, MODE_NEWIMAGE};
consoleMode currentConsoleMode = MODE_NO;

//animación
struct Animation {
    u16 frames      : 15 = 0;
    u16 isPlaying   : 1  = 0;
    u16 pos              = 0;
    u8  speed            = 2;//en frames
};
Animation animation;

enum {
    ACTION_NONE      = 0,
    ACTION_UP        = 1 << 0,
    ACTION_DOWN      = 1 << 1,
    ACTION_LEFT      = 1 << 2,
    ACTION_RIGHT     = 1 << 3,
    ACTION_ZOOM_IN   = 1 << 4,
    ACTION_ZOOM_OUT  = 1 << 5,
};

typedef enum {
    TOOL_BRUSH,
    TOOL_ERASER,
    TOOL_BUCKET,
    TOOL_PICKER
} ToolType;

ToolType currentTool = TOOL_BRUSH; // por defecto

typedef enum {
    BRUSH_MODE_NORMAL = 0,
    BRUSH_MODE_DITHER,
    BRUSH_MODE_VLINES,
    BRUSH_MODE_HLINES,
} BrushMode;

typedef enum {
    BRUSH_SIZE_1 = 0,
    BRUSH_SIZE_2,
    BRUSH_SIZE_3,
    BRUSH_SIZE_4
} BrushSize;

typedef enum {
    BRUSH_TYPE_RECTANGLE_HOLLOW = 0,
    BRUSH_TYPE_RECTANGLE_FILLED,
    BRUSH_TYPE_LINE,
    BRUSH_TYPE_CIRCLE
} BrushType;

BrushMode brushMode = BRUSH_MODE_NORMAL;
BrushSize brushSize = BRUSH_SIZE_1;

ConsoleFont font = {
    .gfx = fontTiles,
    .pal = fontPal,
    .numColors = fontPalLen>>1,
    .bpp = 4,
    .asciiOffset = 32,
    .numChars = fontTilesLen>>5,
};

// FUNCIONES
//función para pasar copiar datos rápidamente
extern "C" void memcpy_fast_arm9(const void* src, void* dst, unsigned int bytes);

void submitVRAM(bool full = false, bool _accurate = false, bool both = true)
{
    const u32 sizeBottom = full ? 98304 : 65536;
    const u32 sizeTop    = surfaceSize << 1; // bytes

    u16* srcTop    = pixelsTop;
    u16* dstTop    = pixelsTopVRAM;
    u16* srcBottom = pixels;
    u16* dstBottom = pixelsVRAM;

    // Flush solo de fuente, no destino (VRAM no necesita flush de destino)
    if (_accurate) {
        if (both) DC_FlushRange(srcTop, sizeTop);
        DC_FlushRange(srcBottom, sizeBottom);
    }

    // Detener canales
    DMA2_CR = 0;
    DMA3_CR = 0;

    if (both) {
        // Configurar DMA2 (top) — NO activar aún
        DMA2_SRC  = (u32)srcTop;
        DMA2_DEST = (u32)dstTop;
        // DMA3 bottom simultáneo
        DMA3_SRC  = (u32)srcBottom;
        DMA3_DEST = (u32)dstBottom;

        // Activar ambos en rápida sucesión para mayor solapamiento
        DMA2_CR = (sizeTop  >> 1) | DMA_ENABLE;
        DMA3_CR = (sizeBottom >> 1) | DMA_ENABLE;

        while ((DMA2_CR & DMA_ENABLE) || (DMA3_CR & DMA_ENABLE));
    } else {
        DMA3_SRC  = (u32)srcBottom;
        DMA3_DEST = (u32)dstBottom;
        DMA3_CR   = (sizeBottom >> 1) | DMA_ENABLE;

        while (DMA3_CR & DMA_ENABLE);
    }
}

static void vblank_handler(void)
{
    // Stop the previous DMA copy
    dmaStopSafe(0);

    BG_PALETTE[0] = gradientTable[0];

    dmaSetParams(0,
                 &gradientTable[1],
                 &BG_PALETTE[0], // Write to the background color
                 DMA_SRC_INC | // Autoincrement source after each copy
                 DMA_DST_FIX | // Keep destination fixed
                 DMA_START_HBL | // Start copy at the start of horizontal blank
                 DMA_REPEAT | // Don't stop DMA after the first copy.
                 DMA_COPY_HALFWORDS | 1 | // Copy one halfword each time
                 DMA_ENABLE);
}

void initGradient(){
    for (int i = 0; i < 32; i++) {
        int r = 15 - i;
        if (r < 0) r = 0;

        int b = (31 - i) >> 1;
        u16 color = (b << 10) | r;
        gradientTable[i] = color;
        gradientTable[SCREEN_H-i] = color;
    }
    irqSet(IRQ_VBLANK, vblank_handler);//configurar HDMA
}


static inline bool brushPatternPass(int x, int y, BrushMode mode)
{
    switch(mode)
    {
        case BRUSH_MODE_NORMAL:
            return true;

        case BRUSH_MODE_HLINES:
            return !(y & 1);

        case BRUSH_MODE_VLINES:
            return !(x & 1);

        case BRUSH_MODE_DITHER:
            return ((x ^ y) & 1) == 0;
    }

    return true;
}
inline void drawPixelSurface(int x, int y, u16 color)
{
    if((unsigned)x < 1<<surfaceXres &&
       (unsigned)y < 1<<surfaceYres &&
       brushPatternPass(x, y, brushMode))
    {
        surface[(y <<surfaceXres) + x] = color;
    }
}
inline u16 mergeColorAlpha(u16 oldCol, u16 color, u8 alpha)
{
    if (alpha > MAX_ALPHA) alpha = MAX_ALPHA;

    u8 r1 = (oldCol >> 10) & 31;
    u8 g1 = (oldCol >> 5)  & 31;
    u8 b1 =  oldCol        & 31;

    u8 r  = (color >> 10) & 31;
    u8 g  = (color >> 5)  & 31;
    u8 b  =  color        & 31;

    int r2 = (r * alpha + r1 * (MAX_ALPHA - alpha)) / MAX_ALPHA;
    int g2 = (g * alpha + g1 * (MAX_ALPHA - alpha)) / MAX_ALPHA;
    int b2 = (b * alpha + b1 * (MAX_ALPHA - alpha)) / MAX_ALPHA;

    // --- corrección de estancamiento ---
    if (alpha > 0)
    {
        if (r2 == r1) r2 += (r > r1) - (r < r1);
        if (g2 == g1) g2 += (g > g1) - (g < g1);
        if (b2 == b1) b2 += (b > b1) - (b < b1);
    }

    return (r2 << 10) | (g2 << 5) | b2 | 0x8000;
}

inline void drawPixelSurfaceAlpha(int x, int y, u16 color)
{
    if((unsigned)x < 1<<surfaceXres &&
       (unsigned)y < 1<<surfaceYres &&
       brushPatternPass(x, y, brushMode))
    {
        int index = (y << surfaceXres)+x;
        surface[index] = mergeColorAlpha(surface[index],color,paletteAlpha);
    }
}
inline void brushStamp2(int x,int y,u16 color)
{
    if(paletteAlpha != MAX_ALPHA){
        drawPixelSurfaceAlpha(x  ,y  ,color);
        drawPixelSurfaceAlpha(x+1,y  ,color);
        drawPixelSurfaceAlpha(x  ,y+1,color);
        drawPixelSurfaceAlpha(x+1,y+1,color);
    }
    else{
        drawPixelSurface(x  ,y  ,color);
        drawPixelSurface(x+1,y  ,color);
        drawPixelSurface(x  ,y+1,color);
        drawPixelSurface(x+1,y+1,color);
    }
}

inline void brushStamp3Square(int x,int y,u16 color)
{
    if(paletteAlpha != MAX_ALPHA){
        for(int oy=-1; oy<=1; oy++)
        for(int ox=-1; ox<=1; ox++)
            drawPixelSurfaceAlpha(x+ox,y+oy,color);
    }
    else{
        for(int oy=-1; oy<=1; oy++)
        for(int ox=-1; ox<=1; ox++)
            drawPixelSurface(x+ox,y+oy,color);
    }
}

inline void brushStamp3Circle(int x,int y,u16 color)
{
    if(paletteAlpha != MAX_ALPHA){
        drawPixelSurfaceAlpha(x  ,y  ,color);
        drawPixelSurfaceAlpha(x+1,y  ,color);
        drawPixelSurfaceAlpha(x  ,y+1,color);
        drawPixelSurfaceAlpha(x-1,y,color);
        drawPixelSurfaceAlpha(x,y-1,color);
    }
    else{
        drawPixelSurface(x  ,y  ,color);
        drawPixelSurface(x+1,y  ,color);
        drawPixelSurface(x  ,y+1,color);
        drawPixelSurface(x-1,y,color);
        drawPixelSurface(x,y-1,color);
    }

}
inline void brushStamp4Circle(int x,int y,u16 color)
{
    if(paletteAlpha != MAX_ALPHA){
        drawPixelSurfaceAlpha(x+1,y-1 ,color);
        drawPixelSurfaceAlpha(x  ,y-1 ,color);
        drawPixelSurfaceAlpha(x-1,y+1 ,color);
        drawPixelSurfaceAlpha(x-1,y  ,color);
        drawPixelSurfaceAlpha(x  ,y  ,color);
        drawPixelSurfaceAlpha(x  ,y+1,color);
        drawPixelSurfaceAlpha(x  ,y+2,color);
        drawPixelSurfaceAlpha(x+1,y+1,color);
        drawPixelSurfaceAlpha(x+1,y+2,color);
        drawPixelSurfaceAlpha(x+2,y+1,color);
        drawPixelSurfaceAlpha(x+1,y  ,color);
        drawPixelSurfaceAlpha(x+2,y  ,color);
    }
    else{
        drawPixelSurface(x+1,y-1 ,color);
        drawPixelSurface(x  ,y-1 ,color);
        drawPixelSurface(x-1,y+1 ,color);
        drawPixelSurface(x-1,y  ,color);
        drawPixelSurface(x  ,y  ,color);
        drawPixelSurface(x  ,y+1,color);
        drawPixelSurface(x  ,y+2,color);
        drawPixelSurface(x+1,y+1,color);
        drawPixelSurface(x+1,y+2,color);
        drawPixelSurface(x+2,y+1,color);
        drawPixelSurface(x+1,y  ,color);
        drawPixelSurface(x+2,y  ,color);
    }

}

inline void brushStamp4Square(int x,int y,u16 color)
{
    if(paletteAlpha != MAX_ALPHA){
        for(int oy = -1; oy <= 2; oy++)
        for(int ox = -1; ox <= 2; ox++)
            drawPixelSurfaceAlpha(x + ox, y + oy, color);
    }
    else{
        for(int oy = -1; oy <= 2; oy++)
        for(int ox = -1; ox <= 2; ox++)
            drawPixelSurface(x + ox, y + oy, color);
    }

}

//decidir el patrón y tamaño de pincel
inline void brushStamp(int x,int y,u16 color)
{
    bool forceSquare = (brushMode == BRUSH_MODE_DITHER);

    switch(brushSize)
    {
        case BRUSH_SIZE_1:
            if(paletteAlpha != MAX_ALPHA){
                drawPixelSurfaceAlpha(x,y,color);
            }else{drawPixelSurface(x,y,color);}
        break;

        case BRUSH_SIZE_2:
            brushStamp2(x,y,color);
        break;

        case BRUSH_SIZE_3:
            if(forceSquare)
                brushStamp3Square(x,y,color);
            else
                brushStamp3Circle(x,y,color);
        break;

        case BRUSH_SIZE_4:
            if(forceSquare)
                brushStamp4Square(x,y,color);
            else
                brushStamp4Circle(x,y,color);
        break;
    }
}

void drawLineSurface(int x0, int y0, int x1, int y1, u16 color){
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        brushStamp(x0,y0,color);
        
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void drawLineSurfaceAlpha(int x0, int y0, int x1, int y1, u16 color){
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        brushStamp(x0,y0,color);

        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

inline void drawGrid(u16 color){
    int separation = 1<<(subSurfaceZoom+gridSkips);
    int rep = (128>>subSurfaceZoom)>>gridSkips;
    for(int i = 0; i < rep; i++)
    {
        AVdrawHline(pixels,64,192,i*separation,color);
        AVdrawVline(pixels,0,128,64+(i*separation),color);
    }
}

//=========================================================DRAW SURFACE========================================================================
inline void drawSurfaceMain(bool full = true)
{
    both = true;
    int xres = 1 << surfaceXres;
    int yres = 1 << surfaceYres;

    if (xres == 7 && paletteBpp == 16) {
        dmaCopy(surface, pixelsTop, 128 << surfaceYres);
        return;
    }

    u16* dst = pixelsTop;
    
    if (paletteBpp == 16) {
        const u16* src = surface;
        for (int i = 0; i < yres; i++, dst += 128, src += xres) {
            memcpy(dst, src, xres<<1);
        }
    }
    else {
        const u16* pal = palette + paletteOffset;
        const u16*  src = surface;
        for (int i = 0; i < yres; i++, dst += 128, src += xres) {
            const u16* row = src;
            for (int j = 0; j < xres; j++) {
                dst[j] = pal[row[j]];
            }
        }
    }
}
extern "C" void __attribute__((target("arm"))) drawSurfaceBottom2x(u16*, u16*, int, int);
extern "C" void __attribute__((target("arm"))) drawSurfaceBottom4x(u16*, u16*, int, int);
extern "C" void __attribute__((target("arm"))) drawSurfaceBottom8x(u16*, u16*, int, int);
extern "C" void __attribute__((target("arm"))) drawSurfaceBottom16x(u16*, u16*, int, int);
extern "C" void __attribute__((target("arm"))) drawSurfaceBottom32x(u16*, u16*, int, int);

__attribute__((optimize("jump-tables")))
void drawSurfaceBottom(){
    updated = true;

    u16 *srcBase = pixelsTop + ((subSurfaceYoffset) << 7) + (subSurfaceXoffset);
    u16 *dstBase = pixels + 64; // centrado

    switch (subSurfaceZoom) {
        case 0://si no hay zoom
            for (int i = 0; i < 128; i++) {//sí, siempre copiar las 128, es para arreglar errores visuales
                u16* src = pixelsTop + (i<< 7);
                u16* dst = pixels + ((i << 8) + 64);
                memcpy_fast_arm9(src, dst, 256);
            }
            break;
        
        //256: tamaño horizontal (en bytes) del layer 3 de la capa de arriba
        //512: lo mismo pero abajo
        case 1: drawSurfaceBottom2x(srcBase, dstBase, 256, 512); break;
        case 2: drawSurfaceBottom4x(srcBase, dstBase, 256, 512); break;
        case 3: drawSurfaceBottom8x(srcBase, dstBase, 256, 512); break;
        case 4: drawSurfaceBottom16x(srcBase, dstBase,256, 512); break;
        case 5: drawSurfaceBottom32x(srcBase, dstBase,256, 512); break;

        default:
            // fórmula general
            {
                int blockSize = 128 >> subSurfaceZoom;
                int yrepeat   = 1 << subSurfaceZoom;
                int xoffset   = subSurfaceXoffset;
                int yoffset   = subSurfaceYoffset;

                int dstY = 0;

                for (int i = 0; i < blockSize; i++) {
                    int srcY = i + yoffset;
                    u16* srcRow = pixelsTop + (srcY << 8) + xoffset;

                    for (int k = 0; k < yrepeat; k++) {
                        u16* dstRow = pixels + (dstY << 8) + 64;

                        for (int j = 0; j < 128; j++) {
                            dstRow[j] = srcRow[j >> subSurfaceZoom];
                        }
                        dstY++;
                    }
                }
            }
            break;
    }
    if(showGrid){drawGrid(AVinvertColor(palette[paletteOffset]));
        accurate = true;
    }
    
}

void drawColorPalette()
{
    for(int i = 0; i < 16; i++)//vertical
    {
        for(int j = 0; j < 16; j++)//horizontal
        {
            AVdrawRectangle(pixels,192+(j<<2),4,64+(i<<2),4,palette[(i<<4)+j]);
        }
    }
    updated = true;
}

void drawNesPalette()
{
    AVdrawRectangle(pixels,192,64,32,16,0);
    //dibujar paleta
    for(int i = 0; i < 4; i++)//vertical
    {
        for(int j = 0; j < 16; j++)//horizontal
        {
            AVdrawRectangle(pixels,192+(j<<2),4,48+(i<<2),4,nesPalette[(i<<4)+j]);
        }
    }
    updated = true;
}

//==================== PALETAS ==========|
void updatePal(int increment, int *palettePos)
{
    //primero debemos saber si estamos en un rango válido
    if(*palettePos + increment < 0 || *palettePos + increment > paletteSize){
        return;
    }
    updated = true;
    
    //obtenemos la coordenada de la paleta
    int posx = *palettePos & 15;
    int posy = *palettePos >>4;

    u16 _col = palette[*palettePos];

    //reparamos el slot anterior
    AVdrawRectangleHollow(pixels,192+(posx<<2),4,64+(posy<<2),4,_col);

    //nueva información
    *palettePos+=increment;

    _col = palette[*palettePos];
    //obtener cada color RGB
    u8 r = (_col & 31);
    u8 g = (_col & 992)>>5;
    u8 b = (_col & 31744)>>10;
    u8 _barColAmount[3] = {r, g, b};
    for(int i = 0; i < 3; i++)palEdit[i] = _barColAmount[i];

    if(nesMode == false)
    {
        //rectangulos de abajo
        u16 _barCol[3] = { C_RED, C_GREEN, C_BLUE };

        for(int i = 0; i < 3; i++)
        {
            AVdrawRectangle(pixels,192,_barColAmount[i]<<1,(i<<3)+40,8,_barCol[i]);//barra de color
            AVdrawRectangle(pixels,192+(_barColAmount[i]<<1),64-(_barColAmount[i]<<1),(i<<3)+40,8,C_BLACK);//area negra de fondo
        }

        AVdrawRectangle(pixels,192,paletteAlpha,32,8,_col);//rectángulo de arriba (color mezclado)
    }

    //obtenemos la coordenada de la paleta (otra vez)
    posx = *palettePos & 15;
    posy = *palettePos >>4;

    if(paletteBpp != 8)
    {
        int prevOffset = paletteOffset;
        if(paletteBpp == 4){
            paletteOffset = posy<<4;
        }
        else if(paletteBpp == 2){
            paletteOffset = (posy<<4)+((posx>>2)<<2);
        }
        if(prevOffset != paletteOffset){//se cambió la paleta, necesita actualizar la pantalla
            drawSurfaceMain(true);
            drawSurfaceBottom();
        }
    }
    AVdrawRectangleHollow(pixels,192+(posx<<2),4,64+(posy<<2),4,AVinvertColor(_col));//dibujamos el nuevo contorno
}

void updatePalEditBar(int index) {
    int amount = palEdit[index];

    // Actualizar barra
    const u16 _barCol[3] = { C_RED, C_GREEN, C_BLUE };
    AVdrawRectangle(pixels, 192, amount << 1, (index << 3) + 40, 8, _barCol[index]);
    // Limpiar el área
    AVdrawRectangle(pixels, 192 + (amount << 1), 64 - (amount << 1), (index << 3) + 40, 8, C_BLACK);

    // Actualizar el color
    u16 _col = palEdit[0];
    _col += palEdit[1] << 5;
    _col += palEdit[2] << 10;
    _col += 32768; // encender bit alpha
    palette[palettePos] = _col;

    AVdrawRectangle(pixels, 192 + ((palettePos & 15) << 2), 4, 64 + ((palettePos >> 4) << 2), 4, _col);

    if (paletteBpp != 16) {
        drawSurfaceMain(true);
        drawSurfaceBottom();
        AVdrawRectangle(pixels, 192, MAX_ALPHA, 32, 8, _col);
    } else {
        updated = true;
        AVdrawRectangle(pixels, 192, MAX_ALPHA, 32, 8, _col);
        AVdrawRectangle(pixels, 192 + paletteAlpha, 64 - paletteAlpha, 32, 8, 0);
        if (palettePos == 0 && showGrid == true) {
            drawGrid(AVinvertColor(_col));
        }
    }

    // Contorno del color seleccionado
    _col = AVinvertColor(_col);
    AVdrawRectangleHollow(pixels, 192 + ((palettePos & 15) << 2), 4, 64 + ((palettePos >> 4) << 2), 4, _col);
}

//=================================================================Inicialización===================================================================================|
inline void clearTop(){
    memset(pixelsTop,0,surfaceSize<<1);
}
inline void clearPal(){
    for(int i = 0; i < paletteSize; i++) {
        palette[i] = C_BLACK;
    }
}
inline void clearAll(){
    clearTop();
    clearPal();
    for(int i = 0; i < surfaceSize; i++)
    {
        surface[i] = 0;
        stack[i] = 0;
        pixelsTopVRAM[i] = 0;
    }
}
void initBitmap()
{
    clearAll();
    for(int i = 0; i < 49152; i++)
    {
        pixels[i] = 0;
        pixelsVRAM[i] = 0;
    }
    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    consoleInit(&topConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 4, true, true);
    consoleSetFont(&topConsole, &font);
    oamClear(&oamSub, 0, 128);
    bgInit(3, BgType_Bmp16, BgSize_B16_128x128, 0, 0);

    videoSetModeSub(MODE_5_2D);             // pantalla inferior bitmap
    vramSetBankC(VRAM_C_SUB_BG);
    vramSetBankD(VRAM_D_SUB_SPRITE); // sprites en VRAM D
    bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    // Cargar imagen inicial en VRAM
    decompress(GFXinputBitmap, BG_GFX_SUB, LZ77Vram);

    // Transferir pixels a la RAM
    dmaCopyHalfWords(3,pixelsVRAM,pixels , 49152 * sizeof(u16));

    drawSurfaceMain(true);
    drawSurfaceBottom();
    accurate = true;

    bgSetScale(3,256,256);
    bgSetScroll(3, -64, -32);
    bgUpdate();
}
u16 *gfxBrushSettings;
u16 *gfx8;
void setBrushSettingsSprites(bool on){
    showBrushSettings = on;
    oamSet(&oamSub, brushSettingsOamId,//
    16, 32,//posición
        0,//prioridad
        on,//opaco
        SpriteSize_32x32, SpriteColorFormat_Bmp,
        gfxBrushSettings,
        -1,
    false, false, false, false, false);

    oamSet(&oamSub, brushSettingsSelectorOamId,//
    ((int)brushMode<<3)+16, 32,//posición
        0,//prioridad
        on,//opaco
        SpriteSize_8x8, SpriteColorFormat_Bmp,
        gfx8,
        -1,
    false, false, false, false, false);
    oamSet(&oamSub, brushSettingsSelectorOamId+1,//
    ((int)brushSize<<3)+16, 40,//posición
        0,//prioridad
        on,//opaco
        SpriteSize_8x8, SpriteColorFormat_Bmp,
        gfx8,
        -1,
    false, false, false, false, false);
    oamUpdate(&oamSub);
}

void setEditorSprites(){
    //iniciamos el sprite para dibujar : )
    oamInit(&oamMain, SpriteMapping_Bmp_1D_128, false);
    oamInit(&oamSub, SpriteMapping_Bmp_1D_128, false);

    oamClear(&oamMain, 0, 128);
    oamClear(&oamSub, 0, 128);

    u16 *gfx32 = oamAllocateGfx(&oamSub, SpriteSize_32x32, SpriteColorFormat_Bmp);
    dmaCopy(GFXselector24Bitmap, gfx32, 32*32*2);
    u16 *gfx16 = oamAllocateGfx(&oamSub, SpriteSize_16x16, SpriteColorFormat_Bmp);
    dmaCopy(GFXselector16Bitmap, gfx16, 16*16*2);

            oamSet(&oamSub, selector24oamID,
            0, 16,
            0,
            15, // opaco
            SpriteSize_32x32, SpriteColorFormat_Bmp,
            gfx32,
            -1,
            false, false, false, false, false);
    
    gfxBrushSettings = oamAllocateGfx(&oamSub, SpriteSize_32x32, SpriteColorFormat_Bmp);
    dmaCopy(GFXbrushSettingsBitmap, gfxBrushSettings, 32*32*2);

    gfx8 = oamAllocateGfx(&oamSub, SpriteSize_8x8, SpriteColorFormat_Bmp);
    dmaCopy(GFXselector8Bitmap, gfx8, 8*8*2);

    setBrushSettingsSprites(true);
}

void setOamBG(){
u16 *gfxBG = oamAllocateGfx(&oamSub, SpriteSize_32x32, SpriteColorFormat_Bmp);
dmaCopy(GFXbackgroundBitmap, gfxBG, 32*32*2);

for(int i = 0; i < 16; i++){
    int x = ((i & 0b11)<<5)+SURFACE_X;
    int y = (i & 0b1100)<<3;

    oamSet(&oamSub, i+4,//index
    x, y,//posición
        1,
        15, // opaco
        SpriteSize_32x32, SpriteColorFormat_Bmp,
        gfxBG,
        -1,
    false, false, false, false, false);
}
//fondo para las paletas
oamSet(&oamSub, 20,//index
    192, 32,//posición
        1,
        15, // opaco
        SpriteSize_32x32, SpriteColorFormat_Bmp,
        gfxBG,
        -1,
    false, false, false, false, false);
oamSet(&oamSub, 21,//index
    224, 32,//posición
        1,
        15, // opaco
        SpriteSize_32x32, SpriteColorFormat_Bmp,
        gfxBG,
        -1,
    false, false, false, false, false);
}
//====================================================================Compatibilidad con modos gráficos====================================|
void textMode()
{
    if(currentSubMode == SUB_TEXT) return; // ya estamos en texto
    currentSubMode = SUB_TEXT;
    //matamos ambas pantallas por que... why not :)

    videoSetMode(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    consoleInit(&topConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
    consoleSetFont(&topConsole, &font);
    oamClear(&oamSub, 0, 128);
    oamUpdate(&oamSub);
    sleepingFrames = 1;
    sleepTimer = 60;
}
int bgPreview;
void textKeyboardDraw(){
    //añadir capa de preview
    bgPreview = bgInitSub(3, BgType_Bmp16, BgSize_B16_128x128,4,0);
    bgPreviewGfx = bgGetGfxPtr(bgPreview);
    dmaFillHalfWords(0, bgPreviewGfx, 128 * 128 * 2);

    int bg2 = bgInitSub(2, BgType_Bmp8, BgSize_B8_256x256, 0, 0);

    decompress(GFXconsoleInputBitmap, bgGetGfxPtr(bg2), LZ77Vram);
    dmaCopy(GFXconsoleInputPal, BG_PALETTE_SUB, GFXconsoleInputPalLen);

    bgSetScale(bgPreview, 296, 296);
    bgSetScroll(bgPreview, 0, 0);
    bgSetPriority(bgPreview, 0); // 0 = mayor prioridad
    bgSetPriority(bg2, 1);
    bgUpdate();

    printf(fname);
    printf(texts[selectorA]);
    printf("\n????????????????????????????????\n");
    listFiles();
    redraw = false;
}
void bitmapMode() 
{
    if(currentSubMode == SUB_BITMAP) return; // ya estamos en bitmap
    currentSubMode = SUB_BITMAP;

    //reiniciamos VRAM
    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    //vramSetBankB(VRAM_B_MAIN_SPRITE); // sprites en VRAM B
    int bgMain = bgInit(3, BgType_Bmp16, BgSize_B16_128x128, 0, 0);
    consoleInit(&topConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 4, true, true);
    consoleSetFont(&topConsole, &font);
    oamClear(&oamSub, 0, 128);
    
    videoSetModeSub(MODE_5_2D);             // pantalla inferior bitmap
    vramSetBankC(VRAM_C_SUB_BG);
    vramSetBankD(VRAM_D_SUB_SPRITE); // sprites en VRAM D
    bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    setEditorSprites();
    paletteAlpha = MAX_ALPHA;
    submitVRAM(true,true,true);//recuperamos nuestros queridos datos
    if(paletteBpp == 16){
        setOamBG();
    }
    bgSetScale(7,256,256);
    bgSetScroll(7, 0, 0);
    bgSetScale(bgMain,256,256);
    bgSetScroll(bgMain, -64, -32);
    bgUpdate();
    oamUpdate(&oamSub);
}
char bucketText[2][6] = {"Color","Index"};
void drawInfo(){
    u32 ticks = frameEndTime - frameStartTime;
    u32 cpuUsage = ((u64)(ticks) * 11732)>>16;
    consoleClear();
    if(animation.frames != 0){printf("frame: %d / %d \nanim speed: %d",animation.pos,animation.frames,animation.speed);}
    else{
        printf("\n\n");
    }
    if(bucketMode != 0){
        printf("\nBucket: replace %s", bucketText[bucketMode-1]);
    }
    else{
        printf("\n");
    }
    printf("\n%d",fps);
    printf("\n%lu",cpuUsage);
}
//====================================================================Backups==============================================================|
void setBackupVariables(){
    backupIndex = 0;
    backupSize = 1<<surfaceXres<<surfaceYres;
    backupMax = 131072>>surfaceXres>>surfaceYres;
    //reinicia el backup
    for(int i = 0; i < 131072; i++){
        backup[i] = 0;
    }
}
void backupWrite(){
    backupIndex++;
    if(backupIndex >= backupMax){
        backupIndex = 0;
    }
    int index = backupIndex*backupSize;
    //copia surface a backup+ su index
    dmaCopyHalfWords(2, surface, backup + index, backupSize * sizeof(u16));
}
void backupRead(){
    // Calculamos el índice del bloque en el array backup
    int index = backupIndex * backupSize;
    dmaCopyHalfWords(2, backup + index, surface, backupSize * sizeof(u16));
}
//============================================================= SD CARD ===============================================|
void createAppFolder(){
    mkdir("/_nds/", 0777);
    mkdir(APP_PATH, 0777);
}
void clearCache() {
    // abrir el directorio
    DIR* dir = opendir(CACHE_PATH);
    if (dir) {
        struct dirent* entry;
        char filepath[256];
        
        while ((entry = readdir(dir)) != NULL) {
            // ignorar . y ..
            if (entry->d_name[0] == '.') continue;
            
            snprintf(filepath, sizeof(filepath), "%s%s", CACHE_PATH, entry->d_name);
            remove(filepath);
        }
        closedir(dir);
    }
    
    // recrear por si no existía
    mkdir(CACHE_PATH, 0777);
}

bool saveArray(const char *path, void *array, size_t size) {
    FILE *file = fopen(path, "wb");  // write binary
    if (!file) return false;
    fwrite(array, 1, size, file);
    fclose(file);
    return true;
}

bool loadArray(const char *path, void *array, size_t size) {
    FILE *file = fopen(path, "rb");  // read binary
    if (!file) return false;
    fread(array, 1, size, file);
    fclose(file);
    return true;
}
// ===================================================== ANIMATION ========================================== |
#define PALETTE_SIZE (256 * 2)  // 512 bytes, fijo siempre

// Tamaño total de un bloque en disco (píxeles + paleta)
static inline int blockSize() {
    return (2 << surfaceXres << surfaceYres) + PALETTE_SIZE;
}

void loadAnimFrame(u16* surface) {
    FILE* f = fopen(ANIM_TEMP, "rb");
    if (!f) return;

    int pixSize  = 2 << surfaceXres << surfaceYres;
    int blkSize  = pixSize + PALETTE_SIZE;

    fseek(f, (long)animation.pos * blkSize, SEEK_SET);
    fread(surface, 1, pixSize, f);          // píxeles
    fread(palette,  1, PALETTE_SIZE, f);    // paleta del frame
    fclose(f);
}

void saveAnimFrame() {
    FILE* f = fopen(ANIM_TEMP, "r+b");
    if (!f) f = fopen(ANIM_TEMP, "wb");
    if (!f) return;

    int pixSize = 2 << surfaceXres << surfaceYres;
    int blkSize = pixSize + PALETTE_SIZE;

    fseek(f, (long)animation.pos * blkSize, SEEK_SET);
    fwrite(surface, 1, pixSize,     f);     // píxeles
    fwrite(palette,  1, PALETTE_SIZE, f);   // paleta del frame
    fclose(f);
}

void nextAnimFrame() {
    saveAnimFrame();
    if (animation.pos >= animation.frames) {
        animation.pos = 0;
    } else {
        animation.pos++;
    }
    loadAnimFrame(surface);
    drawColorPalette();
    updatePal(0,&palettePos);
    drawSurfaceMain(true); drawSurfaceBottom(); accurate = true;
}

void deleteAnimFrame() {
    if (animation.frames <= 0) return;

    int pixSize = 2 << surfaceXres << surfaceYres;
    int blkSize = pixSize + PALETTE_SIZE;

    // Último frame: truncar
    if (animation.pos >= animation.frames) {
        FILE* f = fopen(ANIM_TEMP, "ab");
        if (!f) return;
        ftruncate(fileno(f), (long)(animation.frames - 1) * blkSize);
        fclose(f);
        animation.frames--;
        if (animation.pos >= animation.frames) animation.pos = animation.frames;
        return;
    }

    FILE* src = fopen(ANIM_TEMP, "rb");
    FILE* dst = fopen(ANIM_TEMP_NEW, "wb");
    if (!src || !dst) { if (src) fclose(src); if (dst) fclose(dst); return; }

    u8 buffer[blkSize];

    for (int i = 0; i <= animation.frames; i++) {  // frames+1 bloques en disco
        fread(buffer, 1, blkSize, src);
        if (i == animation.pos) continue;           // saltar el frame borrado
        fwrite(buffer, 1, blkSize, dst);
    }

    fclose(src);
    fclose(dst);
    remove(ANIM_TEMP);
    rename(ANIM_TEMP_NEW, ANIM_TEMP);

    animation.frames--;
    if (animation.pos >= animation.frames) animation.pos = animation.frames - 1;
}

void insertAnimFrame() {
    int pixSize = 2 << surfaceXres << surfaceYres;
    int blkSize = pixSize + PALETTE_SIZE;

    // Bloque vacío: píxeles a 0, paleta actual del editor
    u8 empty[blkSize];
    memset(empty, 0, pixSize);
    memcpy(empty + pixSize, palette, PALETTE_SIZE);  // hereda paleta actual

    // Insertar al final: append directo
    if (animation.pos >= animation.frames) {
        FILE* f = fopen(ANIM_TEMP, "ab");
        if (!f) return;
        fwrite(empty, 1, blkSize, f);
        fclose(f);
        animation.frames++;
        nextAnimFrame();
        return;
    }

    FILE* src = fopen(ANIM_TEMP, "rb");
    FILE* dst = fopen(ANIM_TEMP_NEW, "wb");
    if (!src || !dst) { if (src) fclose(src); if (dst) fclose(dst); return; }

    u8 buffer[blkSize];

    for (int i = 0; i <= animation.frames; i++) {
        fread(buffer, 1, blkSize, src);
        fwrite(buffer, 1, blkSize, dst);
        if (i == animation.pos) {
            fwrite(empty, 1, blkSize, dst);  // insertar frame nuevo después del actual
        }
    }

    fclose(src);
    fclose(dst);
    remove(ANIM_TEMP);
    rename(ANIM_TEMP_NEW, ANIM_TEMP);

    animation.frames++;
    nextAnimFrame();
}

void playAnimation() {
    if (animation.frames < 1) return;

    int pixSize = 2 << surfaceXres << surfaceYres;
    int sw = 1 << surfaceXres;
    int sh = 1 << surfaceYres;

    while (animation.isPlaying) {
        animation.pos++;
        if (animation.pos > animation.frames) animation.pos = 0;

        loadAnimFrame(stack);   // carga píxeles a stack Y actualiza palette[]
        DC_FlushRange(stack, pixSize);

        for (int i = 0; i < animation.speed; i++) {
            scanKeys();
            if (keysDown()) animation.isPlaying = false;
            timerStop();
            swiWaitForVBlank();
            timerContinue();
        }

        frameEndTime = timerRead(); updateFPS(); drawInfo(); timerReset();
        frameStartTime = timerRead();

        if (paletteBpp == 16) {
            for (int y = 0; y < sh; y++) {
                u16* dst = pixelsTopVRAM + (y << 7);
                u16* src = stack         + (y << surfaceXres);
                dmaCopy(src, dst, sw * 2);
            }
        } else {
            // palette[] ya fue actualizado por loadAnimFrame
            for (int y = 0; y < sh; y++) {
                u16* dst = pixelsTopVRAM + (y << 7);
                u16* src = stack         + (y << surfaceXres);
                for (int x = 0; x < sw; x++) {
                    dst[x] = palette[src[x]];
                }
            }
        }
    }
}
//============================================================= INPUT =================================================|
inline int getActionsFromKeys(int keys) {
    int actions = ACTION_NONE;

    if(keys & KEY_UP)    actions |= ACTION_UP;
    if(keys & KEY_DOWN)  actions |= ACTION_DOWN;
    if(keys & KEY_LEFT)  actions |= ACTION_LEFT;
    if(keys & KEY_RIGHT) actions |= ACTION_RIGHT;
    if(keys & KEY_A)     actions |= ACTION_ZOOM_IN;
    if(keys & KEY_B)     actions |= ACTION_ZOOM_OUT;

    return actions;
}

int getActionsFromTouch(int button) {
    int actions = ACTION_NONE;

    switch(button) {
        case 1: actions |= ACTION_LEFT; break;
        case 2: actions |= ACTION_RIGHT; break;
        case 5: actions |= ACTION_UP; break;
        case 6: actions |= ACTION_DOWN; break;
        case 0: actions |= ACTION_ZOOM_IN; break;
        case 4: actions |= ACTION_ZOOM_OUT; break;

        case 3://showGrid
            showGrid = !showGrid;
            drawSurfaceBottom();
        break;
        case 7:
            //screensSwapped = true;
            //lcdSwap();
        break;
    }
    return actions;
}

void applyActions(int actions) {//acciones compartidas entre teclas y touch
    accurate = true;//queremos que se renderize todo correctamente
    int blockSize = (1 << surfaceXres) >> subSurfaceZoom;   // tamaño de bloque en píxeles según el zoom
    if(kHeld & KEY_L || kHeld & KEY_X) {
        // --- Scroll por bloques ---
        if(actions & ACTION_UP)    subSurfaceYoffset -= blockSize;
        if(actions & ACTION_DOWN)  subSurfaceYoffset += blockSize;
        if(actions & ACTION_LEFT)  subSurfaceXoffset -= blockSize;
        if(actions & ACTION_RIGHT) subSurfaceXoffset += blockSize;

        if(actions & ACTION_ZOOM_IN && gridSkips < surfaceXres) {gridSkips++;}
        if(actions & ACTION_ZOOM_OUT && gridSkips > 0) {gridSkips--;}
    }
    else {
        // --- Scroll por píxeles ---
        if(actions & ACTION_UP)    subSurfaceYoffset--;
        if(actions & ACTION_DOWN)  subSurfaceYoffset++;
        if(actions & ACTION_LEFT)  subSurfaceXoffset--;
        if(actions & ACTION_RIGHT) subSurfaceXoffset++;

        if(actions & ACTION_ZOOM_IN) {subSurfaceZoom++;}
        if(actions & ACTION_ZOOM_OUT && subSurfaceZoom > 0) {subSurfaceZoom--;}
    }
    // --- Limitar dentro de la superficie ---
    blockSize = (1 << surfaceXres) >> subSurfaceZoom;
    int maxX      = (1 << surfaceXres) - blockSize;         // límite máximo en X
    int maxY      = (1 << surfaceYres) - blockSize;         // límite máximo en Y
    if(subSurfaceXoffset < 0)     subSurfaceXoffset = 0;
    if(subSurfaceYoffset < 0)     subSurfaceYoffset = 0;
    if(subSurfaceXoffset > maxX)  subSurfaceXoffset = maxX;
    if(subSurfaceYoffset > maxY)  subSurfaceYoffset = maxY;
    // --- Limitar el zoom ---
    int maxRes = MAX(surfaceXres,surfaceYres);
    int minZoom = 7-maxRes;
    if(subSurfaceZoom < minZoom){
            subSurfaceZoom = minZoom;
    }
}
const char *keyboardLower = "1234567890()#@qwertyuiop[]$^asdfghjkl/{}%!|zxcvbnm>>-_~=<<      ,.'`;+";
const char *keyboardUpper = "1234567890()#@QWERTYUIOP[]$^ASDFGHJKL/{}%!|ZXCVBNM>>-_~=<<      ,.'`;+";
char getKeyboardKey(int x, int y){
    //primero arreglamos las teclas a un margen
    x=(x>>4)-1;//dividir en 16

    y = (y>>4)-7;//pasar a un rango de 0 a 5

    //combinar a una sola variable
    x = x+(y*14);

    //convertir el valor a un dato 
    const char *output = mayus ? keyboardUpper : keyboardLower;
    return output[x];
}

void handleKey(char key) {
    int len = strlen(fname);

    switch (key) {
        case '/': // borrar
            if (len > 0) fname[len - 1] = '\0';
            break;

        case '|': // alternar mayúsculas
            mayus = !mayus;
            break;

        case '>': // enter
            fname[0] = '\0';
            kDown = KEY_START;
            break;

        case '<': // salir
            kDown = KEY_SELECT;
            break;

        default:
            // agregar carácter normal
            if (len < 15 && key != '\0') {
                // si mayúsculas activadas y es letra
                if (mayus && key >= 'a' && key <= 'z') {
                    key -= 32; // convierte a mayúscula ('a' → 'A')
                }

                fname[len] = key;
                fname[len + 1] = '\0';
            }
            break;
    }
}
void replaceIndex(u16 *surface, u16 oldColor, u16 newColor){
    //guardar un backup para undo 
    backupWrite();
    //ahora sí reemplazamos todos los indices
    int size  = 1 << surfaceXres<<surfaceYres;
    for(int i = 0; i < size; i++){
        if(surface[i] == oldColor) surface[i] = newColor;
    }
}

void swapIndex(u16 oldIndex, u16 newIndex){
    // Swap en paleta
    u16 tmp = palette[oldIndex];
    palette[oldIndex] = palette[newIndex];
    palette[newIndex] = tmp;

    // Swap de índices en el surface
    int total = 1<<surfaceXres<<surfaceYres;
    for(int i = 0; i < total; i++){
        if(surface[i] == oldIndex) surface[i] = newIndex;
        else if(surface[i] == newIndex) surface[i] = oldIndex;
    }
    //actualizamos ahora todo visualmente
    if(paletteBpp != 16){
        drawSurfaceMain();
        drawSurfaceBottom();
    }
    updatePal(0,&palettePos);
    drawColorPalette();
}
void floodFill(u16 *surface, int x, int y, u16 oldColor, u16 newColor, int xres, int yres) {
    if (oldColor == newColor) return;
    if(bucketMode == 1){
        replaceIndex(surface,oldColor,newColor);
        return;
    }else if(bucketMode == 2){
        swapIndex(oldColor,newColor);
    }
    int width  = 1 << xres;
    int height = 1 << yres;
    if (x < 0 || y < 0 || x >= width || y >= height) return;
    if (surface[(y << xres) + x] != oldColor) return;

    // reinterpretar el stack global como bytes para doble capacidad
    u8 *stack8 = (u8*)stack;
    int maxStack = (sizeof(stack) * 2);
    int sp = 0;

    stack8[sp++] = (u8)x;
    stack8[sp++] = (u8)y;

    while (sp > 0) {
        if (sp < 2) break; // seguridad mínima
        u8 cy = stack8[--sp];
        u8 cx = stack8[--sp];

        int idx = (cy << xres) + cx;
        if (surface[idx] != oldColor) continue;
        surface[idx] = newColor;

        // push vecinos con control de overflow
        if (sp <= maxStack - 8) {
            if (cx + 1 < width  && surface[(cy << xres) + (cx + 1)] == oldColor) { stack8[sp++] = cx + 1; stack8[sp++] = cy; }
            if (cx > 0          && surface[(cy << xres) + (cx - 1)] == oldColor) { stack8[sp++] = cx - 1; stack8[sp++] = cy; }
            if (cy + 1 < height && surface[((cy + 1) << xres) + cx] == oldColor) { stack8[sp++] = cx;     stack8[sp++] = cy + 1; }
            if (cy > 0          && surface[((cy - 1) << xres) + cx] == oldColor) { stack8[sp++] = cx;     stack8[sp++] = cy - 1; }
        } else {
            break; // stack lleno → evita overflow
        }
    }
}

void applyTool(int x, int y, bool dragging) {
    if(x == prevx && y == prevy){return;}
    prevx = x;
    prevy = y;

    u16 color = 0;
    if(paletteBpp != 16){
        color = palettePos - paletteOffset;
    }
    else
    {//si estamos en modo 16 bits
        color = palette[palettePos];
        color = color |0x8000; //forzar alpha
    }

    switch (currentTool) {
        case TOOL_BRUSH:
            if (dragging) {
                if(paletteAlpha != MAX_ALPHA){
                    if(paletteAlpha == 0){
                        drawLineSurface(prevtpx, prevtpy, x, y, 0);
                        break;
                    }
                    drawLineSurfaceAlpha(prevtpx, prevtpy, x, y, color);
                    break;
                }
                drawLineSurface(prevtpx, prevtpy, x, y, color);
            } else {
                if(paletteAlpha == 0){
                    brushStamp(x,y,0);
                    break;
                }
                brushStamp(x,y,color);
                break;
                //modo normal
                brushStamp(x,y,color);
            }
            break;

        case TOOL_ERASER:
            if (dragging) {
                drawLineSurface(prevtpx, prevtpy, x, y, 0);
            } else {
                drawPixelSurface(x,y,0);
            }
            break;

        case TOOL_PICKER:
            if(paletteBpp == 16){
                palette[palettePos] = surface[(y << surfaceXres) + x];
                //paletteAlpha = MAX_ALPHA;
                updatePal(0,&palettePos);
            }
            else{
                updatePal(surface[(y << surfaceXres) + x]-color,&palettePos);
            }
                currentTool = TOOL_BRUSH;//volver a seleccionar el pincel
                oamSetXY(&oamSub,selector24oamID,0,16);
                oamUpdate(&oamSub);
            break;


        case TOOL_BUCKET:
            if(paletteAlpha != MAX_ALPHA){
                if(paletteAlpha == 0){
                    floodFill(surface, x, y, surface[(y << surfaceXres) + x], 0, surfaceXres, surfaceYres);
                    break;
                }
                u16 _col = mergeColorAlpha(surface[(y << surfaceXres) + x], color, paletteAlpha);
                floodFill(surface, x, y, surface[(y << surfaceXres) + x], _col, surfaceXres, surfaceYres);
                break;
            }
            else{
                floodFill(surface, x, y, surface[(y << surfaceXres) + x], color, surfaceXres, surfaceYres);
            }
            break;
    }
}
//========================================= Herramientas extras=====================|
//copia en el stack una parte de la imagen
void copyFromSurfaceToStack()
{
    int zoom = subSurfaceZoom - (7 - MAX(surfaceXres, surfaceYres));

    stackXres = surfaceXres - zoom;
    stackYres = surfaceYres - zoom;

    int stackW   = 1 << stackXres;
    int stackH   = 1 << stackYres;
    int rowBytes = stackW << 1; // sizeof(u16) = 2

    int baseOffset = subSurfaceXoffset +
                    (subSurfaceYoffset << surfaceXres);

    u16* src = surface + baseOffset;
    u16* dst = stack;

    int surfaceStride = 1 << surfaceXres;

    // en este caso copiamos todo de una ya que los bits están alineados
    if (stackW == surfaceStride)
    {
        memcpy(dst, src, rowBytes * stackH);
        return;
    }

    // Copia normal por filas
    for (int y = stackH; y--; )
    {
        memcpy(dst, src, rowBytes);

        dst += stackW;
        src += surfaceStride;
    }
}
void cutFromSurfaceToStack(){
    //copia pero limpia un fragmento de la pantalla
    //en vez de optimizar esto, lo haré de la manera más simple posible lol
    copyFromSurfaceToStack();
    //limpiar la pantalla
    int blockSize = 128 >> subSurfaceZoom;
    AVdrawRectangleDMA(surface,subSurfaceXoffset,blockSize,subSurfaceYoffset,blockSize,0,surfaceXres);
}

void pasteFromStackToSurface()
{
    int ysize = 1 << stackYres;
    int xsize = 1 << stackXres;

    for(int i = 0; i < ysize; i++) // eje vertical
    {
        int y = (i << stackXres); // fila en el stack
        int _y = ((i + subSurfaceYoffset) << surfaceXres) + subSurfaceXoffset; // fila en surface con offset

        for(int j = 0; j < xsize; j++)
        {
            surface[_y + j] = stack[y + j];
        }
    }
}

//para paletas
inline void copyPalette(){
    int iterations = (paletteBpp >= 8) ? 256 : (1 << paletteBpp);
    for(int i = 0; i < iterations;i++){
        stack[i] = palette[i+paletteOffset];
    }
}

inline void pastePalette(){
    int iterations = (paletteBpp >= 8) ? 256 : (1 << paletteBpp);
    for(int i = 0; i < iterations;i++){
        palette[i+paletteOffset] = stack[i];
    }
}

inline void copyColor(){
    stack[0] = palette[palettePos+paletteOffset];
}

inline void pasteColor(){
    palette[palettePos+paletteOffset] = stack[0];
}

void flipH(){
    copyFromSurfaceToStack();

    int ysize = 1 << stackYres;
    int xsize = 1 << stackXres;

    for (int i = 0; i < ysize; i++) // eje vertical
    {
        int y = (i << stackXres); // fila en el stack
        int _y = ((i + subSurfaceYoffset) << surfaceXres) + subSurfaceXoffset; // fila en surface con offset

        for (int j = 0; j < xsize; j++)
        {
            surface[_y + j] = stack[y + (xsize - 1 - j)];
        }
    }
}

 
void flipV()
{
    copyFromSurfaceToStack();
    int ysize = 1 << stackYres;
    int xsize = 1 << stackXres;

    for(int i = 0; i < ysize; i++) // eje vertical
    {
        int y = (((ysize-1)-i) << stackXres); // fila en el stack
        int _y = ((i + subSurfaceYoffset) << surfaceXres) + subSurfaceXoffset; // fila en surface con offset

        for(int j = 0; j < xsize; j++)
        {
            surface[_y + j] = stack[y + j];
        }
    }
}

void scaleUp(){
    copyFromSurfaceToStack();

    int stackW = 1 << stackXres;
    int stackH = 1 << stackYres;

    //felicidades, encontraste la peor línea que verás en tu vida!
    if((((((stackH-1)<<1)+1)+subSurfaceYoffset)<<surfaceXres)+((stackW-1)<<1)+subSurfaceXoffset > (1<<surfaceXres<<surfaceYres)){return;} //fuera de rango

    for(int sy = 0; sy < stackH; ++sy){
        int srcBase = sy * stackW;

        // dos filas destino correspondientes a esta fila fuente
        int dstRow0 = ((sy<<1) + subSurfaceYoffset) << surfaceXres;
        int dstRow1 = (((sy<<1) + 1) + subSurfaceYoffset) << surfaceXres;

        for(int sx = 0; sx < stackW; ++sx){
            u16 pix = stack[srcBase + sx];
            int dstCol = (sx<<1) + subSurfaceXoffset;

            // escribir 2x2
            surface[dstRow0 + dstCol]     = pix;
            surface[dstRow0 + dstCol + 1] = pix;
            surface[dstRow1 + dstCol]     = pix;
            surface[dstRow1 + dstCol + 1] = pix;
        }
    }
}

#define A_MASK 0x8000
#define R_MASK 0x7C00
#define G_MASK 0x03E0
#define B_MASK 0x001F

void scaleDown(){
    cutFromSurfaceToStack();
    int baseOffset = subSurfaceXoffset+(subSurfaceYoffset<<surfaceXres);
    int _y = 0; int offset = 0;
    if(paletteBpp != 16){
        //lee el stack saltandose un pixel
        int yres = (1<<stackYres)>>1;
        int xres = (1<<stackXres)>>1;
        for(int y = 0; y < yres; y++){
            //precalcular algunas cosas
            offset = (y<<surfaceXres)+baseOffset;
            _y = (y << 1) << stackXres;

            for(int x = 0; x < xres; x++){//dibujar
                surface[offset+x] = stack[_y+(x<<1)];
            }
        }
    }
    else{
    int yres = (1 << stackYres) >> 1;
    int xres = (1 << stackXres) >> 1;

    for(int y = 0; y < yres; y++){
        offset = (y << surfaceXres) + baseOffset;

        int row0 = (y << 1) << stackXres;
        int row1 = row0 + (1 << stackXres);

        for(int x = 0; x < xres; x++){
            int sx = x << 1;

            u16 p0 = stack[row0 + sx];
            u16 p1 = stack[row0 + sx + 1];
            u16 p2 = stack[row1 + sx];
            u16 p3 = stack[row1 + sx + 1];

            // extraer canales
            int r =
                ((p0 & R_MASK) >> 10) +
                ((p1 & R_MASK) >> 10) +
                ((p2 & R_MASK) >> 10) +
                ((p3 & R_MASK) >> 10);

            int g =
                ((p0 & G_MASK) >> 5) +
                ((p1 & G_MASK) >> 5) +
                ((p2 & G_MASK) >> 5) +
                ((p3 & G_MASK) >> 5);

            int b =
                (p0 & B_MASK) +
                (p1 & B_MASK) +
                (p2 & B_MASK) +
                (p3 & B_MASK);

            // promedio 
            r >>= 2;
            g >>= 2;
            b >>= 2;

            // alpha: activo si alguno lo tiene
            u16 a = (p0 | p1 | p2 | p3) & A_MASK;

            surface[offset + x] =
                a |
                (r << 10) |
                (g << 5) |
                b;
            }
        }
    }
    accurate = true;
}

void rotatePositive() { // 90° Antihorario
    copyFromSurfaceToStack();

    int size = 1 << stackXres;
    int baseOffset = subSurfaceXoffset + (subSurfaceYoffset << surfaceXres);

    for (int y = 0; y < size; y++) {
        int destOffset = baseOffset + (y << surfaceXres);
        for (int x = 0; x < size; x++) {
            // (x, y) → (y, size-1-x)
            surface[destOffset + x] = stack[((size - 1 - x) << stackXres) + y];
        }
    }
}

void rotateNegative() { // 90° Horario
    copyFromSurfaceToStack();

    int size = 1 << stackXres;
    int baseOffset = subSurfaceXoffset + (subSurfaceYoffset << surfaceXres);

    // Para rotar horario, leer desde cuadrante "inferior" hacia la derecha
    for (int y = 0; y < size; y++) {
        int destOffset = baseOffset + (y << surfaceXres);
        for (int x = 0; x < size; x++) {
            // leer desde cuadrante rotado (x, y) → (size-1-y, x)
            surface[destOffset + x] = stack[(x << stackXres) + (size - 1 - y)];
        }
    }
}

void shiftDownWrap() {
    copyFromSurfaceToStack();

    int width  = 1 << stackXres;
    int height = 1 << stackYres;

    // Guardar última fila en la primera
    memcpy(
        stack,
        stack + ((height - 1) << stackXres),
        width * sizeof(u16)
    );

    // Mover filas hacia abajo
    for(int y = height - 1; y > 0; y--)
    {
        int current = y << stackXres;
        int prev    = (y - 1) << stackXres;

        memcpy(
            stack + current,
            stack + prev,
            width * sizeof(u16)
        );
    }

    pasteFromStackToSurface();
}

void shiftUpWrap() {
    copyFromSurfaceToStack();

    int width  = 1 << stackXres;
    int height = 1 << stackYres;

    // Guardar primera fila en la última
    memcpy(
        stack + ((height - 1) << stackXres),
        stack,
        width * sizeof(u16)
    );

    // Mover filas hacia arriba
    for(int y = 0; y < height - 1; y++)
    {
        int current = y << stackXres;
        int next    = (y + 1) << stackXres;

        memcpy(
            stack + current,
            stack + next,
            width * sizeof(u16)
        );
    }

    pasteFromStackToSurface();
}

void shiftRightWrap()
{
    copyFromSurfaceToStack();

    int width  = 1 << stackXres;
    int height = 1 << stackYres;

    for(int y = 0; y < height; y++)
    {
        int row = y << stackXres;

        u16 last = stack[row + width - 1];

        for(int x = width - 1; x > 0; x--)
        {
            stack[row + x] = stack[row + x - 1];
        }

        stack[row] = last;
    }

    pasteFromStackToSurface();
}

void shiftLeftWrap() {
    copyFromSurfaceToStack();

    int width  = 1 << stackXres;
    int height = 1 << stackYres;

    for(int y = 0; y < height; y++)
    {
        int row = y << stackXres;

        u16 first = stack[row];

        for(int x = 0; x < width - 1; x++)
        {
            stack[row + x] = stack[row + x + 1];
        }

        stack[row + width - 1] = first;
    }

    pasteFromStackToSurface();
}
//====================================================================MAIN==================================================================================================================|
int main(void) {
    defaultExceptionHandler();// Mostrar crasheos
    // Intentar montar la SD
    // Intentar montar primero con DLDI (para flashcards DS/DS Lite)
    bool sd_ok = fatInitDefault();
    if(sd_ok) {
        // Verificar si estamos en una flashcard o DSi
        // En flashcards, la raíz suele ser "fat:/"
        // En DSi, es "sd:/"
        
        // Intentar cambiar a fat:/ primero (flashcards)
        if(chdir("fat:/") == 0) {
            currentDir = opendir(".");
        }
        // Si falla, intentar sd:/ (DSi)
        else if(chdir("sd:/") == 0) {
            currentDir = opendir(".");
        }
        // Como último recurso, usar la raíz actual
        else {
            currentDir = opendir(".");
        }        
        createAppFolder();
        clearCache();
    }

    if (!sd_ok) {
        // --- Inicializar video temporalmente en modo consola (pantalla superior) ---
        videoSetMode(MODE_0_2D);                // modo texto

        // poner pantalla inferior en modo texto temporal
        videoSetModeSub(MODE_0_2D);
        vramSetBankC(VRAM_C_SUB_BG);
        consoleDemoInit();

        printf("\x1b[31m\n");//Rojo

        printf("ERROR: SD CARD NOT INITIATED.\n");
        printf("\x1b[38m\n");//blanco
        printf("You cannot load or save files.\n");
        printf("This error may occur on flashcards if DLDI\n");
        printf("patching was not done correctly.\n");

        printf("\nStarting in 3 seconds");
        for (int i = 0; i < 3; i++)//cantidad segundos
        {
            printf(".");
            for(int j = 0; j < 60; j++)
            {
                swiWaitForVBlank();
            }
        }
    }
    //antes de iniciar el programa, mostramos la intro
    intro();

    initBitmap();
    setEditorSprites();
    setBackupVariables();
    initFPS();
    initGradient();
    initTimers();
    //aclarar la pantalla
    for(int i = 0; i < 16; i++)
    {
        setBrightness(3, i-15);
        swiWaitForVBlank();
    }
    //========================================================================WHILE LOOP!!!!!!!!!==========================================|
    while(1) {
        //input
        scanKeys();
        kDown = keysDown();
        kHeld = keysHeld();
        kUp = keysUp();
        touchRead(&touch);
        int actions = ACTION_NONE;

        if(currentSubMode == SUB_BITMAP)
        {   
            frameEndTime = timerRead();
            drawInfo();
            timerReset();
            frameStartTime = timerRead();
            //if(screensSwapped == false){
                if(kUp & KEY_TOUCH){//permitir volver a dibujar en un pixel
                    prevx = -1;
                    prevy = -1;
                    stylusHoldTimer = STYLUSHOLDTIME;
                    stylusRepeat = false;
                }
                if(kHeld & KEY_L || kHeld & KEY_X)//zoom y offsets
                {
                    actions |= getActionsFromKeys(kDown);
                }
                else//paleta de colores
                {
                    if(kHeld & KEY_SELECT){//selector
                        if(kDown & (KEY_UP | KEY_DOWN)){
                            if(kDown & KEY_UP){
                                palEditSel--;
                            }else{
                                palEditSel++;
                            }
                            palEditSel &= 3;
                            if(paletteBpp != 16 && palEditSel == 0){
                                palEditSel = 1;
                            }
                            //draw the rectangle with the selected bar.
                            AVdrawRectangle(pixels,254,2,32,32,C_BLACK);
                            AVdrawRectangle(pixels,254,2,32+(palEditSel<<3),8,C_YELLOW);
                            updated = true;
                            goto frameEnd;//you can only use one input per frame, nothing more to check here!
                        }
                        if(kDown & (KEY_RIGHT|KEY_LEFT)){
                            if(palEditSel != 0){
                                u8 index = palEditSel-1;
                                if(kDown & KEY_RIGHT)
                                    palEdit[index]++;
                                else
                                    palEdit[index]--;
                                
                                palEdit[index] &= 31;
                                updatePalEditBar(index);
                            }
                            else{
                                if(kDown & KEY_RIGHT)
                                    paletteAlpha++;
                                else
                                    paletteAlpha--;
                                
                                paletteAlpha &= MAX_ALPHA;
                                //el color no cambia, pero la barra sí
                                AVdrawRectangle(pixels,192,paletteAlpha,32,8,palette[palettePos]);
                                //Limpiar el area
                                AVdrawRectangle(pixels,192+paletteAlpha,64-paletteAlpha,32,8,0);
                            }
                        updated = true;
                        goto frameEnd;
                        }
                    }
                    else{
                        if(kDown & KEY_DOWN)
                        {
                            updatePal(16, &palettePos);
                            goto frameEnd;
                        }
                        if(kDown & KEY_UP)
                        {
                            updatePal(-16, &palettePos);
                            goto frameEnd;
                        }
                        if(kDown & KEY_LEFT)
                        {
                            updatePal(-1, &palettePos);
                            goto frameEnd;
                        }
                        if(kDown & KEY_RIGHT)
                        {
                            updatePal(1, &palettePos);
                            goto frameEnd;
                        }
                    }
                }
                if(kDown & KEY_R || kDown & KEY_Y){
                    showGrid = !showGrid;
                    drawSurfaceBottom();
                    goto frameEnd;
                }
                //===========================================PALETAS=========================================================
                palettePos = palettePos & (paletteSize-1);//mantiene la paleta dentro de un límite
                if(palettePos < 0){palettePos = 0;}
                //recordar que debo hacer cambios dependiendo del bpp
                if (kHeld & KEY_TOUCH) {
                    if (stylusHoldTimer > 0) {
                        stylusHoldTimer--;
                    } else {
                        stylusRepeat = true;
                    }
                    if (touch.px >= SURFACE_X && touch.px < (SURFACE_W + SURFACE_X)) {//TOUCH EN EL CENTRO!
                        if(touch.py <= SURFACE_H){//APUNTA A LA SURFACE!
                            int localX = touch.px - SURFACE_X;
                            int localY = touch.py;

                            int srcX = subSurfaceXoffset + (localX >> subSurfaceZoom);
                            int srcY = subSurfaceYoffset + (localY >> subSurfaceZoom);

                            if(srcY < 1<<surfaceYres)//comprobar si está en el rango (solo por si acaso)
                            {
                                // --- ejecutar la herramienta seleccionada ---
                                applyTool(srcX, srcY, stylusPressed);

                                prevtpx = srcX;
                                prevtpy = srcY;

                                drawSurfaceMain(false);
                                drawSurfaceBottom();
                                drew = true;
                                stylusPressed = true;
                                goto frameEnd;
                            }
                        }else{//apunta a los botones de abajo
                            int row = (touch.py-SURFACE_H)>>4;
                            int col = (touch.px-SURFACE_X)>>4;
                            //PLACEHOLDER
                            if(row == 3 && stylusPressed == false){
                                stylusPressed = true;
                                switch(col){
                                    case 0://delete frame
                                        deleteAnimFrame();
                                    break;

                                    case 1://add frame
                                        insertAnimFrame();
                                    break;

                                    case 2://prev frame
                                        saveAnimFrame();
                                        if(animation.pos <= 0){
                                            animation.pos = animation.frames;
                                        }else{
                                            animation.pos--;
                                        }
                                        loadAnimFrame(surface);
                                        drawSurfaceMain(true);drawSurfaceBottom();accurate = true;
                                    break;

                                    case 3://play animation
                                        animation.isPlaying = true;
                                        playAnimation();
                                    break;
                                    
                                    case 5://next frame
                                        nextAnimFrame();
                                    break;

                                    case 6://less speed
                                        if(animation.speed > 1) animation.speed--;
                                    break;

                                    case 7://more speed
                                        animation.speed++;
                                    break;
                                }
                            }
                            goto frameEnd;
                        }
                    }
                    if(touch.px < 64)//apunta a la parte izquierda
                    {
                        if(touch.px < 48 && touch.py > 16 && touch.py < 64 && stylusPressed == false)//herramientas
                        {
                            if(showBrushSettings && touch.px >= 16 && touch.px < 48 && touch.py >= 32 && touch.py < 48){//si está en modo configurar brush
                                int col = (touch.px - 16)>>3;
                                int row = (touch.py - 32)>>3;
                                stylusPressed = true;
                                switch(row){
                                    case 0://patrones
                                        brushMode = (BrushMode)col;
                                    break;

                                    case 1://tamaños
                                        brushSize = (BrushSize)col;
                                    break;
                                }
                                setBrushSettingsSprites(true);
                                goto frameEnd;
                            }
                            int col = touch.px > 24 ? 1 : 0;
                            int row = touch.py > 40 ? 2 : 0;
                            //convertir col+row a un valor único
                            ToolType prevTool = currentTool;
                            currentTool = (ToolType)(row + col);
                            if(currentTool == prevTool && currentTool == TOOL_BUCKET){
                                bucketMode++;
                                if(bucketMode > 2){
                                    bucketMode = 0;
                                }
                            }
                            if(currentTool == TOOL_BRUSH){
                                setBrushSettingsSprites(true);
                            }else{
                                setBrushSettingsSprites(false);
                            }
                            //además dibujamos un contorno en dónde seleccionamos
                            oamSetXY(&oamSub,selector24oamID,col*24, (row*12)+16);
                            oamUpdate(&oamSub);
                            stylusPressed = true;
                            goto frameEnd;
                        }
                        else//apunta a otra parte de la izquierda
                        {
                            if(touch.py < 16 && stylusPressed == false){//iconos de la parte superior
                                int selected = touch.px>>4;
                                fname[0] = '\0';//quitar nombre reciente :>
                                //actualizar input para que la pantalla también lo haga
                                kDown = kDown | KEY_TOUCH;
                                switch(selected)
                                {
                                    case 0: //load file
                                        textMode();
                                        currentConsoleMode = LOAD_file;
                                        textKeyboardDraw();
                                    goto textConsole;
                                    case 1: //New file
                                        textMode();
                                        currentConsoleMode = MODE_NEWIMAGE;
                                        decompress(GFXnewImageInputBitmap, BG_GFX_SUB, LZ77Vram);
                                    goto textConsole;

                                    case 2:// Save file
                                        textMode();
                                        currentConsoleMode = SAVE_file;
                                        textKeyboardDraw();
                                    goto textConsole;

                                    //PLACEHOLDER el caso 3 está libre por ahora :> (pienso usarlo para configuración en un futuro)
                                }
                            }
                            //botones del costado derecho en la izquierda
                            else if(touch.px >= 48 && touch.py < 64 && stylusPressed == false){
                                int selected = touch.py>>4;
                                switch(selected)//puro hardcode lol
                                {
                                    case 1://Copy
                                        copyFromSurfaceToStack();
                                    break;
                                    case 2://cut
                                        cutFromSurfaceToStack();
                                        drawSurfaceMain(false);drawSurfaceBottom();
                                    break;
                                    case 3://Paste
                                        pasteFromStackToSurface();
                                        drawSurfaceMain(true);drawSurfaceBottom();
                                    break;
                                }
                                stylusPressed = true;
                                goto frameEnd;
                            }
                            if(touch.py >= 64 && stylusPressed == false)//revisar botones inferiores
                            {
                                //hardcodeado porque lol
                                int selected = touch.px>>4;
                                selected += ((touch.py-64)>>4)<<2;
                                stylusPressed = true;
                                switch(selected)
                                {
                                    case 0://rotate -90°
                                        rotateNegative();
                                        drawSurfaceMain(false);drawSurfaceBottom();
                                    goto frameEnd;

                                    case 1://rotate 90°
                                        rotatePositive();
                                        drawSurfaceMain(false);drawSurfaceBottom();
                                    goto frameEnd;

                                    case 2:
                                        flipV();
                                        drawSurfaceMain(false);drawSurfaceBottom();
                                    goto frameEnd;

                                    case 3:
                                        flipH();
                                        drawSurfaceMain(false);drawSurfaceBottom();
                                    goto frameEnd;

                                    case 6:
                                        //verificar si es posible escalar
                                        scaleUp();
                                        drawSurfaceMain(true);drawSurfaceBottom();
                                    goto frameEnd;

                                    case 7:
                                        scaleDown();
                                        drawSurfaceMain(false);drawSurfaceBottom();
                                    goto frameEnd;

                                    case 8:
                                        shiftLeftWrap();
                                        drawSurfaceMain(false);drawSurfaceBottom();
                                    goto frameEnd;

                                    case 9:
                                        shiftRightWrap();
                                        drawSurfaceMain(false);drawSurfaceBottom();
                                    goto frameEnd;

                                    case 10:
                                        shiftUpWrap();
                                        drawSurfaceMain(false);drawSurfaceBottom();
                                    goto frameEnd;

                                    case 11:
                                        shiftDownWrap();
                                        drawSurfaceMain(false);drawSurfaceBottom();
                                    goto frameEnd;

                                    case 24: // Page UP
                                    case 25: // Page DOWN
                                    {
                                        if(usesPages){
                                            saveFile(imgFormat, currentFilePath, palette, surface);

                                            int dir = (selected == 24) ? -1 : +1;
                                            fileOffset += dir * (paletteBpp << 11);

                                            loadFile(imgFormat, currentFilePath, palette, surface);
                                            drawSurfaceMain(true);drawSurfaceBottom();accurate = true;
                                        }
                                    }
                                    goto frameEnd;


                                    case 26: //undo
                                        backupIndex--;
                                        if(backupIndex < 0){
                                            backupIndex = backupMax;
                                        }
                                        backupRead();
                                        drawSurfaceMain(true);drawSurfaceBottom();
                                    goto frameEnd;
                                    
                                    case 27: //redo
                                        backupIndex++;
                                        if(backupIndex > backupMax){
                                            backupIndex = 0;
                                        }
                                        backupRead();
                                        drawSurfaceMain(true);drawSurfaceBottom();
                                    goto frameEnd;

                                }
                            }
                        }
                    }
                    //zona de paletas y otras configuraciones
                    if(touch.px >= 192)//apunta a la parte derecha
                    {
                        if (touch.py < 32 && (stylusPressed == false || stylusRepeat == true)) { // botones superiores
                            int col = (touch.px - 192) >> 4;   // 0..3
                            int row = touch.py >> 4;           // 0..1
                            if ((unsigned)col < 4 && (unsigned)row < 2) {
                                int button = (row << 2) | col; // 0..7
                                actions |= getActionsFromTouch(button);
                            }
                            stylusPressed = true;
                            goto frameEnd;
                        }
                        else if(touch.py < 40 && touch.py > 32 && paletteBpp == 16){//transparencia
                            paletteAlpha = (touch.px-192);

                            AVdrawRectangle(pixels,192,63,32,8,palette[palettePos]);
                            //Limpiar el area
                            AVdrawRectangle(pixels,192+paletteAlpha,64-paletteAlpha,32,8,0);
                            updated = true;
                            goto frameEnd;
                        }
                        else if(touch.py >= 40 && touch.py < 64)//creador de colores
                        {
                            //hay mucho código hardcodeado aquí para mejorar el rendimiento :>
                            if(nesMode){
                                int ystart = 48;
                                int row = (touch.px-192)>>2;
                                int col = (touch.py-ystart)>>2;
                                int index = (col<<4)+row;

                                u16 _col = nesPalette[index];
                                AVdrawRectangle(pixels,192+((palettePos & 15)<<2),4, 64+((palettePos>>4)<<2) ,4,_col);
                                drawSurfaceMain(true);drawSurfaceBottom();

                                palette[palettePos] = _col;
                                //dibujar el contorno del color seleccionado
                                _col = AVinvertColor(_col);
                                AVdrawRectangleHollow(pixels,192+((palettePos & 15)<<2),4, 64+((palettePos>>4)<<2) ,4,_col);
                                goto frameEnd;
                            }else{//creador de colores en modo 
                                int index = (touch.py-40)>>3;
                                palEdit[index] = (touch.px-192)>>1;

                                updatePalEditBar(index);

                                goto frameEnd;
                            }
                        }
                        else if(touch.py > 128){//botones inferior derecha
                            //obtenemos el indice a base de donde apretamos
                            int row = (touch.px-192)>>4;
                            int pos = row;
                            switch(pos){
                                case 0:
                                    copyPalette();
                                break;

                                case 1:
                                    pastePalette();
                                break;

                                case 2:
                                    copyColor();
                                break;

                                case 3:
                                    pasteColor();
                                break;
                            }
                            drawColorPalette();
                            updatePal(0,&palettePos);
                            updated = true;
                            goto frameEnd;
                        }
                        else{//seleccionar un color en la paleta
                            if(stylusPressed == false)
                            {
                                int row = (touch.py-64)>>2;
                                int col = (touch.px-192)>>2;

                                updatePal(((row<<4)+col)-palettePos,&palettePos);   
                                stylusPressed = true;
                            }
                            goto frameEnd;
                        }
                    }
                }
                else{
                    stylusPressed = false;
                }

                frameEnd:
                updateFPS();

                if(kUp & KEY_TOUCH && drew == true){
                    drew = false;
                    backupWrite();
                }
                if (actions != ACTION_NONE) {
                    applyActions(actions);
                    drawSurfaceBottom();
                }
                timerStop();
                //implementación del modo reposo para ahorrar energía
                for(int i = 0; i<sleepingFrames; i++){
                    swiWaitForVBlank();
                }
                timerContinue();
                if(sleepTimer < 0 && sleepingFrames < 4){
                    sleepingFrames++;
                    sleepTimer = (60*(sleepingFrames<<2));//cada vez le toma más frames dormirse
                }
                if(updated){//llamar a submitVRAM solo si se modificó algo visual
                    submitVRAM(true,accurate,both);
                    sleepTimer = 60;
                    sleepingFrames = 1;
                }
                sleepTimer--;
                updated = false;
                accurate = false;
                both = false;
                //fin del loop de modo bitmap (pantalla de abajo)
            //}
            //else{
            //  screenSwapped
            //}
        }
        else//=======================================CONSOLA DE TEXTO=======================================>
        {
            textConsole:
                //comportamiento global.
                if(kUp){holdTimer = 0;}
                if(kDown){
                    holdTimer++;//sí, esta weá está muy simplificada
                }
                //este comentario está dedicado a cualqueira que analice este código, sí, sé lo que hago esto es intencional y tengo razones para hacerlo.
                if(holdTimer > 0){
                    swiWaitForVBlank();
                    scanKeys();
                    kUp = keysUp();
                    if(kUp){holdTimer = 0;goto textConsole;}
                    swiWaitForVBlank();
                    holdTimer++;
                }//espero que hayas disfrutado ver estos crimenes a la programación! sigueme para más contenido como este.

                if(kDown & KEY_SELECT)
                {
                    bitmapMode();//simplemente salir de este menú
                }

                if(currentConsoleMode == MODE_NEWIMAGE)
                {
                    
                    int bpps[5]={2,4,8,2,16};
                    consoleClear();
                    printf("Create new file:\n");
                    printf("Resolution: %d",1<<resX);
                    printf("x%d",1<<resY);
                    printf("\nColors:%d",1<<selectorA);

                    if(selector == 3){
                        printf("\nNes mode");
                    }
                    if(selector == 4){
                        printf("\nDirect color mode");
                    }

                    if(kDown & KEY_RIGHT)
                    {
                        selector++;
                        if(selector > 3){selector = 0;}
                        selectorA = bpps[selector];
                    }
                    else if(kDown & KEY_LEFT)
                    {
                        selector--;
                        if(selector < 0){selector = 3;}
                        selectorA = bpps[selector];
                    }
                    if(kDown & KEY_START || kDown & KEY_A)
                    {
                        nesMode = false;
                        surfaceXres = resX;
                        surfaceYres = resY;
                        paletteBpp = selectorA;
                        subSurfaceZoom = 7-MAX(surfaceXres,surfaceYres);//limitar el zoom
                        //se inicia un nuevo lienzo
                        clearAll();
                        bitmapMode();//simplemente salir de este menú
                        drawColorPalette();
                        if(selector == 3){
                            nesMode = true;
                            drawNesPalette();
                        }
                        selector = 0;
                        goto frameEnd;
                    }
                    //touch input
                    if(kDown & KEY_TOUCH){
                        int x = touch.px;
                        int y = touch.py-48;
                        int option = y>>5;
                        switch(option){

                            case 0://Colores
                                selector = ((x-8)/48);
                                selector = MIN(selector,4);
                                selectorA = bpps[selector];
                            break;

                            case 1://Xres
                                if(x > 64){resX = x>>5;}
                            break;

                            case 2://Yres
                                if(x > 64){resY = x>>5;}
                            break;
                        }
                        
                    }
                }
                else if(currentConsoleMode == SAVE_file || currentConsoleMode == LOAD_file)
                {   
                    if(currentConsoleMode == SAVE_file)
                    {
                        if(kDown & KEY_START)//se guarda el archivo
                        {
                            buildCurrentFilePath();
                            nesMode = false;
                            if(selectorA == formatPAL){
                                saveFile(formatPAL,currentFilePath,palette,surface);
                            }
                            else{
                                imgFormat = selectorA;
                                saveFile(imgFormat,currentFilePath,palette,surface);
                            }
                            bitmapMode();
                            goto frameEnd;
                        }
                    }
                    else{
                        if(kDown & KEY_START)//se carga el archivo
                        {
                            buildCurrentFilePath();
                            clearTop();
                            nesMode = false;
                            if(selectorA == formatPAL){
                                loadFile(formatPAL,currentFilePath,palette,surface);
                            }
                            else{
                                imgFormat = selectorA;
                                loadFile(imgFormat,currentFilePath,palette,surface);
                            }

                            drawSurfaceMain(true);drawSurfaceBottom();
                            drawColorPalette();
                            setBackupVariables();
                            bitmapMode();
                            goto frameEnd;
                        }
                    }
                    //input general

                    if(holdTimer > 10){
                        
                        if(kHeld & KEY_RIGHT){selectorA++;redraw = true;consoleClear();}
                        if(kHeld & KEY_LEFT){selectorA--;redraw = true;consoleClear();}

                        if(kHeld & KEY_UP && selector > 0){selector--;redraw = true;consoleClear();}
                        if(kHeld & KEY_DOWN && selector < fileCount-1){selector++;redraw = true;consoleClear();}
                    }
                    else{
                        if(kDown & KEY_RIGHT){selectorA++;redraw = true;consoleClear();}
                        if(kDown & KEY_LEFT){selectorA--;redraw = true;consoleClear();}

                        if(kDown & KEY_UP && selector > 0){selector--;redraw = true;consoleClear();}
                        if(kDown & KEY_DOWN && selector < fileCount-1){selector++;redraw = true;consoleClear();}
                    }

                    if(selectorA >= MaxFormats){
                        selectorA = 0;
                    }else if(selectorA < 0){
                        selectorA = MaxFormats-1;
                    }

                    strcpy(format,formats[selectorA]);
                    //obtener información del teclado
                    if(kDown & KEY_TOUCH){
                        if(touch.py > 112){
                            redraw = true;consoleClear();
                            char key = getKeyboardKey(touch.px, touch.py);
                            if (key != '\0') handleKey(key);
                        }
                    }
                    if(kDown & KEY_A) {
                        redraw = true;consoleClear();
                        if(selector < 0 || selector >= fileCount) {
                            printf("Error");
                        } else {
                            if(entryList[sortedIdx[selector]].d_type == DT_DIR) {
                            enterFolder(selector);
                        } else {
                            strncpy(fname, entryList[sortedIdx[selector]].d_name, sizeof(fname));
                                format[0] = '\0';
                                kDown = KEY_START; // simula "abrir"
                                goto textConsole;
                            }
                        }
                    }
                    if(kDown & KEY_B) {
                        goBack();
                    }
                    if(redraw){
                        printf(fname);
                        printf(texts[selectorA]);
                        printf("\n????????????????????????????????\n");
                        listFiles();
                        redraw = false;
                    }
                }
        //final del textConsole
        swiWaitForVBlank();
        }
    }
    /*
    Esta sección está hecha para quienes leyeron el código!

    solo voy a decir que estoy trabajando en ordenar un poco este código
    - Alfombra de marzo
    */
   return 0;
}