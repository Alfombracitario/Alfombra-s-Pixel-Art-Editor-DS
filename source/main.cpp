/*
    ADVERTENCIA: este será el código con más bitshifts y comentarios inecesarios que verás, suerte tratando de entender algo!
     -Alfombra de madeera, Septiembre de 2025
*/

/*
    To-Do list:
    No redibujar todo al pintar
    Poder cambiar el tamaño o tipo de pincel
    mejorar el sistema de guardado/cargado
    arreglar el grid

*/
#include <nds.h>
#include <stdio.h>
#include <time.h>
#include <fat.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string>

#include "avdslib.h"
#include "formats.h"
#include "GFXinput.h"
#include "GFXconsoleInput.h"
#include "GFXselector24.h"
#include "GFXselector16.h"
#include "GFXnewImageInput.h"

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

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

//===================================================================Variables================================================================
static PrintConsole topConsole;
//static PrintConsole bottomConsole;

//IWRAM, 32KB usados
u16 surface[16384]__attribute__((section(".iwram"))); // 32 KB en IWRAM

//RAM 360,960 bytes usados
u16* pixelsTopVRAM = (u16*)BG_GFX;
u16* pixelsVRAM = (u16*)BG_GFX_SUB;
u16 *gfx32;
u16 *gfx16;
u16 pixelsTop[49152];//copia en RAM (es más rápida)
u16 pixels[49152];
u16 palette[256];

u16 stack[16384];// para operaciones temporales
u16 backup[131072];//para undo/redo y cargar imagenes 256kb 
//ideal añadir un array para guardar más frames

touchPosition touch;

int backupMax = 8;
int backupSize = 16384;

int backupIndex = -1;       // índice del último frame guardado
int oldestBackup = 0;       // límite inferior (el frame más antiguo que aún es válido)
int totalBackups = 0;       // cuántos backups se han llenado realmente

int stackSize = 16384;

//palletas
int paletteSize = 256;
int palettePos = 0;
int paletteBpp = 8;
int paletteOffset = 0;
bool nesMode = false;
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

int mainSurfaceXoffset = 0;
int mainSurfaceYoffset = 0;
int subSurfaceXoffset = 0;
int subSurfaceYoffset = 0;
int subSurfaceZoom = 3;// 8 veces más cerca


//Solo acepto potencias de 2   >:3
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
bool acurrate = false;
bool mayus = false;
int gridSkips = 0;

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


char text[16];
//archivos
#define MAX_FILES 256
#define MAX_NAME_LEN 32   // temporal para el nombre completo del archivo
#define SCREEN_LINES 20   // cuántos archivos caben en pantalla

DIR* currentDir = NULL;
struct dirent entryList[MAX_FILES];  // usamos memoria estática
int fileCount = 0;

char path[257] = "/";  // ruta actual
char format[6];

int selector = 0;//selector para la consola
int selectorA = 0;//selector secundario

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
                snprintf(displayName, sizeof(displayName), "%.24s.../", name);
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
                snprintf(displayName, sizeof(displayName), "%.27s...", name);
            } else {
                strcpy(displayName, name);
            }
        }

        // Mostrar con selector
        if(start + i == selector)
            iprintf("> %s\n", displayName);
        else
            iprintf("  %s\n", displayName);
    }
}


void OnKeyPressed(int key) {
if ( key < 0 ) return;
    switch(key) {
    case '\b':
        if (selector > 0 ) {
            selector--;
            text[selector] = '\0';
        }
    break;
    case '\n':
    case '\r':
        if (selector < 16)
        {
            text[selector++] = '\n';
            text[selector] = '\0';           
        }
    default:
        if (selector < 16)
        {
            text[selector++] = (char)key;
            text[selector] = '\0';
        }
    }
}
//función para pasar copiar datos rápidamente
extern "C" void memcpy_fast_arm9(const void* src, void* dst, unsigned int bytes);

