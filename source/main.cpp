/*
    ADVERTENCIA: este será el código con más bitshifts y comentarios inecesarios que verás, suerte tratando de entender algo!
     -Alfombracitario, Septiembre de 2025
*/

/*
    URGENTE (para v0.3):
    arreglar brushes en modo 16bpp
    compatibilidad con flashcards r4

    To-Do list (para v1.0):
    reordenamiento de código
    añadir figuras (dos pasos)
        rectangulo
        circulo
        línea
    añadir figuras (tres pasos)
        parabola

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
        aumentar páginas (solo para formatos retro)
        eliminar páginas

    añadir soporte a imagenes más grandes (cargando pedazos)

    añadir un preview para el cargador de archivos
*/

#include <nds.h>
#include <stdio.h>
#include <time.h>
#include <fat.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string>

#include <font.h>

#include "avdslib.h"
#include "formats.h"
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
#define SCREEN_H 256
#define SURFACE_X 64
#define SURFACE_Y 0
#define SURFACE_W 128
#define SURFACE_H 256

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
//===================================================================Variables================================================================
static PrintConsole topConsole;
//static PrintConsole bottomConsole;

//IWRAM, 32KB usados
u16 surface[16384]__attribute__((section(".iwram"))); // 32 KB en IWRAM

//RAM 360,960 bytes usados (en arrays)
u16* pixelsTopVRAM = (u16*)BG_GFX;
u16* pixelsVRAM = (u16*)BG_GFX_SUB;
u16 *gfx32;
u16 *gfx16;
u16 *gfxBG;
u16 pixelsTop[49152];//copia en RAM (es más rápida)
u16 pixels[49152];
u16 palette[256];

u16 stack[16384];// para operaciones temporales
u16 backup[131072];//para undo/redo y cargar imagenes 256kb 
u8 paletteAlpha = MAX_ALPHA;//indicador del alpha actual, útil para 16bpp
//ideal añadir un array para guardar más frames

touchPosition touch;

int backupMax = 8;
int backupSize = 16384;//entry size

int backupIndex = -1;       // índice del último frame guardado
int oldestBackup = 0;       // límite inferior (el frame más antiguo que aún es válido)
int totalBackups = 0;       // cuántos backups se han llenado realmente

//palletas
int paletteSize = 256;
int palettePos = 0;
int paletteBpp = 8;

int paletteOffset = 0;
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

int mainSurfaceXoffset = 0;
int mainSurfaceYoffset = 0;
int subSurfaceXoffset = 0;
int subSurfaceYoffset = 0;
int subSurfaceZoom = 3;// 8 veces más cerca
int sleepTimer = 60;//frames para que se duerma (baje FPS)
int sleepingFrames = 1;//solo espera un frame
int stylusHoldTimer = STYLUSHOLDTIME;
bool stylusRepeat = false;
//Exponentes
//máximo es 7
int surfaceXres = 7;
int surfaceYres = 7;

int stackYres = 7;
int stackXres = 7;


// Variables globales para controlar el modo actual
enum subMode { SUB_TEXT, SUB_BITMAP };
subMode currentSubMode = SUB_BITMAP;

enum consoleMode { MODE_NO, LOAD_file, SAVE_file, IMAGE_SETTINGS, MODE_NEWIMAGE};
consoleMode currentConsoleMode = MODE_NO;

int resX = 7;
int resY = 7;
u32 kDown = 0;
u32 kHeld = 0;
u32 kUp = 0;
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


//archivos
#define MAX_TEXT_LENGTH 32
#define MAX_FILES 256
#define MAX_NAME_LEN 32   // temporal para el nombre completo del archivo
#define SCREEN_LINES 20   // cuántos archivos caben en pantalla

char fname[MAX_TEXT_LENGTH];

DIR* currentDir = NULL;
struct dirent entryList[MAX_FILES];  // usamos memoria estática
int fileCount = 0;

char path[257] = "/";  // ruta actual
char format[6];
char currentFilePath[257];

int selector = 0;//selector para la consola
int selectorA = 0;//selector secundario

void buildCurrentFilePath(void) {
    currentFilePath[0] = '\0';
    strcat(currentFilePath, path);
    strcat(currentFilePath, fname);
    strcat(currentFilePath, format);
}

