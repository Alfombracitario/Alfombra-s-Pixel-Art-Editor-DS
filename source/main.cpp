/*
    ADVERTENCIA: este será el código con más bitshifts y comentarios inecesarios que verás, suerte tratando de entender algo!
     -Alfombracitario, Septiembre de 2025
*/

/*
    To-Do list (para v1.0):
    reordenamiento de código (en proceso)
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
        cambiar hue global
        reducir colores (posterización)
*/
#include <nds.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>

#include <font.h>

#include "timers.h"
#include "textconsole.h"
#include "files.h"
#include "avdslib.h"
#include "intro.h" //intro global para todos mis juegos
#include "animation.h"

#include "GFXinput.h"
#include "GFXconsoleInput.h"
#include "GFXselector24.h"
#include "GFXselector16.h"
#include "GFXnewImageInput.h"
#include "GFXbackground.h"
#include "GFXmore.h"
#include "GFXbrushSettings.h"
#include "GFXselector8.h"
#include "GFXrgbSliders.h"
#include "GFXselector5.h"
#include "GFXrgbSliderSel.h"

// Macros
#define SCREEN_W 256
#define SCREEN_H 192
#define SURFACE_X 64
#define SURFACE_Y 0

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

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define MAX_ALPHA 63

// OAM
#define selector24oamID 64
#define brushSettingsOamId 24
#define brushSettingsSelectorOamId 22
#define selector16oamId 64
#define rgbSliderOamId 66
#define paletteOamId 26
#define paletteSelOamId 25
#define rgbSliderSelOamId 65
#define selectedZoneOamId 100
#define gridOamId 0

#define rgbSliderX SURFACE_X + SURFACE_W
#define rgbSliderY 32

//===================================================================Variables================================================================
static PrintConsole topConsole;
// static PrintConsole bottomConsole;

u16 surface[surfaceSize];//lienzo principal, si pudiera lo metería a dtcm
u16 *pixelsTopVRAM = (u16 *)BG_GFX;
u16 *pixelsVRAM = (u16 *)BG_GFX_SUB;
u16 *bgPreviewGfx = NULL;
u16 pixelsTop[surfaceSize];                         // copia en RAM (es más rápida)
u16 __attribute__((section(".dtcm"))) palette[256]; // ram rápida sin cache miss, perfecto para acceso aleatorio de paletas

u16 stack[surfaceSize]; // para operaciones temporales
u16 backup[BACKUP_SIZE];     // para undo/redo y cargar imagenes 256kb

u16 gradientTable[SCREEN_H];

u16 *gfx32;
u16 *gfx16;
u16 *gfxBG;
u16 *gfxRGBsliders;
u16 *gfxRgbSliderSel;
u16 *gfxPalette;
u16 *gfx5;
u16 *gfxGrid;
u16 *gfxSelectedZone;
u8 paletteAlpha = MAX_ALPHA; // indicador del alpha actual, útil para 16bpp
// ideal añadir un array para guardar más frames

touchPosition touch;

int bgCanvas;
int bgUI;


int backupSize = surfaceSize << 1;
int backupMax = BACKUP_SIZE/backupSize;

int backupIndex = -1; // índice del último frame guardado
int oldestBackup = 0; // límite inferior (el frame más antiguo que aún es válido)
int totalBackups = 0; // cuántos backups se han llenado realmente

// paletas
int paletteSize = 256;
int __attribute__((section(".dtcm"))) palettePos = 0;
int __attribute__((section(".dtcm"))) paletteBpp = 8;

int __attribute__((section(".dtcm"))) paletteOffset = 0;
int bucketMode;
bool hasClipboard = false;
bool nesMode = false;
bool usesPages = false;
u8 __attribute__((section(".dtcm"))) palEdit[3];

const u16 nesPalette[64] = {
    0xbdef, 0xd804, 0xdc05, 0xd04c, 0xbc93, 0x9856, 0x80d4, 0x810f,
    0x8169, 0x81a7, 0x81a7, 0xa186, 0xc146, 0x8000, 0x8000, 0x8000,
    0xdef7, 0xfd88, 0xfd08, 0xf912, 0xe11b, 0xb11b, 0x815c, 0x81d8,
    0x8231, 0x828a, 0x8aa9, 0xb689, 0xe248, 0x8000, 0x8000, 0x8000,
    0xffff, 0xfe8c, 0xfe0a, 0xfdd4, 0xfd9e, 0xd99f, 0x99ff, 0x829f,
    0x935d, 0x83b3, 0xa3ce, 0xcb8e, 0xf34c, 0xb18c, 0x8000, 0x8000,
    0xffff, 0xff52, 0xfef4, 0xfed8, 0xfedc, 0xf6ff, 0xdf3f, 0xd37f,
    0xcbdf, 0xc3d9, 0xd3d4, 0xe7f4, 0xfbf4, 0xd294, 0x8000, 0x8000};

// otras variables
int __attribute__((section(".dtcm"))) prevtpx = 0;
int __attribute__((section(".dtcm"))) prevtpy = 0;
// Exponentes
// máximo es 7, por favor, no pongas un número mayor a 7.
int __attribute__((section(".dtcm"))) surfaceXres = 7;
int __attribute__((section(".dtcm"))) surfaceYres = 7;

int __attribute__((section(".dtcm"))) subSurfaceXoffset = 0;
int __attribute__((section(".dtcm"))) subSurfaceYoffset = 0;
int __attribute__((section(".dtcm"))) subSurfaceZoom = 3; // 8 veces más cerca

int previewXoffset = 0;
int previewYoffset = 0;
int previewPosAlpha = 15;

int prevx = 0;
int prevy = 0;

int imgFormat = 0;

int stylusHoldTimer = STYLUSHOLDTIME;
bool stylusRepeat = false;

int stackYres = 7;
int stackXres = 7;

// usado como variable temporal en la creación de imagen nueva
int resX = 7;
int resY = 7;

u8 palEditSel = 1;
u32 __attribute__((section(".dtcm"))) kDown = 0;
u32 __attribute__((section(".dtcm"))) kHeld = 0;
u32 __attribute__((section(".dtcm"))) kUp = 0;

u32 __attribute__((section(".dtcm"))) frameStartTime = 0;
u32 __attribute__((section(".dtcm"))) frameEndTime = 0;

bool stylusPressed = false;
bool showGrid = false;
bool drew = false;
bool __attribute__((section(".dtcm"))) updated = false;
bool __attribute__((section(".dtcm"))) accurate = false;
bool mayus = false;
int holdTimer = 0;
int fileOffset = 0;
int gridSkips = 0;
int prevZoom = subSurfaceZoom;
bool rPressed = false;
bool screensSwapped = false;
bool showBrushSettings = false;
bool preview = true;
bool redraw = true;

// Variables globales para controlar el modo actual
subMode currentSubMode = SUB_BITMAP;

consoleMode currentConsoleMode = MODE_NO;

enum
{
    ACTION_NONE = 0,
    ACTION_UP = 1 << 0,
    ACTION_DOWN = 1 << 1,
    ACTION_LEFT = 1 << 2,
    ACTION_RIGHT = 1 << 3,
    ACTION_ZOOM_IN = 1 << 4,
    ACTION_ZOOM_OUT = 1 << 5,
};

typedef enum
{
    TOOL_BRUSH,
    TOOL_ERASER,
    TOOL_BUCKET,
    TOOL_PICKER
} ToolType;

ToolType currentTool = TOOL_BRUSH; // por defecto

typedef enum
{
    BRUSH_MODE_NORMAL = 0,
    BRUSH_MODE_DITHER,
    BRUSH_MODE_VLINES,
    BRUSH_MODE_HLINES,
} BrushMode;

typedef enum
{
    BRUSH_SIZE_1 = 0,
    BRUSH_SIZE_2,
    BRUSH_SIZE_3,
    BRUSH_SIZE_4
} BrushSize;

typedef enum
{
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
    .numColors = fontPalLen >> 1,
    .bpp = 4,
    .asciiOffset = 32,
    .numChars = fontTilesLen >> 5,
};

// FUNCIONES
void submitVRAM(bool _accurate = false)
{
    const u32 sizeTop = surfaceSize << 1;

    if (paletteBpp != 16)
    {
        if (_accurate)
            DC_FlushRange(pixelsTop, sizeTop);

        DMA2_CR = 0;
        DMA2_SRC  = (u32)pixelsTop;
        DMA2_DEST = (u32)pixelsTopVRAM;
        DMA2_CR   = (sizeTop >> 1) | DMA_ENABLE;
        while (DMA2_CR & DMA_ENABLE);

        // leer desde pixelsTop, no desde VRAM
        DMA3_CR = 0;
        DMA3_SRC  = (u32)pixelsTop;
        DMA3_DEST = (u32)pixelsVRAM;
        DMA3_CR   = (sizeTop >> 1) | DMA_ENABLE;
        while (DMA3_CR & DMA_ENABLE);
    }
    else
    {
        if (_accurate)
            DC_FlushRange(pixelsTop, sizeTop);
        // 16bpp: main ya está en VRAM, sub lee desde ahí
        DMA3_CR = 0;
        DMA3_SRC  = (u32)pixelsTopVRAM;
        DMA3_DEST = (u32)pixelsVRAM;
        DMA3_CR   = (sizeTop >> 1) | DMA_ENABLE;
        while (DMA3_CR & DMA_ENABLE);
    }
}
__attribute__((section(".itcm"))) static void vblank_handler(void)
{
    // Stop the previous DMA copy
    dmaStopSafe(0);

    BG_PALETTE[0] = gradientTable[0];

    dmaSetParams(0,
                 &gradientTable[1],
                 &BG_PALETTE[0],              // Write to the background color
                 DMA_SRC_INC |                // Autoincrement source after each copy
                     DMA_DST_FIX |            // Keep destination fixed
                     DMA_START_HBL |          // Start copy at the start of horizontal blank
                     DMA_REPEAT |             // Don't stop DMA after the first copy.
                     DMA_COPY_HALFWORDS | 1 | // Copy one halfword each time
                     DMA_ENABLE);
}