void submitVRAM(bool full = false, bool _acurrate = false)
{
    // ====== Cálculo de offsets y tamaños ======
    const int offset = (mainSurfaceYoffset << 8) + mainSurfaceXoffset;//offset de la pantalla superior
    const int sizeTop = 512 << surfaceYres;   // Tamaño top
    int sizeBottom = full ? 98304 : 65536;

    u16* srcTop = pixelsTop + offset;
    u16* dstTop = pixelsTopVRAM + offset;
    u16* srcBottom = pixels;
    u16* dstBottom = pixelsVRAM;

    // ====== Flush de caché antes de transferir ======
    if(_acurrate == true){
        DC_FlushRange(srcTop, sizeTop);
        DC_FlushRange(srcBottom, sizeBottom);
    }

    // ====== Asegurar que DMA0 y DMA1 estén detenidos ======
    DMA0_CR = 0;
    DMA1_CR = 0;

    // ====== Iniciar DMA0 -> pantalla superior ======
    DMA0_SRC  = (u32)srcTop;
    DMA0_DEST = (u32)dstTop;
    DMA0_CR   = (sizeTop >> 1) | DMA_ENABLE;  // Halfwords

    // ====== Iniciar DMA1 -> pantalla inferior ======
    DMA1_SRC  = (u32)srcBottom;
    DMA1_DEST = (u32)dstBottom;
    DMA1_CR   = (sizeBottom >> 1) | DMA_ENABLE;  // Halfwords

    // ====== Esperar a que ambos terminen ======
    while (DMA0_CR & DMA_ENABLE || DMA1_CR & DMA_ENABLE);

    // ====== Flush final si VRAM se usará en GPU inmediatamente ======
    if(_acurrate == true){
        DC_FlushRange(dstTop, sizeTop);
        DC_FlushRange(dstBottom, sizeBottom);
    }
}

inline void drawLineSurface(int x0, int y0, int x1, int y1, u16 color, int surfaceW) {
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        // pinta un pixel en surface (índice lineal)
        surface[(y0<<surfaceW) + x0] = color;

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
    int xres = 1<<surfaceXres;   // ancho
    int yres = 1<<surfaceYres;   // alto
    //if(full){
    //    int from = subSurfaceXoffset+(subSurfaceYoffset<<surfaceXres);
    //}
    if(paletteOffset == 0)
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
    if(showGrid){drawGrid(AVinvertColor(palette[paletteOffset]));}
}

void drawColorPalette()
{
    for(int i = 0; i < 16; i++)//vertical
    {
        for(int j = 0; j < 16; j++)//horizontal
        {
            AVdrawRectangle(pixels,192+(j<<2),4,64+(i<<2),4,palette[(i<<4)+j]);
            updated = true;
            
        }
    }
}

void drawNesPalette()
{
    //dibujar paleta
    for(int i = 0; i < 4; i++)//vertical
    {
        for(int j = 0; j < 16; j++)//horizontal
        {
            AVdrawRectangle(pixels,192+(j<<2),4,48+(i<<2),4,nesPalette[(i<<4)+j]);
            updated = true;
        }
    }
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
        AVfillDMA(pixelsTopVRAM,j<<8,(((j+1)<<8)-1),_col);
    }
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

    // Cargar imagen inicial
    decompress(GFXinputBitmap, BG_GFX_SUB, LZ77Vram);

    // Transferir pixels a la RAM
    dmaCopyHalfWords(3,pixelsVRAM,pixels , 49152 * sizeof(u16));

    mainSurfaceXoffset = 128-((1<<surfaceXres)>>1);
    mainSurfaceYoffset = 96-((1<<surfaceYres)>>1);

    clearTopBitmap();

    updatePal(0, &palettePos);
    drawSurfaceMain(true);
    drawSurfaceBottom();
}

void setEditorSprites(){
            oamSet(&oamSub, 0,
            0, 16,
            0,
            15, // opaco
            SpriteSize_32x32, SpriteColorFormat_Bmp,
            gfx32,
            -1,
            false, false, false, false, false);
        //oamSet(&oamSub,1,0,0,0,15,SpriteSize_16x16, SpriteColorFormat_Bmp,gfx16,-1,false, false, false, false, false);
    }