int enterFolder(int index) {
    if(index < 0 || index >= fileCount) return 0;

    if(entryList[index].d_type == DT_DIR) {
        char newPath[256];
        snprintf(newPath, sizeof(newPath), "%s%s/", path, entryList[index].d_name);
        strncpy(path, newPath, sizeof(path));

        // abrir nuevo directorio
        if(currentDir) closedir(currentDir);
        currentDir = opendir(path);
        selector = 0;
        return 1; // carpeta abierta
    }
    return 0; // no es carpeta
}
int goBack() {
    if(strcmp(path, "/") == 0) return 0; // ya en raíz

    // quitar última carpeta
    char* lastSlash = strrchr(path, '/');
    if(lastSlash != path) {
        *lastSlash = '\0';
        lastSlash = strrchr(path, '/');
        *(lastSlash+1) = '\0';
    } else {
        path[1] = '\0';
    }

    // abrir carpeta padre
    if(currentDir) closedir(currentDir);
    currentDir = opendir(path);
    selector = 0;
    return 1;
}
void listFiles() {
    if(!currentDir) return;

    fileCount = 0;
    struct dirent* ent;
    rewinddir(currentDir);

    // Leer entradas
    while((ent = readdir(currentDir)) != NULL && fileCount < MAX_FILES) {
        entryList[fileCount++] = *ent;
    }

    // --- (1) Ordenar alfabéticamente ---
    for(int i = 0; i < fileCount - 1; i++) {
        for(int j = i + 1; j < fileCount; j++) {
            // Ordenar carpetas y archivos juntos alfabéticamente
            if(strcasecmp(entryList[i].d_name, entryList[j].d_name) > 0) {
                struct dirent temp = entryList[i];
                entryList[i] = entryList[j];
                entryList[j] = temp;
            }
        }
    }

    // --- (2) Mostrar en pantalla ---
    int start = selector;
    if(start + SCREEN_LINES > fileCount) start = fileCount - SCREEN_LINES;
    if(start < 0) start = 0;

    for(int i = 0; i < SCREEN_LINES && (start + i) < fileCount; i++) {
        const char* name = entryList[start + i].d_name;
        bool isDir = (entryList[start + i].d_type == DT_DIR);
        char displayName[32];

        if(isDir) {
            // Carpeta: recortar si es demasiado largo
            int len = strlen(name);
            if(len > 27) { // deja 4 para "..."
                snprintf(displayName, sizeof(displayName), "%.27s.../", name);
            } else {
                snprintf(displayName, sizeof(displayName), "%s/", name);
            }
        } else {
            // Archivo: mantener extensión visible
            const char* dot = strrchr(name, '.');
            if(dot && strlen(name) > 27) {
                // nombre demasiado largo: mostrar parte inicial + extensión
                int extLen = strlen(dot);
                int keep = 27 - extLen - 3; // 3 para "..."
                if(keep < 1) keep = 1;
                snprintf(displayName, sizeof(displayName), "%.*s...%s", keep, name, dot);
            } else if(strlen(name) > 27) {
                snprintf(displayName, sizeof(displayName), "%.23s...", name);
            } else {
                strcpy(displayName, name);
            }
        }

        // Mostrar con selector
        if(start + i == selector)
            printf("> %s\n", displayName);
        else
            printf("  %s\n", displayName);
    }
}


void OnKeyPressed(int key) {
if ( key < 0 ) return;
    switch(key) {
    case '\b':
        if (selector > 0 ) {
            selector--;
            fname[selector] = '\0';
        }
    break;
    case '\n':
    case '\r':
        if (selector < MAX_TEXT_LENGTH)
        {
            fname[selector++] = '\n';
            fname[selector] = '\0';           
        }
    default:
        if (selector < MAX_TEXT_LENGTH)
        {
            fname[selector++] = (char)key;
            fname[selector] = '\0';
        }
    }
}
//función para pasar copiar datos rápidamente
extern "C" void memcpy_fast_arm9(const void* src, void* dst, unsigned int bytes);