void initGradient()
{
    for (int i = 0; i < 32; i++)
    {
        int r = 15 - i;
        if (r < 0)
            r = 0;

        int b = (31 - i) >> 1;
        u16 color = (b << 10) | (r<<isGreen);
        gradientTable[i] = color;
        gradientTable[SCREEN_H - i] = color;
    }
    //pequeña probabilidad de que el gradiente se invierta :>
    irqSet(IRQ_VBLANK, vblank_handler); // configurar HDMA
}
__attribute__((section(".itcm"))) bool brushPatternPass(int x, int y, BrushMode mode)
{
    switch (mode)
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
__attribute__((section(".itcm"))) void drawPixelSurface(int x, int y, u16 color)
{
    if ((unsigned)x < 1 << surfaceXres &&
        (unsigned)y < 1 << surfaceYres &&
        brushPatternPass(x, y, brushMode))
    {
        surface[(y << surfaceXres) + x] = color;
    }
}
inline u16 mergeColorAlpha(u16 oldCol, u16 color, u8 alpha)
{
    if (alpha > MAX_ALPHA)
        alpha = MAX_ALPHA;

    u8 r1 = (oldCol >> 10) & 31;
    u8 g1 = (oldCol >> 5) & 31;
    u8 b1 = oldCol & 31;

    u8 r = (color >> 10) & 31;
    u8 g = (color >> 5) & 31;
    u8 b = color & 31;

    int r2 = (r * alpha + r1 * (MAX_ALPHA - alpha)) / MAX_ALPHA;
    int g2 = (g * alpha + g1 * (MAX_ALPHA - alpha)) / MAX_ALPHA;
    int b2 = (b * alpha + b1 * (MAX_ALPHA - alpha)) / MAX_ALPHA;

    // --- corrección de estancamiento ---
    if (alpha > 0)
    {
        if (r2 == r1)
            r2 += (r > r1) - (r < r1);
        if (g2 == g1)
            g2 += (g > g1) - (g < g1);
        if (b2 == b1)
            b2 += (b > b1) - (b < b1);
    }

    return (r2 << 10) | (g2 << 5) | b2 | 0x8000;
}

__attribute__((section(".itcm"))) void drawPixelSurfaceAlpha(int x, int y, u16 color)
{
    if ((unsigned)x < 1 << surfaceXres &&
        (unsigned)y < 1 << surfaceYres &&
        brushPatternPass(x, y, brushMode))
    {
        int index = (y << surfaceXres) + x;
        surface[index] = mergeColorAlpha(surface[index], color, paletteAlpha);
    }
}
inline void brushStamp2(int x, int y, u16 color)
{
    if (paletteAlpha != MAX_ALPHA)
    {
        drawPixelSurfaceAlpha(x, y, color);
        drawPixelSurfaceAlpha(x + 1, y, color);
        drawPixelSurfaceAlpha(x, y + 1, color);
        drawPixelSurfaceAlpha(x + 1, y + 1, color);
    }
    else
    {
        drawPixelSurface(x, y, color);
        drawPixelSurface(x + 1, y, color);
        drawPixelSurface(x, y + 1, color);
        drawPixelSurface(x + 1, y + 1, color);
    }
}

inline void brushStamp3Square(int x, int y, u16 color)
{
    if (paletteAlpha != MAX_ALPHA)
    {
        for (int oy = -1; oy <= 1; oy++)
            for (int ox = -1; ox <= 1; ox++)
                drawPixelSurfaceAlpha(x + ox, y + oy, color);
    }
    else
    {
        for (int oy = -1; oy <= 1; oy++)
            for (int ox = -1; ox <= 1; ox++)
                drawPixelSurface(x + ox, y + oy, color);
    }
}

inline void brushStamp3Circle(int x, int y, u16 color)
{
    if (paletteAlpha != MAX_ALPHA)
    {
        drawPixelSurfaceAlpha(x, y, color);
        drawPixelSurfaceAlpha(x + 1, y, color);
        drawPixelSurfaceAlpha(x, y + 1, color);
        drawPixelSurfaceAlpha(x - 1, y, color);
        drawPixelSurfaceAlpha(x, y - 1, color);
    }
    else
    {
        drawPixelSurface(x, y, color);
        drawPixelSurface(x + 1, y, color);
        drawPixelSurface(x, y + 1, color);
        drawPixelSurface(x - 1, y, color);
        drawPixelSurface(x, y - 1, color);
    }
}
inline void brushStamp4Circle(int x, int y, u16 color)
{
    if (paletteAlpha != MAX_ALPHA)
    {
        drawPixelSurfaceAlpha(x + 1, y - 1, color);
        drawPixelSurfaceAlpha(x, y - 1, color);
        drawPixelSurfaceAlpha(x - 1, y + 1, color);
        drawPixelSurfaceAlpha(x - 1, y, color);
        drawPixelSurfaceAlpha(x, y, color);
        drawPixelSurfaceAlpha(x, y + 1, color);
        drawPixelSurfaceAlpha(x, y + 2, color);
        drawPixelSurfaceAlpha(x + 1, y + 1, color);
        drawPixelSurfaceAlpha(x + 1, y + 2, color);
        drawPixelSurfaceAlpha(x + 2, y + 1, color);
        drawPixelSurfaceAlpha(x + 1, y, color);
        drawPixelSurfaceAlpha(x + 2, y, color);
    }
    else
    {
        drawPixelSurface(x + 1, y - 1, color);
        drawPixelSurface(x, y - 1, color);
        drawPixelSurface(x - 1, y + 1, color);
        drawPixelSurface(x - 1, y, color);
        drawPixelSurface(x, y, color);
        drawPixelSurface(x, y + 1, color);
        drawPixelSurface(x, y + 2, color);
        drawPixelSurface(x + 1, y + 1, color);
        drawPixelSurface(x + 1, y + 2, color);
        drawPixelSurface(x + 2, y + 1, color);
        drawPixelSurface(x + 1, y, color);
        drawPixelSurface(x + 2, y, color);
    }
}

inline void brushStamp4Square(int x, int y, u16 color)
{
    if (paletteAlpha != MAX_ALPHA)
    {
        for (int oy = -1; oy <= 2; oy++)
            for (int ox = -1; ox <= 2; ox++)
                drawPixelSurfaceAlpha(x + ox, y + oy, color);
    }
    else
    {
        for (int oy = -1; oy <= 2; oy++)
            for (int ox = -1; ox <= 2; ox++)
                drawPixelSurface(x + ox, y + oy, color);
    }
}

// decidir el patrón y tamaño de pincel
inline void brushStamp(int x, int y, u16 color)
{
    bool forceSquare = (brushMode == BRUSH_MODE_DITHER);

    switch (brushSize)
    {
    case BRUSH_SIZE_1:
        if (paletteAlpha != MAX_ALPHA)
        {
            drawPixelSurfaceAlpha(x, y, color);
        }
        else
        {
            drawPixelSurface(x, y, color);
        }
        break;

    case BRUSH_SIZE_2:
        brushStamp2(x, y, color);
        break;

    case BRUSH_SIZE_3:
        if (forceSquare)
            brushStamp3Square(x, y, color);
        else
            brushStamp3Circle(x, y, color);
        break;

    case BRUSH_SIZE_4:
        if (forceSquare)
            brushStamp4Square(x, y, color);
        else
            brushStamp4Circle(x, y, color);
        break;
    }
}

__attribute__((section(".itcm"))) void drawLineSurface(int x0, int y0, int x1, int y1, u16 color)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1)
    {
        brushStamp(x0, y0, color);

        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

__attribute__((section(".itcm"))) void drawLineSurfaceAlpha(int x0, int y0, int x1, int y1, u16 color)
{
    const int dx = abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1)
    {
        brushStamp(x0, y0, color);

        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

__attribute__((section(".itcm"))) void drawGrid(u16 color) {
    int separation = 1 << (subSurfaceZoom + gridSkips);

    dmaFillHalfWords(0, gfxGrid, 64 * 64 * 2);

    if (separation < 2 || separation > 64)
        return;

    for (int i = gridOamId; i < gridOamId + 4; i++)
        oamSetHidden(&oamSub, i, false);

    int phaseX = (subSurfaceXoffset << subSurfaceZoom) & (separation - 1);
    int phaseY = (subSurfaceYoffset << subSurfaceZoom) & (separation - 1);

    // líneas verticales
    for (int x = -phaseX; x < 64; x += separation) {
        if (x >= 0) {
            u16* p = gfxGrid + x;
            for (int j = 0; j < 64; j++, p += 64)
                *p = color;
        }
    }

    // líneas horizontales
    for (int y = -phaseY; y < 64; y += separation) {
        if (y >= 0)
            dmaFillHalfWords(color, gfxGrid + y * 64, 64 * 2);
    }
}

void updatePreviewPos(){
    //este sprite mide 64x64
    //obtenemos los datos como el offset y otras cosas
    oamSet(&oamMain, selectedZoneOamId,
        previewXoffset+subSurfaceXoffset,
        previewYoffset+subSurfaceYoffset,
        0, previewPosAlpha,
        SpriteSize_64x64, SpriteColorFormat_Bmp,
        gfxSelectedZone,
        -1,
        false, false, false, false, false);
    oamUpdate(&oamMain);
    
}

__attribute__((section(".itcm"))) void updatePreviewGfx(){
    //reiniciamos visualmente todo
    u16 color = AVinvertColor(palette[paletteOffset]);
    dmaFillHalfWords(1,gfxSelectedZone, 64 * 64 * 2);

    if(subSurfaceZoom > 0){
        const int _size = 1<<(7-subSurfaceZoom);

        const int offset = ((_size - 1) << 6);
        const int size2 = _size-1;

        // línea superior e inferior
        for (int i = 0; i < _size; i++) {
            gfxSelectedZone[i] = color;// top
        }
        for (int i = offset; i < _size+offset; i++) {
            gfxSelectedZone[i] = color;// bottom
        }
        
        // líneas laterales
        for (int j = 1; j < (_size - 1); j++){
            gfxSelectedZone[(j << 6)]         = color;// left
            gfxSelectedZone[(j << 6) + size2] = color;// right
        }
    }
}
//=========================================================DRAW SURFACE========================================================================
__attribute__((section(".itcm"))) void drawSurfaceMain()
{
    updated = true;
    const int xres = 1 << surfaceXres;
    const int yres = 1 << surfaceYres;

    if (paletteBpp == 16)
    {
        u16 *dst = (u16*)pixelsTopVRAM; // directo a VRAM
        const u16 *src = surface;
        if (xres == 128)
        {
            dmaCopy(src, dst, 128 * yres * 2);
            return;
        }
        for (int i = 0; i < yres; i++, dst += 128, src += xres)
        {
            u32 *dst32 = (u32*)dst;
            const u32 *src32 = (const u32*)src;
            for (int j = 0; j < (xres >> 1); j++)
                dst32[j] = src32[j];
        }
        return;
    }

    // modo paleta — sin cambios
    const u16 *pal = palette + paletteOffset;
    const u16 *src = surface;
    u16 *dst = pixelsTop;

    for (int i = 0; i < yres; i++, dst += 128, src += xres)
    {
        const u16 *row = src;
        u32 *dst32 = (u32*)dst;
        int j = 0;
        for (; j < xres - 1; j += 2)
        {
            u32 a = pal[row[j]];
            u32 b = pal[row[j + 1]];
            dst32[j >> 1] = a | (b << 16);
        }
        if (j < xres)
            dst[j] = pal[row[j]];
    }
}
void drawSurfaceBottom()
{ // esta funcion ahora se encarga de limitar y actualizar ciertos datos.
    int visibleX = 128 >> subSurfaceZoom;
    int visibleY = 128 >> subSurfaceZoom;

    int maxX = (1 << surfaceXres) - visibleX;
    int maxY = (1 << surfaceYres) - visibleY;

    if (maxX < 0) maxX = 0;
    if (maxY < 0) maxY = 0;
    if (subSurfaceXoffset < 0)
        subSurfaceXoffset = 0;
    if (subSurfaceYoffset < 0)
        subSurfaceYoffset = 0;
    if (subSurfaceXoffset > maxX)
        subSurfaceXoffset = maxX;
    if (subSurfaceYoffset > maxY)
        subSurfaceYoffset = maxY;
    
    // --- Limitar el zoom ---
    if(prevZoom != subSurfaceZoom){
        int maxRes = MAX(surfaceXres, surfaceYres);
    
        int minZoom = 7 - maxRes;
        int maxZoom = 6;
        if (subSurfaceZoom < minZoom)
        {
            subSurfaceZoom = minZoom;
        }
        if (subSurfaceZoom > maxZoom){
            subSurfaceZoom = maxZoom;
        }
    }


    s16 scale = 256 >> subSurfaceZoom;
    //un lut para los zooms
    s16 oamScale = 1<<(subSurfaceZoom+4);
    if(oamScale > 256){
        oamScale = 256;
    }

    
    oamRotateScale(&oamSub, 1, 0,oamScale,oamScale);
    oamUpdate(&oamSub);
    bgSetScale(bgCanvas, scale, scale);
    bgSetScroll(bgCanvas,
                subSurfaceXoffset - (64 >> subSurfaceZoom), // centrar en x=64..191
                subSurfaceYoffset);
    bgUpdate();
    updatePreviewPos();
}
//==================== PALETAS ==========|
// función auxiliar para esta situación específica
static void drawSliderRect(u16 *buf, int x, int row, int w, u16 color)
{
    u32 color32 = ((u32)color << 16) | color;
    u16 *base = buf + row * 64 + x;

    for (int j = 0; j < 8; j++)
    {
        u16 *p16 = base;
        int i = 0;

        if ((uintptr_t)p16 & 2)
        {
            *p16++ = color;
            i++;
        }

        u32 *p32 = (u32 *)p16;
        int w32 = (w - i) >> 1;
        for (int k = 0; k < w32; k++)
            *p32++ = color32;

        if ((w - i) & 1)
            *(u16 *)p32 = color;

        base += 64;
    }
}
__attribute__((section(".itcm"))) static void draw4xRectIn64w(u16 *buf, int x, int y, u16 col)
{
    u32 col32 = ((u32)col << 16) | col;
    u32 *p = (u32 *)(buf + x + (y << 6));

    p[0] = col32;
    p[1] = col32;
    p += 32;
    p[0] = col32;
    p[1] = col32;
    p += 32;
    p[0] = col32;
    p[1] = col32;
    p += 32;
    p[0] = col32;
    p[1] = col32;
}

void drawNesPalette()
{
    for (int i = 0; i < 16 * 64; i++)
    {
        gfxRGBsliders[i] = C_BLACK;
    }
    // dibujar paleta
    for (int i = 0; i < 4; i++) // vertical
    {
        for (int j = 0; j < 16; j++) // horizontal
        {
            draw4xRectIn64w(gfxRGBsliders, j << 2, (i << 2) + 16, nesPalette[(i << 4) + j]);
        }
    }
}
__attribute__((section(".itcm"))) void drawColorPalette()
{
    // dibujar paleta
    for (int i = 0; i < 16; i++) // vertical
    {
        for (int j = 0; j < 16; j++) // horizontal
        {
            draw4xRectIn64w(gfxPalette, j << 2, i << 2, palette[(i << 4) + j]);
        }
    }
}

void updatePal(int increment, int *palettePos)
{
    // primero debemos saber si estamos en un rango válido
    if (*palettePos + increment < 0 || *palettePos + increment > paletteSize - 1)
    {
        return;
    }

    // obtenemos la coordenada de la paleta
    int posx = *palettePos & 15;
    int posy = *palettePos >> 4;

    u16 _col = palette[*palettePos];

    // nueva información
    *palettePos += increment;

    _col = palette[*palettePos];
    // obtener cada color RGB
    u8 r = (_col & 31);
    u8 g = (_col & 992) >> 5;
    u8 b = (_col & 31744) >> 10;
    u8 _barColAmount[3] = {r, g, b};
    for (int i = 0; i < 3; i++)
        palEdit[i] = _barColAmount[i];

    if (nesMode == false)
    {
        // rectangulos de abajo
        u16 _barCol[3] = {C_RED, C_GREEN, C_BLUE};

        for (int i = 0; i < 3; i++)
        {
            int y = (i << 3) + 8;
            drawSliderRect(gfxRGBsliders, 0, y, _barColAmount[i] << 1, _barCol[i]);
            drawSliderRect(gfxRGBsliders, _barColAmount[i] << 1, y, 64 - (_barColAmount[i] << 1), C_BLACK);
        }
        drawSliderRect(gfxRGBsliders, 0, 0, paletteAlpha, _col);
    }

    // obtenemos la coordenada de la paleta (otra vez)
    posx = *palettePos & 15;
    posy = *palettePos >> 4;

    if (paletteBpp != 8)
    {
        int prevOffset = paletteOffset;
        if (paletteBpp == 4)
        {
            paletteOffset = posy << 4;
        }
        else if (paletteBpp == 2)
        {
            paletteOffset = (posy << 4) + ((posx >> 2) << 2);
        }
        if (prevOffset != paletteOffset)
        { // se cambió la paleta, necesita actualizar la pantalla
            drawSurfaceMain();
        }
    }
    oamSetXY(&oamSub, paletteSelOamId, (posx << 2) + 191, (posy << 2) + 63);
    oamUpdate(&oamSub);
}
//esta función actualiza tanto el color como las barras de paletas
void updatePalEditBar(int index)
{
    static int prevAmount[4] = {0,0,0,0};
    if(prevAmount[index] == palEdit[index]){
        return;
    }
    int amount = palEdit[index];
    prevAmount[index] = amount;
    

    // Actualizar barra
    const u16 _barCol[3] = {C_RED, C_GREEN, C_BLUE};
    int y = (index << 3) + 8;
    drawSliderRect(gfxRGBsliders, 0, y, amount << 1, _barCol[index]);
    drawSliderRect(gfxRGBsliders, amount << 1, y, 64 - (amount << 1), C_BLACK);

    // Actualizar el color
    u16 _col = palEdit[0];
    _col += palEdit[1] << 5;
    _col += palEdit[2] << 10;
    _col |= 0x8000; // encender bit alpha
    palette[palettePos] = _col;

    draw4xRectIn64w(gfxPalette, (palettePos & 15) << 2, (palettePos >> 4) << 2, _col);

    if (paletteBpp != 16)
    {
        drawSurfaceMain();
        drawSliderRect(gfxRGBsliders, 0, 0, MAX_ALPHA, _col);
    }
    else
    {
        drawSliderRect(gfxRGBsliders, 0, 0, MAX_ALPHA, _col);
        drawSliderRect(gfxRGBsliders, paletteAlpha, 0, 64 - paletteAlpha, 0);
    }
    if (palettePos == 0)
    {
        if(showGrid == true){
            drawGrid(AVinvertColor(_col));
        }
        updatePreviewGfx();
    }
}

//=================================================================Inicialización===================================================================================|
void clearTop()
{
    memset(pixelsTop, 0, surfaceSize << 1);
}
void clearPal()
{
    for (int i = 0; i < paletteSize; i++)
    {
        palette[i] = C_BLACK;
    }
}
void clearAll()
{
    clearTop();
    clearPal();
    for (int i = 0; i < surfaceSize; i++)
    {
        surface[i] = 0;
        stack[i] = 0;
        pixelsTopVRAM[i] = 0;
    }
}

u16 *gfxBrushSettings;
u16 *gfx8;
void setBrushSettingsSprites(bool on)
{
    showBrushSettings = on;
    oamSet(&oamSub, brushSettingsOamId, //
           16, 32,                      // posición
           0,                           // prioridad
           on,                          // opaco
           SpriteSize_32x16, SpriteColorFormat_Bmp,
           gfxBrushSettings,
           -1,
           false, false, false, false, false);

    oamSet(&oamSub, brushSettingsSelectorOamId, //
           ((int)brushMode << 3) + 16, 32,      // posición
           0,                                   // prioridad
           on,                                  // opaco
           SpriteSize_8x8, SpriteColorFormat_Bmp,
           gfx8,
           -1,
           false, false, false, false, false);
    oamSet(&oamSub, brushSettingsSelectorOamId + 1, //
           ((int)brushSize << 3) + 16, 40,          // posición
           0,                                       // prioridad
           on,                                      // opaco
           SpriteSize_8x8, SpriteColorFormat_Bmp,
           gfx8,
           -1,
           false, false, false, false, false);
    oamUpdate(&oamSub);
}

void setEditorSprites()
{
    // iniciamos el sprite para dibujar : )
    oamInit(&oamMain,SpriteMapping_Bmp_1D_128, false);
    oamInit(&oamSub, SpriteMapping_Bmp_1D_128, false);

    oamClear(&oamMain, 0, 128);
    oamClear(&oamSub, 0, 128);

    gfxPalette = oamAllocateGfx(&oamSub, SpriteSize_64x64, SpriteColorFormat_Bmp);

    gfx32 = oamAllocateGfx(&oamSub, SpriteSize_32x32, SpriteColorFormat_Bmp);
    dmaCopy(GFXselector24Bitmap, gfx32, 32 * 32 * 2);
    gfx16 = oamAllocateGfx(&oamSub, SpriteSize_16x16, SpriteColorFormat_Bmp);
    dmaCopy(GFXselector16Bitmap, gfx16, 16 * 16 * 2);
    gfx8 = oamAllocateGfx(&oamSub, SpriteSize_8x8, SpriteColorFormat_Bmp);
    dmaCopy(GFXselector8Bitmap, gfx8, 8 * 8 * 2);
    gfx5 = oamAllocateGfx(&oamSub, SpriteSize_8x8, SpriteColorFormat_Bmp);
    dmaCopy(GFXselector5Bitmap, gfx5, 8 * 8 * 2);

    gfxBrushSettings = oamAllocateGfx(&oamSub, SpriteSize_32x16, SpriteColorFormat_Bmp);
    dmaCopy(GFXbrushSettingsBitmap, gfxBrushSettings, 32 * 16 * 2);
    if(!nesMode){
        gfxRGBsliders = oamAllocateGfx(&oamSub, SpriteSize_64x32, SpriteColorFormat_Bmp);
        dmaCopy(GFXrgbSlidersBitmap, gfxRGBsliders, 64 * 32 * 2);
    }
    gfxRgbSliderSel = oamAllocateGfx(&oamSub, SpriteSize_8x8, SpriteColorFormat_Bmp);
    dmaCopy(GFXrgbSliderSelBitmap, gfxRgbSliderSel, 8 * 8 * 2);

    oamSet(&oamSub, paletteOamId,
           192, 64,
           0,  // prioridad
           15, // palette
           SpriteSize_64x64, SpriteColorFormat_Bmp,
           gfxPalette,
           -1,
           false, false, false, false, false);

    oamSet(&oamSub, rgbSliderOamId,
           rgbSliderX, rgbSliderY,
           0,
           15, // opaco
           SpriteSize_64x32, SpriteColorFormat_Bmp,
           gfxRGBsliders,
           -1,
           false, false, false, false, false);

    oamSet(&oamSub, rgbSliderSelOamId,
           SCREEN_W - 2, rgbSliderY + 8,
           0,
           15, // opaco
           SpriteSize_8x8, SpriteColorFormat_Bmp,
           gfxRgbSliderSel,
           -1,
           false, false, false, false, false);

    oamSet(&oamSub, selector24oamID,
           0, 16,
           0,
           15, // opaco
           SpriteSize_32x32, SpriteColorFormat_Bmp,
           gfx32,
           -1,
           false, false, false, false, false);

    oamSet(&oamSub, paletteSelOamId,
           rgbSliderX - 1, 63,
           0,
           15, // opaco
           SpriteSize_8x8, SpriteColorFormat_Bmp,
           gfx5,
           -1,
           false, false, false, false, false);

    palettePos = 0;
    paletteOffset = 0;
    gfxGrid = oamAllocateGfx(&oamSub, SpriteSize_64x64, SpriteColorFormat_Bmp);
    dmaFillHalfWords(0, gfxGrid, 64 * 64 * 2);
    gfxSelectedZone = oamAllocateGfx(&oamMain, SpriteSize_64x64, SpriteColorFormat_Bmp);
    
    for(int i = gridOamId; i < 4; i++)
    {
        oamSetBlendMode(&oamSub, i,SpriteMode_Blended);
    }
    oamSetBlendMode(&oamMain,selectedZoneOamId,SpriteMode_Blended);

    const int eva = 5;
    const int evb = 8;
    const int evy = 10;

    // Motor principal (oamMain)
    REG_BLDCNT = BLEND_ALPHA
    | BLEND_SRC_SPRITE
    | BLEND_DST_BG3
    | BLEND_DST_BACKDROP;
    REG_BLDALPHA = BLDALPHA_EVA(eva) | BLDALPHA_EVB(evb);
    REG_BLDY     = BLDY_EVY(evy);

    // Motor secundario (oamSub)
    REG_BLDCNT_SUB    = BLEND_ALPHA | BLEND_SRC_SPRITE | BLEND_DST_BG3 | BLEND_DST_BACKDROP;
    REG_BLDALPHA_SUB  = BLDALPHA_EVA(eva) | BLDALPHA_EVB(evb);
    REG_BLDY_SUB      = BLDY_EVY(evy);

    oamSet(&oamMain, selectedZoneOamId,
        0, 0,
        0, 15,
        SpriteSize_64x64, SpriteColorFormat_Bmp,
        gfxSelectedZone,
        -1,
        false, false, false, false, false);
        
    #define gridOpacity 8
    // top-left
    oamSet(&oamSub, gridOamId,
        SURFACE_X, 0,
        0, 8,
        SpriteSize_64x64, SpriteColorFormat_Bmp,
        gfxGrid,
        -1,
        false, false, false, false, false);

    // top-right
    oamSet(&oamSub, gridOamId + 1,
        SURFACE_X + 64, 0,
        0, 8,
        SpriteSize_64x64, SpriteColorFormat_Bmp,
        gfxGrid,
        -1,
        false, false, false, false, false);

    // bottom-left
    oamSet(&oamSub, gridOamId + 2,
        SURFACE_X, 64,
        0, 8,
        SpriteSize_64x64, SpriteColorFormat_Bmp,
        gfxGrid,
        -1,
        false, false, false, false, false);

    // bottom-right
    oamSet(&oamSub, gridOamId + 3,
        SURFACE_X + 64, 64,
        0, 8,
        SpriteSize_64x64, SpriteColorFormat_Bmp,
        gfxGrid,
        -1,
        false, false, false, false, false);
    setBrushSettingsSprites(true);
    updatePreviewPos();
    updatePreviewGfx();
}

void setOamBG()
{
    u16 *gfxBG = oamAllocateGfx(&oamSub, SpriteSize_32x32, SpriteColorFormat_Bmp);
    dmaCopy(GFXbackgroundBitmap, gfxBG, 32 * 32 * 2);
    for (int i = 0; i < 16; i++)
    {
        int x = ((i & 0b11) << 5) + SURFACE_X;
        int y = (i & 0b1100) << 3;

        oamSet(&oamSub, i + 4, // index
               x, y,           // posición
               1,
               15, // opaco
               SpriteSize_32x32, SpriteColorFormat_Bmp,
               gfxBG,
               1,
               false, false, false, false, false);
    }
}

void initBitmap()
{
    clearAll();

    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_SPRITE);

    consoleInit(&topConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 4, true, true);
    consoleSetFont(&topConsole, &font);
    oamClear(&oamSub, 0, 128);
    bgInit(3, BgType_Bmp16, BgSize_B16_128x128, 0, 0);

    videoSetModeSub(MODE_5_2D); // pantalla inferior bitmap
    vramSetBankC(VRAM_C_SUB_BG);
    vramSetBankD(VRAM_D_SUB_SPRITE); // sprites en VRAM 
    // capas
    bgCanvas = bgInitSub(3, BgType_Bmp16, BgSize_B16_128x128, 4, 0);
    bgUI = bgInitSub(2, BgType_Bmp8, BgSize_B8_256x256, 0, 0);

    pixelsVRAM = (u16 *)bgGetGfxPtr(bgCanvas);

    decompress(GFXinputBitmap, bgGetGfxPtr(bgUI), LZ77Vram);
    dmaCopy(GFXinputPal, BG_PALETTE_SUB, GFXinputPalLen);    


    drawSurfaceMain();
    drawSurfaceBottom();
    accurate = true;
    submitVRAM();

    // Prioridades: UI detrás del canvas
    bgSetPriority(bgCanvas, 3); // canvas prioridad mínima
    bgSetPriority(bgUI, 0);
    bgSetScale(3, 256, 256);
    //calcular offsets
    previewXoffset = (SCREEN_W-(1<<surfaceXres))>>1;
    previewYoffset = (SCREEN_H-(1<<surfaceYres))>>1;
    bgSetScroll(3, -previewXoffset, -previewYoffset);
    bgUpdate();

    setEditorSprites();
    setBackupVariables();
}

//====================================================================Compatibilidad con modos gráficos====================================|
void textMode()
{
    if (currentSubMode == SUB_TEXT)
        return; // ya estamos en texto
    currentSubMode = SUB_TEXT;
    // matamos ambas pantallas por que... why not :)

    videoSetMode(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    consoleInit(&topConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
    consoleSetFont(&topConsole, &font);
    oamClear(&oamSub, 0, 128);
    oamUpdate(&oamSub);
}
int bgPreview;
void textKeyboardDraw()
{
    // añadir capa de preview
    bgPreview = bgInitSub(3, BgType_Bmp16, BgSize_B16_128x128, 4, 0);
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
    if (currentSubMode == SUB_BITMAP)
        return; // ya estamos en bitmap
    currentSubMode = SUB_BITMAP;

    // reiniciamos VRAM
    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_SPRITE); // sprites en VRAM B
    int bgMain = bgInit(3, BgType_Bmp16, BgSize_B16_128x128, 0, 0);
    consoleInit(&topConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 4, true, true);
    consoleSetFont(&topConsole, &font);

    oamClear(&oamMain, 0, 128);
    oamClear(&oamSub, 0, 128);

    videoSetModeSub(MODE_5_2D); // pantalla inferior bitmap
    vramSetBankC(VRAM_C_SUB_BG);
    vramSetBankD(VRAM_D_SUB_SPRITE); // sprites en VRAM D

    bgCanvas = bgInitSub(3, BgType_Bmp16, BgSize_B16_128x128, 4, 0);
    bgUI = bgInitSub(2, BgType_Bmp8, BgSize_B8_256x256, 0, 0);

    decompress(GFXinputBitmap, bgGetGfxPtr(bgUI), LZ77Vram);
    dmaCopy(GFXinputPal, BG_PALETTE_SUB, GFXinputPalLen);

    setEditorSprites();
    paletteAlpha = MAX_ALPHA;
    submitVRAM(true); // recuperamos nuestros queridos datos

    //ahora siempre activo
    setOamBG();
    bgSetScale(bgCanvas, 256, 256);
    bgSetScroll(bgCanvas, -64, -32);

    previewXoffset = (SCREEN_W-(1<<surfaceXres))>>1;
    previewYoffset = (SCREEN_H-(1<<surfaceYres))>>1;
    bgSetScale(bgMain, 256, 256);
    bgSetScroll(bgMain, -previewXoffset, -previewYoffset);
    drawSurfaceBottom();
    oamUpdate(&oamSub);
}
char bucketText[2][6] = {"Color", "Index"};
void drawInfo()
{
    consoleClear();

    if (animation.frames != 0)
        printf("frame: %d / %d \nanim speed: %d", animation.pos, animation.frames, animation.speed);
    else
        printf("\n\n");

    if (bucketMode != 0)
        printf("\nBucket: replace %s", bucketText[bucketMode - 1]);
    else
        printf("\n");

#ifdef DEBUG_CPU
    u32 ticks = frameEndTime - frameStartTime;
    u32 cpuUsage = ((u64)(ticks) * 11732) >> 16;
    printf("\n%d\n%lu", fps, cpuUsage);
#endif
}
//====================================================================Backups==============================================================|
void setBackupVariables()
{
    backupIndex = 0;
    backupSize = 1 << surfaceXres << surfaceYres;
    backupMax = BACKUP_SIZE/backupSize;
    
    // reinicia el backup
    for (int i = 0; i < 131072; i++)
    {
        backup[i] = 0;
    }
}
void backupWrite()
{
    backupIndex++;
    if (backupIndex > backupMax)
    {
        backupIndex = 0;
    }
    int index = backupIndex * backupSize;
    // copia surface a backup+ su index
    dmaCopyHalfWords(2, surface, backup + index, backupSize * sizeof(u16));
}
void backupRead()
{
    // Calculamos el índice del bloque en el array backup
    int index = backupIndex * backupSize;
    dmaCopyHalfWords(2, backup + index, surface, backupSize * sizeof(u16));
    accurate = true;
}
//============================================================= SD CARD ===============================================|
void createAppFolder()
{
    mkdir("/_nds/", 0777);
    mkdir(APP_PATH, 0777);
}

void clearCache()
{
    // abrir el directorio
    DIR *dir = opendir(CACHE_PATH);
    if (dir)
    {
        struct dirent *entry;
        char filepath[256];

        while ((entry = readdir(dir)) != NULL)
        {
            // ignorar . y ..
            if (entry->d_name[0] == '.')
                continue;

            snprintf(filepath, sizeof(filepath), "%s%s", CACHE_PATH, entry->d_name);
            remove(filepath);
        }
        closedir(dir);
    }

    // recrear por si no existía
    mkdir(CACHE_PATH, 0777);
}

//============================================================= INPUT =================================================|
inline int getActionsFromKeys(int keys)
{
    int actions = ACTION_NONE;

    if (keys & KEY_UP)
        actions |= ACTION_UP;
    if (keys & KEY_DOWN)
        actions |= ACTION_DOWN;
    if (keys & KEY_LEFT)
        actions |= ACTION_LEFT;
    if (keys & KEY_RIGHT)
        actions |= ACTION_RIGHT;
    if (keys & KEY_A)
        actions |= ACTION_ZOOM_IN;
    if (keys & KEY_B)
        actions |= ACTION_ZOOM_OUT;

    return actions;
}

int getActionsFromTouch(int button)
{
    int actions = ACTION_NONE;

    switch (button)
    {
    case 1: actions |= ACTION_LEFT;     break;
    case 2: actions |= ACTION_RIGHT;    break;
    case 5: actions |= ACTION_UP;       break;
    case 6: actions |= ACTION_DOWN;     break;
    case 0: actions |= ACTION_ZOOM_IN;  break;
    case 4: actions |= ACTION_ZOOM_OUT; break;
    case 3:
        showGrid = !showGrid;
        if(showGrid)
            drawGrid(AVinvertColor(palette[paletteOffset]));
        else
            dmaFillHalfWords(0, gfxGrid, 64 * 64 * 2);
        break;
    case 7:
        break;
    }

    return actions;
}

void applyActions(int actions)
{                                                         
    accurate = true;                                      
    int blockSize = (1 << surfaceXres) >> subSurfaceZoom;// tamaño de bloque en píxeles según el zoom
    if (kHeld & KEY_L || kHeld & KEY_X)
    {
        // --- Scroll por bloques ---
        if (actions & ACTION_UP)
            subSurfaceYoffset -= blockSize;
        if (actions & ACTION_DOWN)
            subSurfaceYoffset += blockSize;
        if (actions & ACTION_LEFT)
            subSurfaceXoffset -= blockSize;
        if (actions & ACTION_RIGHT)
            subSurfaceXoffset += blockSize;

        if (actions & ACTION_ZOOM_IN && gridSkips < surfaceXres)
        {
            gridSkips++;
        }
        if (actions & ACTION_ZOOM_OUT && gridSkips > 0)
        {
            gridSkips--;
        }
    }
    else
    {
        previewPosAlpha = 15;
        // --- Scroll por píxeles ---
        if (actions & ACTION_UP)
            subSurfaceYoffset--;
        if (actions & ACTION_DOWN)
            subSurfaceYoffset++;
        if (actions & ACTION_LEFT)
            subSurfaceXoffset--;
        if (actions & ACTION_RIGHT)
            subSurfaceXoffset++;

        if (actions & ACTION_ZOOM_IN)
        {
            subSurfaceZoom++;
            updatePreviewGfx();
        }
        if (actions & ACTION_ZOOM_OUT && subSurfaceZoom > 0)
        {
            subSurfaceZoom--;
            updatePreviewGfx();
        }
    }
    drawSurfaceBottom();
    if(showGrid)
        drawGrid(AVinvertColor(palette[paletteOffset]));
}
void replaceIndex(u16 *surface, u16 oldColor, u16 newColor)
{
    // guardar un backup para undo
    backupWrite();
    // ahora sí reemplazamos todos los indices
    int size = 1 << surfaceXres << surfaceYres;
    for (int i = 0; i < size; i++)
    {
        if (surface[i] == oldColor)
            surface[i] = newColor;
    }
}

void swapIndex(u16 oldIndex, u16 newIndex)
{
    // Swap en paleta
    u16 tmp = palette[oldIndex];
    palette[oldIndex] = palette[newIndex];
    palette[newIndex] = tmp;

    // Swap de índices en el surface
    int total = 1 << surfaceXres << surfaceYres;
    for (int i = 0; i < total; i++)
    {
        if (surface[i] == oldIndex)
            surface[i] = newIndex;
        else if (surface[i] == newIndex)
            surface[i] = oldIndex;
    }
    // actualizamos ahora todo visualmente
    if (paletteBpp != 16)
    {
        drawSurfaceMain();
    }
    updatePal(0, &palettePos);
    drawColorPalette();
}
void floodFill(u16 *surface, int x, int y, u16 oldColor, u16 newColor, int xres, int yres)
{
    if (oldColor == newColor)
        return;
    if (bucketMode == 1)
    {
        replaceIndex(surface, oldColor, newColor);
        return;
    }
    else if (bucketMode == 2)
    {
        swapIndex(oldColor, newColor);
    }
    int width = 1 << xres;
    int height = 1 << yres;
    if (x < 0 || y < 0 || x >= width || y >= height)
        return;
    if (surface[(y << xres) + x] != oldColor)
        return;

    // reinterpretar el stack global como bytes para doble capacidad
    u8 *stack8 = (u8 *)stack;
    int maxStack = (sizeof(stack) * 2);
    int sp = 0;

    stack8[sp++] = (u8)x;
    stack8[sp++] = (u8)y;

    while (sp > 0)
    {
        if (sp < 2)
            break; // seguridad mínima
        u8 cy = stack8[--sp];
        u8 cx = stack8[--sp];

        int idx = (cy << xres) + cx;
        if (surface[idx] != oldColor)
            continue;
        surface[idx] = newColor;

        // push vecinos con control de overflow
        if (sp <= maxStack - 8)
        {
            if (cx + 1 < width && surface[(cy << xres) + (cx + 1)] == oldColor)
            {
                stack8[sp++] = cx + 1;
                stack8[sp++] = cy;
            }
            if (cx > 0 && surface[(cy << xres) + (cx - 1)] == oldColor)
            {
                stack8[sp++] = cx - 1;
                stack8[sp++] = cy;
            }
            if (cy + 1 < height && surface[((cy + 1) << xres) + cx] == oldColor)
            {
                stack8[sp++] = cx;
                stack8[sp++] = cy + 1;
            }
            if (cy > 0 && surface[((cy - 1) << xres) + cx] == oldColor)
            {
                stack8[sp++] = cx;
                stack8[sp++] = cy - 1;
            }
        }
        else
        {
            break; // stack lleno → evita overflow
        }
    }
}

void applyTool(int x, int y, bool dragging)
{
    if (x == prevx && y == prevy)
    {
        return;
    }
    prevx = x;
    prevy = y;

    u16 color = 0;
    if (paletteBpp != 16)
    {
        color = palettePos - paletteOffset;
    }
    else
    { // si estamos en modo 16 bits
        color = palette[palettePos];
        color = color | 0x8000; // forzar alpha
    }

    switch (currentTool)
    {
    case TOOL_BRUSH:
        if (dragging)
        {
            if (paletteAlpha != MAX_ALPHA)
            {
                if (paletteAlpha == 0)
                {
                    drawLineSurface(prevtpx, prevtpy, x, y, 0);
                    break;
                }
                drawLineSurfaceAlpha(prevtpx, prevtpy, x, y, color);
                break;
            }
            drawLineSurface(prevtpx, prevtpy, x, y, color);
        }
        else
        {
            if (paletteAlpha == 0)
            {
                brushStamp(x, y, 0);
                break;
            }
            brushStamp(x, y, color);
            break;
            // modo normal
            brushStamp(x, y, color);
        }
        break;

    case TOOL_ERASER:
        if (dragging)
        {
            drawLineSurface(prevtpx, prevtpy, x, y, 0);
        }
        else
        {
            drawPixelSurface(x, y, 0);
        }
        break;

    case TOOL_PICKER:
        if (paletteBpp == 16)
        {
            palette[palettePos] = surface[(y << surfaceXres) + x];
            // paletteAlpha = MAX_ALPHA;
            updatePal(0, &palettePos);
        }
        else
        {
            updatePal(surface[(y << surfaceXres) + x] - color, &palettePos);
        }
        currentTool = TOOL_BRUSH; // volver a seleccionar el pincel
        oamSetXY(&oamSub, selector24oamID, 0, 16);
        oamUpdate(&oamSub);
        break;

    case TOOL_BUCKET:
        if (paletteAlpha != MAX_ALPHA)
        {
            if (paletteAlpha == 0)
            {
                floodFill(surface, x, y, surface[(y << surfaceXres) + x], 0, surfaceXres, surfaceYres);
                break;
            }
            u16 _col = mergeColorAlpha(surface[(y << surfaceXres) + x], color, paletteAlpha);
            floodFill(surface, x, y, surface[(y << surfaceXres) + x], _col, surfaceXres, surfaceYres);
            break;
        }
        else
        {
            floodFill(surface, x, y, surface[(y << surfaceXres) + x], color, surfaceXres, surfaceYres);
        }
        break;
    }
}
//========================================= Herramientas extras=====================|
// copia en el stack una parte de la imagen
void copyFromSurfaceToStack()
{
    hasClipboard = true;
    int zoom = subSurfaceZoom - (7 - MAX(surfaceXres, surfaceYres));

    stackXres = surfaceXres - zoom;
    stackYres = surfaceYres - zoom;

    int stackW = 1 << stackXres;
    int stackH = 1 << stackYres;
    int rowBytes = stackW << 1; // sizeof(u16) = 2

    int baseOffset = subSurfaceXoffset +
                     (subSurfaceYoffset << surfaceXres);

    u16 *src = surface + baseOffset;
    u16 *dst = stack;

    int surfaceStride = 1 << surfaceXres;

    // en este caso copiamos todo de una ya que los bits están alineados
    if (stackW == surfaceStride)
    {
        memcpy(dst, src, rowBytes * stackH);
        return;
    }

    // Copia normal por filas
    for (int y = stackH; y--;)
    {
        memcpy(dst, src, rowBytes);

        dst += stackW;
        src += surfaceStride;
    }
}
void cutFromSurfaceToStack()
{
    // copia pero limpia un fragmento de la pantalla
    // en vez de optimizar esto, lo haré de la manera más simple posible lol
    copyFromSurfaceToStack();
    // limpiar la pantalla
    int blockSize = 128 >> subSurfaceZoom;
    AVdrawRectangleDMA(surface, subSurfaceXoffset, blockSize, subSurfaceYoffset, blockSize, 0, surfaceXres);
    accurate = true;
}

void pasteFromStackToSurface()
{
    if (!hasClipboard)
    {
        return;
    }
    int ysize = 1 << stackYres;
    int xsize = 1 << stackXres;

    for (int i = 0; i < ysize; i++) // eje vertical
    {
        int y = (i << stackXres);                                              // fila en el stack
        int _y = ((i + subSurfaceYoffset) << surfaceXres) + subSurfaceXoffset; // fila en surface con offset

        for (int j = 0; j < xsize; j++)
        {
            surface[_y + j] = stack[y + j];
        }
    }
    accurate = true;
}

// para paletas
inline void copyPalette()
{
    int iterations = (paletteBpp >= 8) ? 256 : (1 << paletteBpp);
    for (int i = 0; i < iterations; i++)
    {
        stack[i] = palette[i + paletteOffset];
    }
}

inline void pastePalette()
{
    int iterations = (paletteBpp >= 8) ? 256 : (1 << paletteBpp);
    for (int i = 0; i < iterations; i++)
    {
        palette[i + paletteOffset] = stack[i];
    }
}

inline void copyColor()
{
    stack[0] = palette[palettePos + paletteOffset];
}

inline void pasteColor()
{
    palette[palettePos + paletteOffset] = stack[0];
}

void flipH()
{
    copyFromSurfaceToStack();

    int ysize = 1 << stackYres;
    int xsize = 1 << stackXres;

    for (int i = 0; i < ysize; i++) // eje vertical
    {
        int y = (i << stackXres);                                              // fila en el stack
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

    for (int i = 0; i < ysize; i++) // eje vertical
    {
        int y = (((ysize - 1) - i) << stackXres);                              // fila en el stack
        int _y = ((i + subSurfaceYoffset) << surfaceXres) + subSurfaceXoffset; // fila en surface con offset

        for (int j = 0; j < xsize; j++)
        {
            surface[_y + j] = stack[y + j];
        }
    }
}

void scaleUp()
{
    copyFromSurfaceToStack();

    int stackW = 1 << stackXres;
    int stackH = 1 << stackYres;

    // felicidades, encontraste la peor línea que verás en tu vida!
    if ((((((stackH - 1) << 1) + 1) + subSurfaceYoffset) << surfaceXres) + ((stackW - 1) << 1) + subSurfaceXoffset > (1 << surfaceXres << surfaceYres))
    {
        return;
    } // fuera de rango

    for (int sy = 0; sy < stackH; ++sy)
    {
        int srcBase = sy * stackW;

        // dos filas destino correspondientes a esta fila fuente
        int dstRow0 = ((sy << 1) + subSurfaceYoffset) << surfaceXres;
        int dstRow1 = (((sy << 1) + 1) + subSurfaceYoffset) << surfaceXres;

        for (int sx = 0; sx < stackW; ++sx)
        {
            u16 pix = stack[srcBase + sx];
            int dstCol = (sx << 1) + subSurfaceXoffset;

            // escribir 2x2
            surface[dstRow0 + dstCol] = pix;
            surface[dstRow0 + dstCol + 1] = pix;
            surface[dstRow1 + dstCol] = pix;
            surface[dstRow1 + dstCol + 1] = pix;
        }
    }
}

#define A_MASK 0x8000
#define R_MASK 0x7C00
#define G_MASK 0x03E0
#define B_MASK 0x001F

void scaleDown()
{
    cutFromSurfaceToStack();
    int baseOffset = subSurfaceXoffset + (subSurfaceYoffset << surfaceXres);
    int _y = 0;
    int offset = 0;
    if (paletteBpp != 16)
    {
        // lee el stack saltandose un pixel
        int yres = (1 << stackYres) >> 1;
        int xres = (1 << stackXres) >> 1;
        for (int y = 0; y < yres; y++)
        {
            // precalcular algunas cosas
            offset = (y << surfaceXres) + baseOffset;
            _y = (y << 1) << stackXres;

            for (int x = 0; x < xres; x++)
            { // dibujar
                surface[offset + x] = stack[_y + (x << 1)];
            }
        }
    }
    else
    {
        int yres = (1 << stackYres) >> 1;
        int xres = (1 << stackXres) >> 1;

        for (int y = 0; y < yres; y++)
        {
            offset = (y << surfaceXres) + baseOffset;

            int row0 = (y << 1) << stackXres;
            int row1 = row0 + (1 << stackXres);

            for (int x = 0; x < xres; x++)
            {
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

void rotatePositive()
{ // 90° Antihorario
    copyFromSurfaceToStack();

    int size = 1 << stackXres;
    int baseOffset = subSurfaceXoffset + (subSurfaceYoffset << surfaceXres);

    for (int y = 0; y < size; y++)
    {
        int destOffset = baseOffset + (y << surfaceXres);
        for (int x = 0; x < size; x++)
        {
            // (x, y) → (y, size-1-x)
            surface[destOffset + x] = stack[((size - 1 - x) << stackXres) + y];
        }
    }
}

void rotateNegative()
{ // 90° Horario
    copyFromSurfaceToStack();

    int size = 1 << stackXres;
    int baseOffset = subSurfaceXoffset + (subSurfaceYoffset << surfaceXres);

    // Para rotar horario, leer desde cuadrante "inferior" hacia la derecha
    for (int y = 0; y < size; y++)
    {
        int destOffset = baseOffset + (y << surfaceXres);
        for (int x = 0; x < size; x++)
        {
            // leer desde cuadrante rotado (x, y) → (size-1-y, x)
            surface[destOffset + x] = stack[(x << stackXres) + (size - 1 - y)];
        }
    }
}
void shiftDownWrap()
{
    copyFromSurfaceToStack();

    int width = 1 << stackXres;
    int height = 1 << stackYres;

    u16 temp[128];
    memcpy(temp, stack + ((height - 1) << stackXres), width * sizeof(u16));

    // Mover filas hacia abajo (esto NO toca la fila 0 todavía)
    for (int y = height - 1; y > 0; y--)
    {
        int current = y << stackXres;
        int prev = (y - 1) << stackXres;

        memcpy(stack + current, stack + prev, width * sizeof(u16));
    }

    // Recién ahora coloco la última fila guardada en la primera
    memcpy(stack, temp, width * sizeof(u16));

    pasteFromStackToSurface();
}

void shiftUpWrap()
{
    copyFromSurfaceToStack();

    int width = 1 << stackXres;
    int height = 1 << stackYres;

    u16 temp[128];
    memcpy(temp, stack, width * sizeof(u16));

    // Mover filas hacia arriba (esto NO toca la última fila todavía)
    for (int y = 0; y < height - 1; y++)
    {
        int current = y << stackXres;
        int next = (y + 1) << stackXres;

        memcpy(stack + current, stack + next, width * sizeof(u16));
    }

    // Recién ahora coloco la primera fila guardada en la última
    memcpy(stack + ((height - 1) << stackXres), temp, width * sizeof(u16));

    pasteFromStackToSurface();
}

void shiftRightWrap()
{
    copyFromSurfaceToStack();

    int width = 1 << stackXres;
    int height = 1 << stackYres;

    for (int y = 0; y < height; y++)
    {
        int row = y << stackXres;

        u16 last = stack[row + width - 1];

        for (int x = width - 1; x > 0; x--)
        {
            stack[row + x] = stack[row + x - 1];
        }

        stack[row] = last;
    }
    pasteFromStackToSurface();
}

void shiftLeftWrap()
{
    copyFromSurfaceToStack();

    int width = 1 << stackXres;
    int height = 1 << stackYres;

    for (int y = 0; y < height; y++)
    {
        int row = y << stackXres;

        u16 first = stack[row];

        for (int x = 0; x < width - 1; x++)
        {
            stack[row + x] = stack[row + x + 1];
        }

        stack[row + width - 1] = first;
    }

    pasteFromStackToSurface();
}
//====================================================================MAIN==================================================================================================================|
//lo quitaré del itcm cuando me falte espacio ahí lol
__attribute__((section(".itcm"))) int main(void)
{
    defaultExceptionHandler(); // Mostrar crasheos
    // Intentar montar la SD
    // Intentar montar primero con DLDI (para flashcards DS/DS Lite)
    bool sd_ok = fatInitDefault();
    if (sd_ok)
    {
        // Verificar si estamos en una flashcard o DSi
        // En flashcards, la raíz suele ser "fat:/"
        // En DSi, es "sd:/"

        // Intentar cambiar a fat:/ primero (flashcards)
        if (chdir("fat:/") == 0)
        {
            currentDir = opendir(".");
        }
        // Si falla, intentar sd:/ (DSi)
        else if (chdir("sd:/") == 0)
        {
            currentDir = opendir(".");
        }
        // Como último recurso, usar la raíz actual
        else
        {
            currentDir = opendir(".");
        }
        createAppFolder();
        clearCache();
    }

    if (!sd_ok)
    {
        // --- Inicializar video temporalmente en modo consola (pantalla superior) ---
        videoSetMode(MODE_0_2D); // modo texto

        // poner pantalla inferior en modo texto temporal
        videoSetModeSub(MODE_0_2D);
        vramSetBankC(VRAM_C_SUB_BG);
        consoleDemoInit();

        printf("\x1b[31m\n"); // Rojo

        printf("ERROR: SD CARD NOT INITIATED.\n");
        printf("\x1b[38m\n"); // blanco
        printf("You cannot load or save files.\n");
        printf("This error may occur on flashcards if DLDI\n");
        printf("patching was not done correctly.\n");

        printf("\nStarting in 3 seconds");
        for (int i = 0; i < 3; i++) // cantidad segundos
        {
            printf(".");
            for (int j = 0; j < 60; j++)
            {
                swiWaitForVBlank();
            }
        }
    }
    // antes de iniciar el programa, mostramos la intro
    intro();
    initGradient();

    initBitmap();
    initFPS();
    initTimers();
    // aclarar la pantalla
    for (int i = 0; i < 16; i++)
    {
        setBrightness(3, i - 15);
        swiWaitForVBlank();
    }
    //========================================================================WHILE LOOP!!!!!!!!!==========================================|
    while(1)
    {
        int actions = ACTION_NONE;
        // input
        scanKeys();
        kDown = keysDown();
        kHeld = keysHeld();
        kUp = keysUp();

        frameEndTime = timerRead();
        drawInfo();
        timerReset();
        frameStartTime = timerRead();
        // verificar si siquiera hay un input en este frame
        if ((kDown | kUp | kHeld) == 0)
        {
            goto frameEnd;
        }
        touchRead(&touch);
        

        // if(screensSwapped == false){
        if (kUp & KEY_TOUCH)
        { // permitir volver a dibujar en un pixel
            prevx = -1;
            prevy = -1;
            stylusHoldTimer = STYLUSHOLDTIME;
            stylusRepeat = false;
        }
        if (kHeld & (KEY_L|KEY_X)) // zoom y offsets
        {
            actions |= getActionsFromKeys(kDown);
        }
        else // paleta de colores
        {
            if (kHeld & KEY_SELECT && nesMode == false)
            { // selector
                if (kDown & (KEY_UP | KEY_DOWN))
                {
                    if (kDown & KEY_UP)
                    {
                        palEditSel--;
                    }
                    else
                    {
                        palEditSel++;
                    }
                    palEditSel &= 3;
                    if (paletteBpp != 16 && palEditSel == 0)
                    {
                        palEditSel = 1;
                    }
                    // draw the rectangle with the selected bar.
                    oamSetXY(&oamSub, rgbSliderSelOamId, SCREEN_W - 2, (palEditSel << 3) + rgbSliderY);
                    oamUpdate(&oamSub);
                    goto frameEnd; // you can only use one input per frame, nothing more to check here!
                }
                if (kDown & (KEY_RIGHT | KEY_LEFT))
                {
                    if (palEditSel != 0)
                    {
                        u8 index = palEditSel - 1;
                        if (kDown & KEY_RIGHT)
                            palEdit[index]++;
                        else
                            palEdit[index]--;

                        palEdit[index] &= 31;
                        updatePalEditBar(index);
                    }
                    else
                    {
                        if (kDown & KEY_RIGHT)
                            paletteAlpha++;
                        else
                            paletteAlpha--;

                        paletteAlpha &= MAX_ALPHA;
                        // el color no cambia, pero la barra sí
                        drawSliderRect(gfxRGBsliders, 0, 0, MAX_ALPHA, palette[palettePos]);
                        drawSliderRect(gfxRGBsliders, paletteAlpha, 0, 64 - paletteAlpha, 0);
                    }
                    goto frameEnd;
                }
            }
            else
            {
                if (kDown & KEY_DOWN)
                {
                    updatePal(16, &palettePos);
                    goto frameEnd;
                }
                if (kDown & KEY_UP)
                {
                    updatePal(-16, &palettePos);
                    goto frameEnd;
                }
                if (kDown & KEY_LEFT)
                {
                    updatePal(-1, &palettePos);
                    goto frameEnd;
                }
                if (kDown & KEY_RIGHT)
                {
                    updatePal(1, &palettePos);
                    goto frameEnd;
                }
            }
        }
        if (kDown & KEY_R || kDown & KEY_Y)
        {
            showGrid = !showGrid;
            if(showGrid)
            {drawGrid(AVinvertColor(palette[paletteOffset]));}else{dmaFillHalfWords(0, gfxGrid, 64 * 64 * 2);}
            goto frameEnd;
        }
        //===========================================PALETAS=========================================================
        palettePos = palettePos & (paletteSize - 1); // mantiene la paleta dentro de un límite
        if (palettePos < 0)
        {
            palettePos = 0;
        }
        // recordar que debo hacer cambios dependiendo del bpp
        if (kHeld & KEY_TOUCH)
        {
            if (stylusHoldTimer > 0)
            {
                stylusHoldTimer--;
            }
            else
            {
                stylusRepeat = true;
            }
            if (touch.px >= SURFACE_X && touch.px < (SURFACE_W + SURFACE_X))
            { // TOUCH EN EL CENTRO!
                if (touch.py <= SURFACE_H)
                { // APUNTA A LA SURFACE!
                    int localX = touch.px - SURFACE_X;
                    int localY = touch.py;

                    int srcX = subSurfaceXoffset + (localX >> subSurfaceZoom);
                    int srcY = subSurfaceYoffset + (localY >> subSurfaceZoom);

                    if (srcY < 1 << surfaceYres) // comprobar si está en el rango (solo por si acaso)
                    {
                        if (!(stylusPressed && prevtpx == srcX && prevtpy == srcY))
                        {
                            applyTool(srcX, srcY, stylusPressed);
                            prevtpx = srcX;
                            prevtpy = srcY;
                            drawSurfaceMain();
                            drew = true;
                            stylusPressed = true;
                        }
                        goto frameEnd;
                    }
                }
                else
                { // apunta a los botones de abajo
                    int row = (touch.py - SURFACE_H) >> 4;
                    int col = (touch.px - SURFACE_X) >> 4;
                    // PLACEHOLDER
                    if (row == 3 && stylusPressed == false)
                    {
                        stylusPressed = true;
                        switch (col)
                        {
                        case 0: // delete frame
                            deleteAnimFrame();
                            break;

                        case 1: // add frame
                            insertAnimFrame();
                            break;

                        case 2: // prev frame
                            prevAnimFrame();
                            break;

                        case 3: // play animation
                            animation.isPlaying = true;
                            playAnimation();
                            break;

                        case 5: // next frame
                            nextAnimFrame();
                            break;

                        case 6: // less speed
                            if (animation.speed > 1)
                                animation.speed--;
                            break;

                        case 7: // more speed
                            animation.speed++;
                            break;
                        }
                    }
                    goto frameEnd;
                }
            }
            if (touch.px < 64) // apunta a la parte izquierda
            {
                if (touch.px < 48 && touch.py > 16 && touch.py < 64 && stylusPressed == false) // herramientas
                {
                    if (showBrushSettings && touch.px >= 16 && touch.px < 48 && touch.py >= 32 && touch.py < 48)
                    { // si está en modo configurar brush
                        int col = (touch.px - 16) >> 3;
                        int row = (touch.py - 32) >> 3;
                        stylusPressed = true;
                        switch (row)
                        {
                        case 0: // patrones
                            brushMode = (BrushMode)col;
                            break;

                        case 1: // tamaños
                            brushSize = (BrushSize)col;
                            break;
                        }
                        setBrushSettingsSprites(true);
                        goto frameEnd;
                    }
                    int col = touch.px > 24 ? 1 : 0;
                    int row = touch.py > 40 ? 2 : 0;
                    // convertir col+row a un valor único
                    ToolType prevTool = currentTool;
                    currentTool = (ToolType)(row + col);
                    if (currentTool == prevTool && currentTool == TOOL_BUCKET)
                    {
                        bucketMode++;
                        if (bucketMode > 2)
                        {
                            bucketMode = 0;
                        }
                    }
                    if (currentTool == TOOL_BRUSH)
                    {
                        setBrushSettingsSprites(true);
                    }
                    else
                    {
                        setBrushSettingsSprites(false);
                    }
                    // además dibujamos un contorno en dónde seleccionamos
                    oamSetXY(&oamSub, selector24oamID, col * 24, (row * 12) + 16);
                    oamUpdate(&oamSub);
                    stylusPressed = true;
                    goto frameEnd;
                }
                else // apunta a otra parte de la izquierda
                {
                    if (touch.py < 16 && stylusPressed == false)
                    { // iconos de la parte superior
                        int selected = touch.px >> 4;
                        fname[0] = '\0'; // quitar nombre reciente :>
                        // actualizar input para que la pantalla también lo haga
                        kDown = kDown | KEY_TOUCH;
                        switch (selected)
                        {
                        case 0: // load file
                            textMode();
                            currentConsoleMode = LOAD_file;
                            textKeyboardDraw();
                            runTextConsole();
                        break;
                        case 1: // New file
                            textMode();
                            currentConsoleMode = MODE_NEWIMAGE;
                            decompress(GFXnewImageInputBitmap, BG_GFX_SUB, LZ77Vram);
                            dmaCopy(GFXnewImageInputPal, BG_PALETTE_SUB, GFXnewImageInputPalLen);
                            runTextConsole();
                        break;
                        case 2: // Save file
                            textMode();
                            currentConsoleMode = SAVE_file;
                            textKeyboardDraw();
                            runTextConsole();
                        break;
                            // PLACEHOLDER el caso 3 está libre por ahora :> (pienso usarlo para configuración en un futuro)
                        }
                    }
                    // botones del costado derecho en la izquierda
                    else if (touch.px >= 48 && touch.py < 64 && stylusPressed == false)
                    {
                        int selected = touch.py >> 4;
                        switch (selected) // puro hardcode lol
                        {
                        case 1: // Copy
                            copyFromSurfaceToStack();
                            break;
                        case 2: // cut
                            cutFromSurfaceToStack();
                            drawSurfaceMain();
                            break;
                        case 3: // Paste
                            pasteFromStackToSurface();
                            drawSurfaceMain();
                            break;
                        }
                        stylusPressed = true;
                        goto frameEnd;
                    }
                    if (touch.py >= 64 && stylusPressed == false) // revisar botones inferiores
                    {
                        // hardcodeado porque lol
                        int selected = touch.px >> 4;
                        selected += ((touch.py - 64) >> 4) << 2;
                        stylusPressed = true;
                        switch (selected)
                        {
                        case 0: // rotate -90°
                            rotateNegative();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 1: // rotate 90°
                            rotatePositive();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 2:
                            flipV();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 3:
                            flipH();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 6:
                            // verificar si es posible escalar
                            scaleUp();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 7:
                            scaleDown();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 8:
                            shiftLeftWrap();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 9:
                            shiftRightWrap();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 10:
                            shiftUpWrap();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 11:
                            shiftDownWrap();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 24: // Page UP
                        case 25: // Page DOWN
                        {
                            if (usesPages)
                            {
                                saveFile(imgFormat, currentFilePath, palette, surface);

                                int dir = (selected == 24) ? -1 : +1;
                                fileOffset += dir * (paletteBpp << 11);

                                loadFile(imgFormat, currentFilePath, palette, surface);
                                drawSurfaceMain();
                                accurate = true;
                            }
                        }
                            goto frameEnd;

                        case 26: // undo
                            backupIndex--;
                            if (backupIndex < 0)
                            {
                                backupIndex = backupMax;
                            }
                            backupRead();
                            drawSurfaceMain();
                            goto frameEnd;

                        case 27: // redo
                            backupIndex++;
                            if (backupIndex > backupMax)
                            {
                                backupIndex = 0;
                            }
                            backupRead();
                            drawSurfaceMain();
                            goto frameEnd;
                        }
                    }
                }
            }
            // zona de paletas y otras configuraciones
            if (touch.px >= 192) // apunta a la parte derecha
            {
                if (touch.py < 32 && (stylusPressed == false || stylusRepeat == true))
                {                                    // botones superiores
                    int col = (touch.px - 192) >> 4; // 0..3
                    int row = touch.py >> 4;         // 0..1
                    if ((unsigned)col < 4 && (unsigned)row < 2)
                    {
                        int button = (row << 2) | col; // 0..7
                        actions |= getActionsFromTouch(button);
                    }
                    stylusPressed = true;
                    goto frameEnd;
                }
                else if (touch.py < 40 && touch.py > 32 && paletteBpp == 16)
                { // transparencia
                    paletteAlpha = (touch.px - 192);

                    drawSliderRect(gfxRGBsliders, 0, 0, MAX_ALPHA, palette[palettePos]);
                    drawSliderRect(gfxRGBsliders, paletteAlpha, 0, 64 - paletteAlpha, 0);
                    goto frameEnd;
                }
                else if (touch.py >= 40 && touch.py < 64) // creador de colores
                {
                    // hay mucho código hardcodeado aquí para mejorar el rendimiento :>
                    if(nesMode)
                    {
                        int ystart = 48;
                        int row = (touch.px - 192) >> 2;
                        int col = (touch.py - ystart) >> 2;
                        int index = (col << 4) + row;

                        u16 _col = nesPalette[index];
                        palette[palettePos] = _col;
                        draw4xRectIn64w(gfxPalette, (palettePos & 15) << 2, (palettePos >> 4) << 2, _col);
                        drawSurfaceMain();

                        goto frameEnd;
                    }
                    else
                    { // creador de colores en modo
                        int index = (touch.py - 40) >> 3;
                        palEdit[index] = (touch.px - 192) >> 1;

                        updatePalEditBar(index);

                        goto frameEnd;
                    }
                }
                else if (touch.py > 128)
                { // botones inferior derecha
                    // obtenemos el indice a base de donde apretamos
                    int row = (touch.px - 192) >> 4;
                    int pos = row;
                    switch (pos)
                    {
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
                    updatePal(0, &palettePos);
                    goto frameEnd;
                }
                else
                { // seleccionar un color en la paleta
                    if (stylusPressed == false)
                    {
                        int row = (touch.py - 64) >> 2;
                        int col = (touch.px - 192) >> 2;

                        updatePal(((row << 4) + col) - palettePos, &palettePos);
                        stylusPressed = true;
                    }
                    goto frameEnd;
                }
            }
        }
        else
        {
            stylusPressed = false;
        }

    frameEnd:
        //actualizamos el coso del preview
        if(previewPosAlpha > 0){
            previewPosAlpha--;
            updatePreviewPos();
        }
        
        updateFPS();

        if (kUp & KEY_TOUCH && drew == true)
        {
            drew = false;
            backupWrite();
        }
        if (actions != ACTION_NONE)
        {
            applyActions(actions);
        }
        timerStop();
        swiWaitForVBlank();//ya no hay modo reposo ya que si no hay input no se hace nada.
        timerContinue();
        if (updated)
        { // llamar a submitVRAM solo si se modificó algo visual
            submitVRAM(accurate);
        }
        updated = false;
        accurate = false;
        // fin del loop de modo bitmap (pantalla de abajo)
        //}
        // else{
        //  screenSwapped
        //}
    }
    /*
    Esta sección está hecha para quienes leyeron el código!

    solo voy a decir que estoy trabajando en ordenar un poco este código
    - Alfombra de marzo
    */
    return 0;
}