//====================================================================Compatibilidad con modos gráficos====================================|
inline void textMode()
{
    if(currentSubMode == SUB_TEXT) return; // ya estamos en texto
    currentSubMode = SUB_TEXT;
    //matamos ambas pantallas por que... why not :)

    videoSetMode(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    consoleInit(&topConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);

    oamClear(&oamSub, 0, 128);
    
}

inline void bitmapMode()
{
    if(currentSubMode == SUB_BITMAP) return; // ya estamos en bitmap
    currentSubMode = SUB_BITMAP;

    videoSetMode(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_SPRITE); // sprites en VRAM B
    bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    videoSetModeSub(MODE_5_2D);             // pantalla inferior bitmap
    vramSetBankC(VRAM_C_SUB_BG);
    vramSetBankD(VRAM_D_SUB_SPRITE); // sprites en VRAM D
    bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    clearTopBitmap();
    setEditorSprites();
    submitVRAM(true,true);//recuperamos nuestros queridos datos
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

//============================================================= INPUT =================================================|
inline int getActionsFromKeys(int keys) {
    int actions = ACTION_NONE;

    if(keys & KEY_UP)    actions |= ACTION_UP;
    if(keys & KEY_DOWN)  actions |= ACTION_DOWN;
    if(keys & KEY_LEFT)  actions |= ACTION_LEFT;
    if(keys & KEY_RIGHT) actions |= ACTION_RIGHT;
    if(keys & KEY_Y)     actions |= ACTION_ZOOM_IN;
    if(keys & KEY_X)     actions |= ACTION_ZOOM_OUT;

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
    acurrate = true;//queremos que se renderize todo correctamente
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
    int maxY      = (1 << surfaceXres) - blockSize;         // límite máximo en Y
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
    int len = strlen(text);

    switch (key) {
        case '/': // borrar
            if (len > 0) text[len - 1] = '\0';
            break;

        case '|': // alternar mayúsculas
            mayus = !mayus;
            break;

        case '>': // enter
            // aquí podrías procesar el texto completo, por ejemplo:
            // guardarTexto(text);
            // limpiar el buffer si quieres:
            text[0] = '\0';
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

                text[len] = key;
                text[len + 1] = '\0';
            }
            break;
    }
}
void floodFill(u16 *surface, int x, int y, u16 oldColor, u16 newColor, int xres, int yres) {// NECESITA SER ARREGLADO (overflow)
    if (oldColor == newColor) return;
    if (surface[(y << xres) + x] != oldColor) return;

    int sp = 0;

    stack[sp++] = x;
    stack[sp++] = y;

    while (sp > 0) {
        int cy = stack[--sp];
        int cx = stack[--sp];

        if (cx < 0 || cy < 0 || cx >= (1 << xres) || cy >= (1 << yres)) continue;
        if (surface[(cy << xres) + cx] != oldColor) continue;

        surface[(cy << xres) + cx] = newColor;

        stack[sp++] = cx+1; stack[sp++] = cy;
        stack[sp++] = cx-1; stack[sp++] = cy;
        stack[sp++] = cx;   stack[sp++] = cy+1;
        stack[sp++] = cx;   stack[sp++] = cy-1;
    }
}

void applyTool(int x, int y, bool dragging) {
    u16 color = palettePos - paletteOffset;//lo limitamos al rango de bpp
    switch (currentTool) {
        case TOOL_BRUSH:
            if (dragging) {
                drawLineSurface(prevtpx, prevtpy, x, y, color, surfaceXres);
            } else {
                surface[(y << surfaceXres) + x] = color;
            }
            break;

        case TOOL_ERASER:
            if (dragging) {
                drawLineSurface(prevtpx, prevtpy, x, y, 0, surfaceXres);
            } else {
                surface[(y << surfaceXres) + x] = 0;
            }
            break;

        case TOOL_PICKER: 
            updatePal(surface[(y << surfaceXres) + x]-color,&palettePos);
            currentTool = TOOL_BRUSH;
            oamSetXY(&oamSub,0,0,16);
            break;

        case TOOL_BUCKET:
            floodFill(surface, x, y, surface[(y << surfaceXres) + x], color, surfaceXres, surfaceYres);
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

void pasteFromStackToSurface(int xoffset = 0, int yoffset = 0)
{
    int ysize = 1 << stackYres;
    int xsize = 1 << stackXres;

    for(int i = 0; i < ysize; i++) // eje vertical
    {
        int y = (i << stackXres); // fila en el stack
        int _y = ((i + yoffset) << surfaceXres) + xoffset; // fila en surface con offset

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


void scaleDown(){
    cutFromSurfaceToStack();

    //lee el stack saltandose un pixel
    int yres = (1<<stackYres)>>1;
    int xres = (1<<stackXres)>>1;
    int baseOffset = subSurfaceXoffset+(subSurfaceYoffset<<surfaceXres);
    int _y = 0; int offset = 0;

    for(int y = 0; y < yres; y++){
        //precalcular algunas cosas
        offset = (y<<surfaceXres)+baseOffset;
        _y = (y << 1) << stackXres;

        for(int x = 0; x < xres; x++){//dibujar
            surface[offset+x] = stack[_y+(x<<1)];
        }
    }
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
    initFPS();
    // --- Inicializar video temporalmente en modo consola (pantalla superior) ---
    videoSetMode(MODE_0_2D);                // modo texto

    // Intentar montar la SD
    bool sd_ok = fatInitDefault();
    if(sd_ok){
        chdir("sd:/");
        currentDir = opendir("/");
    }

    // poner pantalla inferior en modo texto temporal
    videoSetModeSub(MODE_0_2D);
    vramSetBankC(VRAM_C_SUB_BG);
    consoleDemoInit();

    if (!sd_ok) {
        iprintf("\x1b[31m\n");//Rojo

        iprintf("ERROR: SD CARD NOT INITIATED.\n");
        iprintf("\x1b[38m\n");//blanco
        iprintf("You cannot load or save files.\n");

        iprintf("\nStarting in 3 seconds");
        for (int i = 0; i < 3; i++)//cantidad segundos
        {
            iprintf(".");
            for(int j = 0; j < 60; j++)
            {
                swiWaitForVBlank();
            }
        }
    }

    defaultExceptionHandler();// Mostrar crasheos
    initBitmap();
    //iniciamos el sprite para dibujar : )
    oamInit(&oamMain, SpriteMapping_Bmp_1D_128, false);
    oamInit(&oamSub, SpriteMapping_Bmp_1D_128, false);

    oamClear(&oamMain, 0, 128);
    oamClear(&oamSub, 0, 128);

    u16 *gfx32 = oamAllocateGfx(&oamSub, SpriteSize_32x32, SpriteColorFormat_Bmp);
    dmaCopy(GFXselector24Bitmap, gfx32, 32*32*2);

    u16 *gfx16 = oamAllocateGfx(&oamSub, SpriteSize_16x16, SpriteColorFormat_Bmp);
    dmaCopy(GFXselector16Bitmap, gfx16, 16*16*2);

    setEditorSprites();

    setBackupVariables();

    //========================================================================WHILE LOOP!!!!!!!!!==========================================|
    while(pmMainLoop()) {
        scanKeys();
        kDown = keysDown();
        kHeld = keysHeld();
        kUp = keysUp();

        touchRead(&touch);

        //reiniciar algunos inputs
        int actions = ACTION_NONE;

        // keys
        if(currentSubMode == SUB_BITMAP)
        {
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
                if (touch.px >= SURFACE_X && touch.px < (SURFACE_W + SURFACE_X)) {//apunta a la surface
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
                        int col = touch.px > 24 ? 1 : 0;
                        int row = touch.py > 40 ? 2 : 0;
                        //convertir col+row a un valor único
                        currentTool = (ToolType)(row + col);

                        //además dibujamos un contorno en dónde seleccionamos
                        oamSetXY(&oamSub,0,col*24, (row*12)+16);//sprite 0 es el contorno 24x24
                        stylusPressed = true;
                        goto frameEnd;
                    }
                    else//apunta a otra parte de la izquierda
                    {
                        if(touch.py < 16 && stylusPressed == false){//iconos de la parte superior
                            int selected = touch.px>>4;
                            switch(selected)
                            {
                                case 0: //load file
                                    textMode();
                                    currentConsoleMode = LOAD_file;
                                    decompress(GFXconsoleInputBitmap, BG_GFX_SUB, LZ77Vram);
                                    goto textConsole;
                                break;

                                case 1: //New file
                                    textMode();
                                    currentConsoleMode = MODE_NEWIMAGE;
                                    decompress(GFXnewImageInputBitmap, BG_GFX_SUB, LZ77Vram);
                                    goto textConsole;
                                break;

                                case 2:// Save file
                                    textMode();
                                    currentConsoleMode = SAVE_file;
                                    decompress(GFXconsoleInputBitmap, BG_GFX_SUB, LZ77Vram);
                                    goto textConsole;
                                break;

                                //PLACEHOLDER el caso 3 está libre por ahora :> (pienso usarlo para configuración en un futuro)
                            }
                        }
                        else if(touch.px >= 48 && touch.py < 64 && stylusPressed == false){//botones del costado derecho en la izquierda
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
                            switch(selected)
                            {
                                case 0://rotate -90°
                                    rotateNegative();
                                    drawSurfaceMain(false);drawSurfaceBottom();
                                break;
                                case 1://rotate 90°
                                    rotatePositive();
                                    drawSurfaceMain(false);drawSurfaceBottom();
                                break;
                                case 2:
                                    flipV();
                                    drawSurfaceMain(false);drawSurfaceBottom();
                                break;
                                case 3:
                                    flipH();
                                    drawSurfaceMain(false);drawSurfaceBottom();
                                break;

                                case 6:
                                    //verificar si es posible escalar
                                    scaleUp();
                                    drawSurfaceMain(true);drawSurfaceBottom();
                                break;

                                case 7:
                                    scaleDown();
                                    drawSurfaceMain(false);drawSurfaceBottom();
                                break;

                                case 26: //undo
                                    backupIndex--;
                                    if(backupIndex < 0){
                                        backupIndex = backupMax;
                                    }
                                    backupRead();
                                    drawSurfaceMain(true);drawSurfaceBottom();
                                break;
                                
                                case 27: //redo
                                    backupIndex++;
                                    if(backupIndex > backupMax){
                                        backupIndex = 0;
                                    }
                                    backupRead();
                                    drawSurfaceMain(true);drawSurfaceBottom();
                                break;

                            }
                            stylusPressed = true;
                            goto frameEnd;
                        }
                    }
                }

                //zona de paletas y otras configuraciones
                if(touch.px >= 192)//apunta a la parte derecha
                {
                    if (touch.py < 32 && stylusPressed == false) { // botones superiores
                        int col = (touch.px - 192) >> 4;   // 0..3
                        int row = touch.py >> 4;           // 0..1
                        if ((unsigned)col < 4 && (unsigned)row < 2) {
                            int button = (row << 2) | col; // 0..7
                            actions |= getActionsFromTouch(button);
                        }
                        stylusPressed = true;
                        goto frameEnd;
                    }
                    
                    else if(touch.py >= 40 && touch.py < 64)//creador de colores
                    {
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
                        }
                        else{
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
                            //dibujar en la pantalla
                            AVdrawRectangle(pixels,192+((palettePos & 15)<<2),4, 64+((palettePos>>4)<<2) ,4,_col);
                            
                            drawSurfaceMain(true);
                            drawSurfaceBottom();//actualizar surface, sí, es necsario

                            //dibujar arriba el nuevo color generado
                            AVdrawRectangle(pixels,192,64,32,8,_col);

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

            frameEnd://finalizar el frame

            if(kUp & KEY_TOUCH && drew == true){
                drew = false;
                backupWrite();
            }
            if (actions != ACTION_NONE) {
                applyActions(actions);
                drawSurfaceBottom();
            }
            swiWaitForVBlank();
            if(updated){//llamar a submitVRAM solo si se modificó algo visual
                submitVRAM(acurrate);
            }
            updated = false;
            acurrate = false;
            //fin del loop de modo bitmap
        }
        else//=======================================CONSOLA DE TEXTO=======================================>
        {
            textConsole:
                swiWaitForVBlank();
                bool redraw = false;
                if(kDown){redraw = true;}
                if(redraw){
                    consoleClear();
                }

                if(kDown & KEY_SELECT)
                {
                    bitmapMode();//simplemente salir de este menú
                }
                if(currentConsoleMode == MODE_NEWIMAGE)
                {
                    int bpps[4]={2,4,8,2};
                    if(redraw){
                        iprintf("Create new file:\n");
                        iprintf("Resolution: %d",1<<resX);
                        iprintf("x%d",1<<resY);
                        iprintf("\nColors:%d",1<<selectorA);

                        if(selector == 3){
                            iprintf("\nNes mode");
                        }
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
                    if(kDown & KEY_START)
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
                    }
                    //touch input
                    if(kDown & KEY_TOUCH){
                        int x = touch.px;
                        int y = touch.py-48;
                        int option = y>>5;
                        switch(option){

                            case 0://Colores
                                selector = ((x-32)/48);
                                selector = selector % 4;
                                selectorA = bpps[selector];
                            break;

                            case 1://Xres
                                resX = x>>5;
                            break;

                            case 2://Yres
                                resY = x>>5;
                            break;
                        }
                        
                    }
                }
                if(currentConsoleMode == SAVE_file || currentConsoleMode == LOAD_file)
                {   
                    
                    if(currentConsoleMode == SAVE_file)
                    {
                        if(kDown & KEY_START)//se guarda el archivo
                        {
                            strcat(path, text);
                            strcat(path, format);

                            switch(selectorA)
                            {
                                default:
                                    iprintf("\nNot supported!");
                                break;
                                case 0://bmp direct
                                    saveBMP(path,palette,surface);//Gracias Zhennyak!
                                break;

                                case 1://bmp indexed
                                    saveBMP_indexed(path,palette,surface);
                                break;

                                case 2://bmp 4bpp
                                    saveBMP_4bpp(path,palette,surface);
                                break;

                                case 3://NES
                                    exportNES(path,surface,1<<surfaceYres);
                                break;
                                case 4://GameBoy
                                    exportGBC(path,surface,1<<surfaceYres);
                                break;
                                case 5://SNES                                                         
                                    exportSNES(path,surface,1<<surfaceYres);
                                break;
                                case 6://GBA
                                    exportGBA(path,surface,1<<surfaceYres);
                                break;
                                case 7://PCX                                                                             
                                    exportPCX(path,surface,1<<surfaceXres,1<<surfaceYres);
                                break;
                                case 8: //Pal
                                    exportPal(path);
                                break;
                                case 9://Gif
                                    exportGIF(path,surface,1<<surfaceXres,1<<surfaceYres);
                                break;
                                case 10://Tga
                                    exportTGA(path,surface,1<<surfaceXres,1<<surfaceYres);
                                break;
                            }
                            bitmapMode();
                        }
                    }
                    else{
                        if(kDown & KEY_START)//se carga el archivo
                        {
                            //variables predeterminadas
                            strcat(path, text);
                            strcat(path, format);
                            nesMode = false;
                            switch(selectorA)
                            {
                                default:
                                    iprintf("\nNot supported!");
                                break;
                                case 1://bmp 8bpp
                                    loadBMP_indexed(path,palette,surface);
                                    paletteBpp = 8;
                                break;
                                case 2://bmp 4bpp
                                    loadBMP_4bpp(path,palette,surface);
                                    paletteBpp = 4;
                                break;
                                case 3://NES
                                    importNES(path,surface);
                                    paletteBpp = 2;nesMode = true;
                                    drawNesPalette();
                                break;
                                case 4://GBC
                                    importGBC(path,surface);
                                    paletteBpp = 2;
                                break;
                                case 5://SNES
                                    importSNES(path,surface);
                                    paletteBpp = 4;
                                break;
                                case 6://GBA
                                    importGBA(path,surface);
                                    paletteBpp = 4;
                                break;
                                case 7://PCX
                                    importPCX(path,surface);
                                break;
                                case 8://PAL
                                    importPal(path);
                                break;
                                case 9://GIF
                                    importGIF(path,surface);
                                break;
                                case 10://TGA
                                    importTGA(path,surface);
                                break;
                            }
                            bitmapMode();
                            drawSurfaceMain(true);drawSurfaceBottom();
                            drawColorPalette();
                            setBackupVariables();
                        }
                    }
                    //general
                    if(kDown & KEY_RIGHT && selectorA < 10){selectorA++;}
                    if(kDown & KEY_LEFT && selectorA > 0){selectorA--;}
                    if(kDown & KEY_UP && selector > 0){selector--;}
                    if(kDown & KEY_DOWN){selector++;}
                    char texts[11][32] = {
                        ".bmp [direct]",
                        ".bmp [8bpp]",
                        ".bmp [4bpp]",
                        ".bin [NES]",
                        ".bin [GB]",
                        ".bin [SNES]",
                        ".bin [GBA]",
                        ".pcx",
                        ".pal",
                        ".gif",
                        ".tga"
                    };
                    char formats[11][6] = {
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
                        ".tga"
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
                            iprintf("Error");
                        } else {
                            if(entryList[selector].d_type == DT_DIR) {
                                enterFolder(selector);
                            } else {
                                // Guardar archivo completo directamente en path
                                snprintf(path, sizeof(path), "%s%s", path, entryList[selector].d_name);

                                // Limpiar text y format si ya no se usan
                                text[0] = '\0';
                                format[0] = '\0';

                                kDown = KEY_START;//forzar a cargar el archivo
                                goto textConsole;
                            }
                        }
                    }
                    if(kDown & KEY_B) {
                        goBack();
                    }
                    if(redraw){
                        iprintf(text);//nombre del archivo
                        iprintf(texts[selectorA]);//extensión + información extra
                        //separar para mostrar el explorador
                        iprintf("\n-------------------------------\n");
                        printf("\n");
                        listFiles();//dibujar información del explorador
                    }
                }
        }
        oamUpdate(&oamSub);

        //dejar solo para debug, en la DSi oficial da un error visual gigantesco :D
        updateFPS();
        AVfillDMA(pixelsTopVRAM,0,60,C_BLACK);
        AVfillDMA(pixelsTopVRAM,0,fps,C_GREEN);
    }
    return 0;
}