void submitVRAM(bool full = false, bool _accurate = false, bool both = true)
{
    // ====== Cálculo de offsets y tamaños ======
    const int offset = (mainSurfaceYoffset << 8) + mainSurfaceXoffset; // offset de la pantalla superior
    const int sizeTop = 512 << surfaceYres;   // Tamaño top
    int sizeBottom = full ? 98304 : 65536;

    u16* srcTop = pixelsTop + offset;
    u16* dstTop = pixelsTopVRAM + offset;
    u16* srcBottom = pixels;
    u16* dstBottom = pixelsVRAM;

    // ====== Flush de caché antes de transferir ======
    if (_accurate) {
        if (both) DC_FlushRange(srcTop, sizeTop);
        DC_FlushRange(srcBottom, sizeBottom);
    }

    // ====== Asegurar que DMA0 y DMA1 estén detenidos ======
    DMA0_CR = 0;
    DMA1_CR = 0;

    // ====== Iniciar DMA ======
    if (both) {
        // Top screen
        DMA0_SRC  = (u32)srcTop;
        DMA0_DEST = (u32)dstTop;
        DMA0_CR   = (sizeTop >> 1) | DMA_ENABLE; // Halfwords
    }

    // Bottom screen
    DMA1_SRC  = (u32)srcBottom;
    DMA1_DEST = (u32)dstBottom;
    DMA1_CR   = (sizeBottom >> 1) | DMA_ENABLE; // Halfwords

    // ====== Esperar a que terminen ======
    if (both) {
        while ((DMA0_CR & DMA_ENABLE) || (DMA1_CR & DMA_ENABLE));
    } else {
        while (DMA1_CR & DMA_ENABLE);
    }

    // ====== Flush final si VRAM se usará en GPU inmediatamente ======
    if (_accurate) {
        if (both) DC_FlushRange(dstTop, sizeTop);
        DC_FlushRange(dstBottom, sizeBottom);
    }
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

inline void brushStamp2(int x,int y,u16 color)
{
    drawPixelSurface(x  ,y  ,color);
    drawPixelSurface(x+1,y  ,color);
    drawPixelSurface(x  ,y+1,color);
    drawPixelSurface(x+1,y+1,color);
}

inline void brushStamp3Square(int x,int y,u16 color)
{
    for(int oy=-1; oy<=1; oy++)
    for(int ox=-1; ox<=1; ox++)
        drawPixelSurface(x+ox,y+oy,color);
}

inline void brushStamp3Circle(int x,int y,u16 color)
{
    drawPixelSurface(x  ,y  ,color);
    drawPixelSurface(x+1,y  ,color);
    drawPixelSurface(x  ,y+1,color);
    drawPixelSurface(x-1,y,color);
    drawPixelSurface(x,y-1,color);
}
inline void brushStamp4Circle(int x,int y,u16 color)
{
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

inline void brushStamp4Square(int x,int y,u16 color)
{
    for(int oy = -1; oy <= 2; oy++)
    for(int ox = -1; ox <= 2; ox++)
        drawPixelSurface(x + ox, y + oy, color);
}


inline void brushStamp(int x,int y,u16 color)
{
    bool forceSquare = (brushMode == BRUSH_MODE_DITHER);

    switch(brushSize)
    {
        case BRUSH_SIZE_1:
            drawPixelSurface(x,y,color);
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


void drawLineSurfaceAlpha(int x0, int y0, int x1, int y1, u16 color){
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        u16 col = mergeColorAlpha(surface[(y0<<surfaceXres) + x0],color,paletteAlpha);
        brushStamp(x0,y0,col);

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

//por algún motivo inline bajaba el rendimiento
void drawSurfaceMain(bool full = true)
{
    both = true;//debe actualizar ambas pantallas
    int xres = 1<<surfaceXres;   // ancho
    int yres = 1<<surfaceYres;   // alto

    if(paletteBpp == 16){//color directo
        for(int i = 0; i < yres; i++) // eje Y
        {
            int _y = i <<surfaceXres; // fila en surface
            int y  = ((i+mainSurfaceYoffset)<<8) + mainSurfaceXoffset; // fila en pantalla

            for(int j = 0; j < xres; j++) // eje X
            {
                pixelsTop[y+j] = surface[_y+j];
            }
        }
    }
    else if(paletteOffset == 0)
    {
        for(int i = 0; i < yres; i++) // eje Y
        {
            int _y = i <<surfaceXres; // fila en surface
            int y  = ((i+mainSurfaceYoffset)<<8) + mainSurfaceXoffset; // fila en pantalla

            for(int j = 0; j < xres; j++) // eje X
            {
                pixelsTop[y+j] = palette[surface[_y+j]];
            }
        }
    }
    else
    {
        for(int i = 0; i < yres; i++) // eje Y
        {
            int _y = i <<surfaceXres; // fila en surface
            int y  = ((i+mainSurfaceYoffset)<<8) + mainSurfaceXoffset; // fila en pantalla

            for(int j = 0; j < xres; j++) // eje X
            {
                pixelsTop[y+j] = palette[paletteOffset + surface[_y+j]];
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

    u16 *srcBase = pixelsTop + ((mainSurfaceYoffset + subSurfaceYoffset) << 8) + (mainSurfaceXoffset + subSurfaceXoffset);
    u16 *dstBase = pixels + 64; // centrado

    switch (subSurfaceZoom) {
        case 0://si no hay zoom
            for (int i = 0; i < 128; i++) {//sí, siempre copiar las 128, es para arreglar errores visuales
                u16* src = pixelsTop + ((i + mainSurfaceYoffset) << 8) + mainSurfaceXoffset;
                u16* dst = pixels + ((i << 8) + 64);
                memcpy_fast_arm9(src, dst, 256);
            }
            break;

        case 1: drawSurfaceBottom2x(srcBase, dstBase, 512, 512); break;
        case 2: drawSurfaceBottom4x(srcBase, dstBase, 512, 512); break;
        case 3: drawSurfaceBottom8x(srcBase, dstBase, 512, 512); break;
        case 4: drawSurfaceBottom16x(srcBase, dstBase, 512, 512); break;
        case 5: drawSurfaceBottom32x(srcBase, dstBase, 512, 512); break;

        default:
            // fórmula general
            {
                int blockSize = 128 >> subSurfaceZoom;
                int yrepeat   = 1 << subSurfaceZoom;
                int xoffset   = subSurfaceXoffset;
                int yoffset   = subSurfaceYoffset;

                int dstY = 0;

                for (int i = 0; i < blockSize; i++) {
                    int srcY = i + mainSurfaceYoffset + yoffset;
                    u16* srcRow = pixelsTop + (srcY << 8) + mainSurfaceXoffset + xoffset;

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

        AVdrawRectangle(pixels,192,64,32,8,_col);//rectángulo de arriba (color mezclado)
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
//=================================================================Inicialización===================================================================================|
void clearTopBitmap()
{
    u8 r = 16;
    u8 b = 16;
    u16 _col = 0;
    for(int j = 0; j < 192; j++)//toda la pantalla superior (sí, así de optimizado lol)
    {
        if(j < 176)
        {
            if(r > 0) {r-=2;}
            if(b > 0) {b--;}   
        }
        else
        {
            if(r < 16 && j >= 184) {r+=2;}
            if(b < 16) {b++;}   
        }
        _col = AVARGB(r,0,b);
        AVfillDMA(pixelsTopVRAM,j<<8,(((j+1)<<8)-1),_col);//dibuja directamente en la VRAM
    }
}
void centerCanvas(){
    mainSurfaceXoffset = 128-((1<<surfaceXres)>>1);
    mainSurfaceYoffset = 96-((1<<surfaceYres)>>1);
}
void initBitmap()
{
    for(int i = 0; i < 16383; i++)
    {
        surface[i] = 0;
        stack[i] = 0;
    }
    for(int i = 0; i < paletteSize; i++) {
        palette[i] = C_BLACK;
    }
    for(int i = 0; i < 49152; i++)
    {
        pixels[i] = 0;
        pixelsTop[i] = 0;
        pixelsTopVRAM[i] = 0;
        pixelsVRAM[i] = 0;
    }

    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_SPRITE); // sprites en VRAM B
    bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    videoSetModeSub(MODE_5_2D);             // pantalla inferior bitmap
    vramSetBankC(VRAM_C_SUB_BG);
    vramSetBankD(VRAM_D_SUB_SPRITE); // sprites en VRAM D
    bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    // Cargar imagen inicial en VRAM
    decompress(GFXinputBitmap, BG_GFX_SUB, LZ77Vram);

    // Transferir pixels a la RAM
    dmaCopyHalfWords(3,pixelsVRAM,pixels , 49152 * sizeof(u16));

    centerCanvas();
    clearTopBitmap();

    drawSurfaceMain(true);
    drawSurfaceBottom();
    accurate = true;
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
ConsoleFont font = {
    .gfx = fontTiles,
    .pal = fontPal,
    .numColors = fontPalLen>>1,
    .bpp = 4,
    .asciiOffset = 32,
    .numChars = fontTilesLen>>5,
};
inline void textMode()
{
    if(currentSubMode == SUB_TEXT) return; // ya estamos en texto
    currentSubMode = SUB_TEXT;
    //matamos ambas pantallas por que... why not :)

    videoSetMode(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    consoleInit(&topConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
    consoleSetFont(&topConsole, &font);


    printf("Press any key\nor touch the screen \nto update the text console.");
    oamClear(&oamSub, 0, 128);
    sleepingFrames = 1;
    sleepTimer = 60;
}
int bgPreview;

void textKeyboardDraw(){
    //planeo tener una capa extra para poder hacer preview
    int bg2 = bgInitSub(2, BgType_Bmp8, BgSize_B8_256x256, 0, 0);

    //hay espacio para una capa 16bpp de 128x128!!11!

    dmaCopy(GFXconsoleInputPal, BG_PALETTE_SUB, GFXconsoleInputPalLen);
    dmaCopy(GFXconsoleInputBitmap, bgGetGfxPtr(bg2), GFXconsoleInputBitmapLen);
}
inline void bitmapMode()
{
    if(currentSubMode == SUB_BITMAP) return; // ya estamos en bitmap
    currentSubMode = SUB_BITMAP;

    //reiniciamos VRAM
    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_SPRITE); // sprites en VRAM B
    bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    videoSetModeSub(MODE_5_2D);             // pantalla inferior bitmap
    vramSetBankC(VRAM_C_SUB_BG);
    vramSetBankD(VRAM_D_SUB_SPRITE); // sprites en VRAM D
    bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    clearTopBitmap();//redibujar el fondo
    setEditorSprites();
    paletteAlpha = MAX_ALPHA;
    submitVRAM(true,true,true);//recuperamos nuestros queridos datos
    if(paletteBpp == 16){
        setOamBG();
    }
}
//============================================================= SD CARD ===============================================|
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

void saveFile(int format,char* path,u16* palette,u16* surface){
    switch(format){
        default:
            return;
        break;
        case formatDirectBMP:
            saveBMP(path,palette,surface);//Gracias Zhennyak!
        break;
        case format8bppBMP:
            saveBMP_indexed(path,palette,surface);
        break;
        case format4bppBMP:
            saveBMP_4bpp(path,palette,surface);
        break;
        case formatNES:
            exportNES(path,surface,1<<surfaceYres);
        break;
        case formatGBC:
            exportGBC(path,surface,1<<surfaceYres);
        break;
        case formatSNES4:
            exportSNES(path,surface,1<<surfaceYres);
        break;
        case formatGBA4:
            exportGBA(path,surface,1<<surfaceYres);
        break;
        case formatPCX:                                                                           
            exportPCX(path,surface,1<<surfaceXres,1<<surfaceYres);
        break;
        case formatPAL:
            exportPal(path);
        break;
        case formatSNES8:
            exportSNES8bpp(path,surface,1<<surfaceYres);
        break;
        case formatRAW:
            loadArray(path,surface,2<<surfaceXres<<surfaceYres);
        break;
        case formatACS:
            exportACS(path,surface);
        break;
    }
}
void loadFile(int format,char* path,u16* palette,u16* surface){
    switch(format)
    {
        default:
            printf("\nNot supported!");
        break;
        case formatDirectBMP:
            loadBMP_direct(path,surface);
            paletteBpp = 16;usesPages = false;
        break;
        case format8bppBMP:
            loadBMP_indexed(path,palette,surface);
            paletteBpp = 8;usesPages = false;
        break;
        case format4bppBMP:
            loadBMP_4bpp(path,palette,surface);
            paletteBpp = 4;usesPages = false;
        break;
         case formatNES:
            paletteBpp = 2;nesMode = true;usesPages = true;
            importNES(path,surface);
            drawNesPalette();
        break;
        case formatGBC:
            paletteBpp = 2;usesPages = true;
            importGBC(path,surface);
        break;
        case formatSNES4:
            paletteBpp = 4;usesPages = true;
            importSNES(path,surface);
        break;
        case formatGBA4:
            paletteBpp = 4;usesPages = true;
            importGBA(path,surface);
        break;
        case formatPCX:    
            paletteBpp = 8;usesPages = false;
            importPCX(path,surface);
        break;
        case formatPAL:
            importPal(path);
        break;
        case formatSNES8:usesPages = true;
            importSNES8bpp(path,surface);
        break;
        case formatRAW:usesPages = false;
            saveArray(path,surface,32768);
        break;
        case formatACS:usesPages = false;
            importACS(path,surface);
        break;
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
        case 7://unused

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
            // aquí podrías procesar el texto completo, por ejemplo:
            // guardarTexto(fname);
            // limpiar el buffer si quieres:
            fname[0] = '\0';
            break;

        case '<': // salir
            // aquí pones tu código de salida o retorno
            // por ejemplo:
            // returnToMenu();
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
void floodFill(u16 *surface, int x, int y, u16 oldColor, u16 newColor, int xres, int yres) {
    if (oldColor == newColor) return;

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
                if(paletteAlpha != MAX_ALPHA){
                    if(paletteAlpha == 0){
                        brushStamp(x,y,0);
                        break;
                    }
                    u16 col = mergeColorAlpha(surface[(y << surfaceXres) + x],color,paletteAlpha);
                    brushStamp(x,y,col);
                    break;
                }
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
void copyFromSurfaceToStack() {
    int zoom = subSurfaceZoom-(7-MAX(surfaceXres,surfaceYres));//arreglar el zoom a todas las resoluciones
    int xsize = surfaceXres - zoom;
    int ysize = surfaceYres - zoom;
    int xoffset = subSurfaceXoffset;
    int yoffset = subSurfaceYoffset;
    stackXres = xsize;
    stackYres = ysize;

    // Convertir a valores reales
    int stackW = 1 << xsize;
    int stackH = 1 << ysize;
    int surfaceW = 1 << surfaceXres;

    u16* dst = stack;
    u16* src = surface + (yoffset * surfaceW + xoffset);

    for (int y = 0; y < stackH; ++y) {
        // Copia una fila entera
        memcpy(dst, src, stackW * sizeof(u16));

        // Avanzar una fila en ambos
        dst += stackW;
        src += surfaceW;
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

void flipH() {
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
//====================================================================FPS==================================================================|
int fps = 0;
int frameCount = 0;
time_t lastTime = 0;

void initFPS() {
    lastTime = time(NULL);
    frameCount = 0;
    fps = 0;
}

void updateFPS() {
    frameCount++;
    time_t now = time(NULL);
    if(now != lastTime) {  // pasó 1 segundo real
        fps = frameCount;
        frameCount = 0;
        lastTime = now;
    }
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
    //aclarar la pantalla
    for(int i = 0; i < 16; i++)
    {
        setBrightness(3, i-15);
        swiWaitForVBlank();
    }
    //========================================================================WHILE LOOP!!!!!!!!!==========================================|
    while(1) {
        //inicio del loop (global)
        //input
        scanKeys();
        kDown = keysDown();
        kHeld = keysHeld();
        kUp = keysUp();
        touchRead(&touch);
        int actions = ACTION_NONE;

        if(currentSubMode == SUB_BITMAP)
        {
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
                if (touch.px >= SURFACE_X && touch.px < (SURFACE_W + SURFACE_X)) {//TOUCH EN SURFACE
                    int localX = touch.px - SURFACE_X;
                    int localY = touch.py;

                    int srcX = subSurfaceXoffset + (localX >> subSurfaceZoom);
                    int srcY = subSurfaceYoffset + (localY >> subSurfaceZoom);

                    if(srcY < 1<<surfaceYres)//comprobar si está en el rango
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
                        currentTool = (ToolType)(row + col);
                        if(currentTool == TOOL_BRUSH){
                            setBrushSettingsSprites(true);
                        }else{
                            setBrushSettingsSprites(false);
                        }
                        //además dibujamos un contorno en dónde seleccionamos
                        oamSetXY(&oamSub,selector24oamID,col*24, (row*12)+16);
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

                                case 24: // Page UP
                                case 25: // Page DOWN
                                {
                                    if(usesPages){
                                        saveFile(imgFormat, currentFilePath, palette, surface);

                                        int dir = (selected == 24) ? -1 : +1;
                                        fileOffset += dir * (paletteBpp << 11);

                                        loadFile(imgFormat, currentFilePath, palette, surface);
                                        drawSurfaceMain(true);
                                        drawSurfaceBottom();
                                        accurate = true;
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
                    if(touch.py < 40 && touch.py > 32 && paletteBpp == 16){//transparencia
                        paletteAlpha = (touch.px-192);

                        AVdrawRectangle(pixels,192,63,32,8,palette[palettePos]);
                        //Limpiar el area
                        AVdrawRectangle(pixels,192+paletteAlpha,64-paletteAlpha,32,8,0);
                        updated = true;
                        goto frameEnd;
                    }
                    else if(touch.py >= 40 && touch.py < 64)//creador de colores
                    {
                        //hay mucho código hardcodeado aquí para mejorar el rendimiento
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

                        }else{
                            int index = (touch.py-40)>>3;
                            int amount = (touch.px-192)>>1;
                            palEdit[index] = amount;

                            //actualizar barra
                            u16 _barCol[3] = { C_RED, C_GREEN, C_BLUE };
                            AVdrawRectangle(pixels,192,amount<<1,(index<<3)+40,8,_barCol[index]);
                            //Limpiar el area
                            AVdrawRectangle(pixels,192+(amount<<1),64-(amount<<1),(index<<3)+40,8,C_BLACK);

                            //actualizar el color
                            u16 _col = palEdit[0];
                            _col += palEdit[1]<<5;
                            _col += palEdit[2]<<10;
                            _col += 32768;//encender pixel
                            palette[palettePos] = _col;
                            
                            AVdrawRectangle(pixels,192+((palettePos & 15)<<2),4, 64+((palettePos>>4)<<2) ,4,_col);

                            if(paletteBpp != 16){//Solo actualizar en modo index
                                drawSurfaceMain(true);
                                drawSurfaceBottom();
                                //dibujar arriba el nuevo color generado
                                AVdrawRectangle(pixels,192,64,32,8,_col);
                            }else{
                                updated = true;
                                //color seleccionado/alpha
                                AVdrawRectangle(pixels,192,63,32,8,_col);
                                AVdrawRectangle(pixels,192+paletteAlpha,64-paletteAlpha,32,8,C_BLACK);
                                if(palettePos == 0 && showGrid == true){
                                    drawGrid(AVinvertColor(_col));
                                }
                            }

                            //dibujar el contorno del color seleccionado
                            _col = AVinvertColor(_col);
                            AVdrawRectangleHollow(pixels,192+((palettePos & 15)<<2),4, 64+((palettePos>>4)<<2) ,4,_col);
                            goto frameEnd;
                        }
                    }
                    else//seleccionar un color en la paleta
                    {
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

            frameEnd://finalizar el frame del modo bitmap

            if(kUp & KEY_TOUCH && drew == true){
                drew = false;
                backupWrite();
            }
            if (actions != ACTION_NONE) {
                applyActions(actions);
                drawSurfaceBottom();
            }
            //implementación del modo reposo para ahorrar energía
            for(int i = 0; i<sleepingFrames; i++){
                swiWaitForVBlank();
            }
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
            //fin del loop de modo bitmap
        }
        else//=======================================CONSOLA DE TEXTO=======================================>
        {
            textConsole:
                bool redraw = false;
                if(kUp){holdTimer = 0;}
                if(kDown){
                    redraw = true;
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
                    redraw = true;
                    consoleClear();
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
                        surfaceXres = resX;
                        surfaceYres = resY;
                        paletteBpp = selectorA;
                        subSurfaceZoom = 7-MAX(surfaceXres,surfaceYres);//limitar el zoom
                        //se inicia un nuevo lienzo
                        initBitmap();
                        bitmapMode();//simplemente salir de este menú
                        drawColorPalette();
                        if(selector == 3){
                            nesMode = true;
                            drawNesPalette();
                        }
                        selector = 0;
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
                        }
                    }
                    else{
                        if(kDown & KEY_START)//se carga el archivo
                        {
                            buildCurrentFilePath();

                            nesMode = false;
                            if(selectorA == formatPAL){
                                loadFile(formatPAL,currentFilePath,palette,surface);
                            }
                            else{
                                imgFormat = selectorA;
                                loadFile(imgFormat,currentFilePath,palette,surface);
                            }

                            updated = true;
                            drawSurfaceMain(true);drawSurfaceBottom();
                            drawColorPalette();
                            setBackupVariables();
                            bitmapMode();
                        }
                    }
                    //input general

                    #define MaxFormats 12

                    if(holdTimer > 10){
                        if(kHeld & KEY_RIGHT && selectorA < (MaxFormats-1)){selectorA++;}
                        if(kHeld & KEY_LEFT && selectorA > 0){selectorA--;}
                        if(kHeld & KEY_UP && selector > 0){selector--;}
                        if(kHeld & KEY_DOWN){selector++;}
                    }
                    else{
                        if(kDown & KEY_RIGHT && selectorA < (MaxFormats-1)){selectorA++;}
                        if(kDown & KEY_LEFT && selectorA > 0){selectorA--;}
                        if(kDown & KEY_UP && selector > 0){selector--;}
                        if(kDown & KEY_DOWN){selector++;}
                    }

                    const char texts[MaxFormats][24] = {
                        ".bmp [direct]",
                        ".bmp [8bpp]",
                        ".bmp [4bpp]",
                        ".bin [NES]",
                        ".bin [GB 2bpp]",
                        ".bin [SNES 4bpp]",
                        ".bin [GBA 4bpp]",
                        ".pcx",
                        ".pal [YY-CHR]",
                        ".bin [SNES 8bpp]",
                        ".raw",
                        ".acs [Custom format]"
                    };
                    const char formats[MaxFormats][6] = {
                        ".bmp",
                        ".bmp",
                        ".bmp",
                        ".bin",
                        ".bin",
                        ".bin",
                        ".bin",
                        ".pcx",
                        ".pal",
                        ".gif",
                        ".tga",
                        ".acs"
                    };
                    strcpy(format,formats[selectorA]);
                    //obtener información del teclado
                    if(kDown & KEY_TOUCH){
                        if(touch.py > 112){
                            char key = getKeyboardKey(touch.px, touch.py);
                            if (key != '\0') handleKey(key);
                        }
                    }
                    if(kDown & KEY_A) {
                        if(selector < 0 || selector >= fileCount) {
                            printf("Error");
                        } else {
                            if(entryList[selector].d_type == DT_DIR) {
                                enterFolder(selector);
                            } else {
                                // Si es archivo, guarda el nombre sin modificar el path
                                strncpy(fname, entryList[selector].d_name, sizeof(fname));
                                // No concatenes al path, solo usa path como carpeta actual
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
                        printf(fname);//nombre del archivo
                        printf(texts[selectorA]);//extensión + información extra
                        //separar para mostrar el explorador
                        printf("\n????????????????????????????????\n");
                        listFiles();//dibujar información del explorador
                    }
                }
        //final del textConsole
        swiWaitForVBlank();
        }
        oamUpdate(&oamSub);
    }
    /*
    esta sección la dedico a quien sea que haya leido todo mi código, sea una persona con tiempo libre o una IA
        y sí, vengo a justificar algunas de mis horribles practicas.
        en estos meses de desarrollo he aprendido muchas cosas, este fué mi primer proyecto para la DS y me gustaría hablar de
        desiciones que tomé al hacer el código y posibles preguntas

    1. ¿por qué modo bitmap16bpp para toda la pantalla?
        es por flexibilidad y además, es más fácil, no debo ahorrar VRAM realmente...

    2. ¿por qué uso GOTO?
        sé que es considerado una malísima practica, y en varios casos es verdad.
        Sin embargo, si te fijas bien, en este códgio solo hay 2 labels
        textConsole y frameEnd, estos saltos de hecho son muy útiles, irónicamente para ordenar más el proyecto y
        porque pueden mejorar el rendimiento considerablemente, ya que generalmente los uso para saltarme código que no necesito ejecutar

    3. ¿por qué tantos magic numbers?
        esta es mala mía 100%, en los proyectos en paralelo que he hecho esto ya lo hago JAJAJ

    4. por qué está casi todo en main.cpp?
        un poco de lo que decía antes de la 1, es mi primer código en el que uso más de un archivo de hecho.
    
    5. por qué haces estos comentarios tontos o cosas sin sentido como esta?
        programo por diversión y mi único compañero soy yo mismo!
        imagínate llevar programando en el mismo archivo por meses, en algún momento te pondrás chistoso.
    */
   return 0;
}