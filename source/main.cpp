/*
    ADVERTENCIA: este será el código con más bitshifts y comentarios inecesarios que verás, suerte tratando de entender algo!
     -Alfombra de madeera, Septiembre de 2025
*/

/*
    To-Do list:
    No redibujar todo al pintar
    Permitir bpp personalizados y resoluciones distintas (TERMINAR DE ARREGLAR!)
    Poder cambiar el tamaño o tipo de pincel
    mejorar el creador de colores
    mejorar el sistema de guardado/cargado
    terminar el resto de botones (casi terminado!)

    (puede que a futuro añada más)

*/
#include <nds.h>
#include <stdio.h>
#include <time.h>
#include <fat.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "avdslib.h"
#include "formats.h"
#include "GFXinput.h"
#include "GFXselector24.h"
#include "GFXselector16.h"

//Macros

#define SCREEN_W 256
#define SCREEN_H 256
#define SURFACE_X 64
#define SURFACE_Y 0
#define SURFACE_W 128
#define SURFACE_H 256
#define BACKUP_MAX 8

#define C_WHITE 65535
#define C_RED 32799
#define C_YELLOW 33791
#define C_GREEN 33760
#define C_CYAN 65504
#define C_BLUE 64512
#define C_PURPLE 64543
#define C_BLACK 32768
#define C_GRAY 48623

//===================================================================Variables================================================================
static PrintConsole topConsole;
static PrintConsole bottomConsole;

//IWRAM, 32KB usados
u16 surface[16384]__attribute__((section(".iwram"))); // 32 KB en IWRAM

//RAM 360,960 bytes usados
u16* pixelsTopVRAM = (u16*)BG_GFX;
u16* pixelsVRAM = (u16*)BG_GFX_SUB;
u16 pixelsTop[49152];//copia en RAM (es más rápida)
u16 pixels[49152];
u16 palette[256];

u16 stack[16384];// para operaciones temporales
u16 backup[131072];//para undo/redo y cargar imagenes 256kb

int backupIndex = -1;       // índice del último frame guardado
int oldestBackup = 0;       // límite inferior (el frame más antiguo que aún es válido)
int totalBackups = 0;       // cuántos backups se han llenado realmente

int stackSize = 16384;

//palletas
int paletteSize = 256;
int palettePos = 0;
int paletteBpp = 8;
int paletteOffset = 0;
u8 palEdit[3];
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

bool showGrid = false;
int gridSkips = 0;

// Variables globales para controlar el modo actual
enum subMode { SUB_TEXT, SUB_BITMAP };
subMode currentSubMode = SUB_BITMAP;

enum consoleMode { MODE_NO, LOAD_file, SAVE_file, IMAGE_SETTINGS, MODE_NEWIMAGE};
consoleMode currentConsoleMode = MODE_NO;

int selector = 0;//selector para la consola
int selectorA = 0;//selector secundario
u32 kDown = 0;
u32 kHeld = 0;
u32 kUp = 0;
bool stylusPressed = false;

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

Keyboard *kbd = keyboardDemoInit();
char text[16];

char path[128];//ubicación del archivo

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
//función para pasar de RAM a VRAM
extern "C" void memcpy_fast_arm9(const void* src, void* dst, unsigned int bytes);

inline void submitVRAM()
{
    // Mantenemos exactamente la misma semántica/firmas
    int offset = (mainSurfaceYoffset << 8) + mainSurfaceXoffset;
    int size = 512 << surfaceYres;

    // Punteros como bytes
    u16* srcTop = pixelsTop + offset;
    u16* dstTop = pixelsTopVRAM + offset;

    // Flush caché antes de copiar (importante en ARM9)
    DC_FlushRange(srcTop, size);
    // Llamada al memcpy asm rápido (bytes)
    memcpy_fast_arm9((const void*)srcTop, (void*)dstTop, (unsigned int)size);

    // Segunda copia (pantalla inferior), igual que antes
    const int size2 = 98304; // tal como tenías
    DC_FlushRange(pixels, size2);
    memcpy_fast_arm9((const void*)pixels, (void*)pixelsVRAM, (unsigned int)size2);
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
        AVdrawHlineDMA(pixels,64,192,i*separation,color);
        AVdrawVline(pixels,0,128,64+(i*separation),color);
    }
}

//=========================================================DRAW SURFACE========================================================================

//por algún motivo inline bajaba el rendimiento
//este era el código antiguo de renderizado, el nuevo es más rápido, pero altamente distinto y específico, conservo esto solo por si acaso

void drawSurfaceMain(int xsize = 7,int ysize = 7)
{
    int xres = 1<<xsize;   // ancho
    int yres = 1<<ysize;   // alto

    if(paletteOffset == 0)
    {
        for(int i = 0; i < yres; i++) // eje Y
        {
            int _y = i <<xsize; // fila en surface
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
            int _y = i <<xsize; // fila en surface
            int y  = ((i+mainSurfaceYoffset)<<8) + mainSurfaceXoffset; // fila en pantalla

            for(int j = 0; j < xres; j++) // eje X
            {
                pixelsTop[y+j] = palette[paletteOffset + surface[_y+j]];
            }
        }
    }
}
void drawSurfaceBottom(int xsize = 7, int ysize = 7) {
    int xres = 1 << xsize;
    int yres = 1 << ysize;

    // rama sin zoom: copia directa por DMA, desactiva el sprite
    if(subSurfaceZoom == 0 && xres == 128) {
        yres = (yres - 1) & 127;
        yres++;
        for(int i = 0; i < yres; i++) {
            u16* src = pixelsTop + ((i + mainSurfaceYoffset) << 8) + mainSurfaceXoffset;
            u16* dst = pixels + ((i << 8) + 64);
            dmaCopyHalfWords(3, src, dst, 256);
        }
        return;
    }

    //calcular offsets y repeticiones
    int blockSize = 128 >> subSurfaceZoom;
    int yrepeat   = 1 << subSurfaceZoom;
    int xoffset   = subSurfaceXoffset;
    int yoffset   = subSurfaceYoffset;

    int dstY = 0;// fila destino

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
    if(showGrid == true){drawGrid(AVinvertColor(palette[paletteOffset]));}
}

void drawColorPalette()// optimizable (DMA)
{
    for(int i = 0; i < 16; i++)//vertical
    {
        for(int j = 0; j < 16; j++)//horizontal
        {
            AVdrawRectangle(pixels,192+(j<<2),4,64+(i<<2),4,palette[(i<<4)+j]);
            
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

    //rectangulos de abajo
    u16 _barCol[3] = { C_RED, C_GREEN, C_BLUE };

    for(int i = 0; i < 3; i++)
    {
        AVdrawRectangle(pixels,192,_barColAmount[i]<<1,(i<<3)+40,8,_barCol[i]);//barra de color
        AVdrawRectangle(pixels,192+(_barColAmount[i]<<1),64-(_barColAmount[i]<<1),(i<<3)+40,8,C_BLACK);//area negra de fondo
    }

    AVdrawRectangle(pixels,192,64,32,8,_col);//rectángulo de arriba (color mezclado)

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
            drawSurfaceMain(surfaceXres,surfaceYres);
            drawSurfaceBottom(surfaceXres,surfaceYres);
        }
    }
    AVdrawRectangleHollow(pixels,192+(posx<<2),4,64+(posy<<2),4,AVinvertColor(_col));//dibujamos el nuevo contorno
}
//====================================================================Compatibilidad con modos gráficos====================================|
inline void clearTopBitmap()
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

inline void textMode()
{
    if(currentSubMode == SUB_TEXT) return; // ya estamos en texto
    currentSubMode = SUB_TEXT;
    //matamos ambas pantallas por que... why not :)

    videoSetMode(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    consoleInit(&topConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);

    // Pantalla inferior
    videoSetModeSub(MODE_0_2D);
    vramSetBankC(VRAM_C_SUB_BG);
    consoleInit(&bottomConsole, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
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
    submitVRAM();//recuperamos nuestros queridos datos
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
            drawSurfaceBottom(surfaceXres,surfaceYres);
        break;
        case 7://unused

        break;
    }
    return actions;
}

void applyActions(int actions) {//acciones compartidas entre teclas y touch
    int blockSize = (1 << surfaceXres) >> subSurfaceZoom;   // tamaño de bloque en píxeles según el zoom
    if(kHeld & KEY_R) {
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
void copyFromSurfaceToStack(int xsize, int ysize, int xoffset = 0, int yoffset = 0) {
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
void cutFromSurfaceToStack(int xsize, int ysize, int xoffset = 0, int yoffset = 0){
    //copia pero limpia un fragmento de la pantalla
    //en vez de optimizar esto, lo haré de la manera más simple posible lol
    copyFromSurfaceToStack(surfaceXres - subSurfaceZoom, surfaceYres - subSurfaceZoom, subSurfaceXoffset, subSurfaceYoffset);
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
    copyFromSurfaceToStack(surfaceXres - subSurfaceZoom, surfaceYres - subSurfaceZoom, subSurfaceXoffset, subSurfaceYoffset);

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
    copyFromSurfaceToStack(surfaceXres-subSurfaceZoom, surfaceYres-subSurfaceZoom, subSurfaceXoffset, subSurfaceYoffset);
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
    copyFromSurfaceToStack(surfaceXres - subSurfaceZoom,
                           surfaceYres - subSurfaceZoom,
                           subSurfaceXoffset,
                           subSurfaceYoffset);

    int stackW = 1 << stackXres;
    int stackH = 1 << stackYres;

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
    cutFromSurfaceToStack(surfaceXres - subSurfaceZoom,
                           surfaceYres - subSurfaceZoom,
                           subSurfaceXoffset,
                           subSurfaceYoffset);

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
    copyFromSurfaceToStack(surfaceXres - subSurfaceZoom,
                           surfaceYres - subSurfaceZoom,
                           subSurfaceXoffset,
                           subSurfaceYoffset);

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
    copyFromSurfaceToStack(surfaceXres - subSurfaceZoom,
                           surfaceYres - subSurfaceZoom,
                           subSurfaceXoffset,
                           subSurfaceYoffset);

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
//=================================================================Inicialización===================================================================================|
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
    drawSurfaceBottom(surfaceXres, surfaceYres);
}
//================Backups========================|
void pushBackup(void) {
    backupIndex = (backupIndex + 1) & (BACKUP_MAX - 1);
    u16 *dst = backup + backupIndex * stackSize;// Dirección destino en backup
    // Copiar surface actual
    dmaCopyHalfWords(3, surface, dst, stackSize * sizeof(u16));

    if (totalBackups < BACKUP_MAX) totalBackups++;
    // Si llenamos todo, el más antiguo avanza
    if (totalBackups == BACKUP_MAX)
        oldestBackup = (oldestBackup + 1) & (BACKUP_MAX - 1);
}
// Obtener puntero al respaldo con un índice relativo
// offset = 0 → más reciente, 1 → anterior, etc.
void loadBackup(void) {
    // No retroceder si no hay backups
    if (totalBackups == 0) return;

    // Calcular índice anterior
    int previousIndex = (backupIndex - 1) & (BACKUP_MAX - 1);

    // Si retroceder nos llevaría al más antiguo que ya es inválido, detener
    if (previousIndex == oldestBackup) {
        // Llegaste al límite más antiguo permitido
        return;
    }

    // Cargar backup anterior en surface (DMA)
    u16 *src = backup + previousIndex * stackSize;
    dmaCopyHalfWords(3, src, surface, stackSize * sizeof(u16));

    // Retroceder índice actual
    backupIndex = previousIndex;
}

//====================================================================MAIN==================================================================================================================|
int main(void) {

    initFPS();
    // --- Inicializar video temporalmente en modo consola (pantalla superior) ---
    videoSetMode(MODE_0_2D);                // modo texto

    // Intentar montar la SD
    bool sd_ok = fatInitDefault();

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

    oamSet(&oamSub, 0,
        0, 16,
        0,
        15, // opaco
        SpriteSize_32x32, SpriteColorFormat_Bmp,
        gfx32,
        -1,
        false, false, false, false, false);

    u16 *gfx16 = oamAllocateGfx(&oamSub, SpriteSize_16x16, SpriteColorFormat_Bmp);
    dmaCopy(GFXselector16Bitmap, gfx16, 16*16*2);

    oamSet(&oamSub, 1,
        0, 0,
        0,
        15,
        SpriteSize_16x16, SpriteColorFormat_Bmp,
        gfx16,
        -1,
        false, false, false, false, false);

    //========================================================================WHILE LOOP!!!!!!!!!==========================================|
    while(pmMainLoop()) {
        swiWaitForVBlank();
        scanKeys();
        kDown = keysDown();
        kHeld = keysHeld();
        kUp = keysUp();

        touchPosition touch;
        touchRead(&touch);

        //reiniciar algunos inputs
        int actions = ACTION_NONE;

        // salir con START
        if(kDown & KEY_START) break;

        // keys
        if(currentSubMode == SUB_BITMAP)
        {
            if(kHeld & KEY_R)//zoom y offsets
            {
                actions |= getActionsFromKeys(kDown);
            }
            else//paleta de colores
            {
                if(kDown & KEY_DOWN)
                {
                    updatePal(16, &palettePos);
                }
                if(kDown & KEY_UP)
                {
                    updatePal(-16, &palettePos);
                }
                if(kDown & KEY_LEFT)
                {
                    updatePal(-1, &palettePos);
                }
                if(kDown & KEY_RIGHT)
                {
                    updatePal(1, &palettePos);
                }
            }
            if(kDown & KEY_L)
            {
                showGrid = !showGrid;
                drawSurfaceBottom(surfaceXres,surfaceYres);
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

                        drawSurfaceMain(surfaceXres, surfaceYres);
                        drawSurfaceBottom(surfaceXres, surfaceYres);
                        stylusPressed = true;   
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
                    }
                    else//apunta a otra parte de la izquierda
                    {
                        if(touch.py < 16 && stylusPressed == false){//iconos de la parte superior
                            int selected = touch.px>>4;
                            switch(selected)
                            {
                                case 0: //load file
                                    textMode();
                                    kbd->OnKeyPressed = OnKeyPressed;
                                    //keyboardShow();
                                    currentConsoleMode = LOAD_file;
                                break;

                                case 1: //New file
                                    textMode();
                                    currentConsoleMode = MODE_NEWIMAGE;
                                break;

                                case 2:// Save file
                                    textMode();
                                    kbd->OnKeyPressed = OnKeyPressed;
                                    //keyboardShow();
                                    currentConsoleMode = SAVE_file;
                                break;

                                //PLACEHOLDER el caso 3 está libre por ahora :> (pienso usarlo para configuración en un futuro)
                            }
                        }
                        else if(touch.px >= 48 && touch.py < 64 && stylusPressed == false){//botones del costado derecho en la izquierda
                            int selected = touch.py>>4;
                            switch(selected)//puro hardcode lol
                            {
                                case 1://Copy
                                    copyFromSurfaceToStack(surfaceXres-subSurfaceZoom,surfaceYres-subSurfaceZoom, subSurfaceXoffset, subSurfaceYoffset);
                                break;
                                case 2://cut
                                    cutFromSurfaceToStack(surfaceXres-subSurfaceZoom,surfaceYres-subSurfaceZoom, subSurfaceXoffset, subSurfaceYoffset);
                                    drawSurfaceMain(surfaceXres, surfaceYres);drawSurfaceBottom(surfaceXres, surfaceYres);
                                break;
                                case 3://Paste
                                    pasteFromStackToSurface(subSurfaceXoffset, subSurfaceYoffset);
                                    drawSurfaceMain(surfaceXres, surfaceYres);drawSurfaceBottom(surfaceXres, surfaceYres);
                                break;
                            }
                            stylusPressed = true;
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
                                    drawSurfaceMain(surfaceXres, surfaceYres);drawSurfaceBottom(surfaceXres, surfaceYres);
                                break;
                                case 1://rotate 90°
                                    rotatePositive();
                                    drawSurfaceMain(surfaceXres, surfaceYres);drawSurfaceBottom(surfaceXres, surfaceYres);
                                break;
                                case 2:
                                    flipV();
                                    drawSurfaceMain(surfaceXres, surfaceYres);drawSurfaceBottom(surfaceXres, surfaceYres);
                                break;
                                case 3:
                                    flipH();
                                    drawSurfaceMain(surfaceXres, surfaceYres);drawSurfaceBottom(surfaceXres, surfaceYres);
                                break;

                                case 6:
                                    scaleUp();
                                    drawSurfaceMain(surfaceXres, surfaceYres);drawSurfaceBottom(surfaceXres, surfaceYres);
                                break;

                                case 7:
                                    scaleDown();
                                    drawSurfaceMain(surfaceXres, surfaceYres);drawSurfaceBottom(surfaceXres, surfaceYres);
                                break;

                                case 38: //undo

                                break;
                                
                                case 39: //redo

                                break;

                            }
                            stylusPressed = true;
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
                    }
                    
                    else if(touch.py >= 40 && touch.py < 64)//creador de colores
                    {
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
                        
                        drawSurfaceMain(surfaceXres, surfaceYres);
                        drawSurfaceBottom(surfaceXres, surfaceYres);//actualizar surface, sí, es necsario

                        //dibujar arriba el nuevo color generado
                        AVdrawRectangleDMA(pixels,192,64,32,8,_col,8);

                        //dibujar el contorno del color seleccionado
                        _col = AVinvertColor(_col);
                        AVdrawRectangleHollow(pixels,192+((palettePos & 15)<<2),4, 64+((palettePos>>4)<<2) ,4,_col);
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
                    }


                }
            }
            else
            {
                stylusPressed = false;
            }
            //código de final de frame
            if (actions != ACTION_NONE) {
                applyActions(actions);
                drawSurfaceBottom();
            }
            submitVRAM();
        }
        else//=======================================CONSOLA DE TEXTO=======================================>
        {
                //mostrar qué se está haciendo y revisar input
                //consoleSelect(&topConsole);
                swiWaitForVBlank();
                consoleClear();

                //keyboardUpdate(); // importante para mantener el teclado funcional

                //u32 keys = kDown;

                if(kDown & KEY_B)
                {
                    bitmapMode();//simplemente salir de este menú
                }
                if(currentConsoleMode == MODE_NEWIMAGE)
                {
                    int bpps[3]={2,4,8};
                    iprintf("Create new file:\n");
                    iprintf("Resolution: 128x128\n");
                    iprintf("Colors:%d",1<<paletteBpp);

                    if(kDown & KEY_RIGHT)
                    {
                        selector++;
                        if(selector > 2){selector = 0;}
                        paletteBpp = bpps[selector];
                    }
                    else if(kDown & KEY_LEFT)
                    {
                        selector--;
                        if(selector < 0){selector = 2;}
                        paletteBpp = bpps[selector];
                    }
                    if(kDown & KEY_A)
                    {
                        //se inicia un nuevo lienzo
                        initBitmap();
                        bitmapMode();//simplemente salir de este menú
                        drawColorPalette();
                    }
                }
                if(currentConsoleMode == SAVE_file || currentConsoleMode == LOAD_file)
                {
                    if(currentConsoleMode == SAVE_file)
                    {
                        iprintf("Save file:\n");
                        if(kDown & KEY_A)//se guarda el archivo
                        {
                            sprintf(path, "sd:/AlfombraPixelArtEditor/%d.bmp", selector);
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
                                    sprintf(path, "sd:/AlfombraPixelArtEditor/%dNes.bin", selector);
                                    exportNES(path,surface,1<<surfaceYres);
                                break;
                                case 4:
                                    sprintf(path, "sd:/AlfombraPixelArtEditor/%dSnes.bin", selector);                                                                                   
                                    exportSNES(path,surface,1<<surfaceYres);
                                break;

                                case 7: //Pal
                                    sprintf(path, "sd:/AlfombraPixelArtEditor/%dSnes.pal", selector);
                                    exportPal(path);
                                break;
                            }
                            bitmapMode();
                        }
                    }
                    else{
                        iprintf("Load file:\n");
                        if(kDown & KEY_A)//se carga el archivo
                        {
                            sprintf(path, "sd:/AlfombraPixelArtEditor/%d.bmp", selector);
                            switch(selectorA)
                            {
                                default:
                                    iprintf("\nNot supported!");
                                break;
                                case 1://bmp 8bpp
                                    loadBMP_indexed(path,palette,surface);
                                break;
                                case 2://bmp 4bpp
                                    loadBMP_4bpp(path,palette,surface);
                                break;
                                case 3://NES
                                    sprintf(path, "sd:/AlfombraPixelArtEditor/%dNes.bin", selector);
                                    importNES(path,surface);
                                    paletteBpp = 2;
                                break;

                                case 4://SNES
                                    sprintf(path, "sd:/AlfombraPixelArtEditor/%dSnes.bin", selector);
                                    importSNES(path,surface);
                                    paletteBpp = 4;
                                break;
                                case 6://ACS
                                    sprintf(path, "sd:/AlfombraPixelArtEditor/%d.acs", selector);
                                    decodeAcs(path,surface);
                                break;
                                case 7://PAL
                                    sprintf(path, "sd:/AlfombraPixelArtEditor/%d.pal", selector);
                                    importPal(path);
                                break;

                            }
                            bitmapMode();
                            drawSurfaceBottom(surfaceXres,surfaceYres);
                            drawSurfaceMain(surfaceXres,surfaceYres);
                            drawColorPalette();
                        }
                    }
                    //general
                    if(kDown & KEY_RIGHT && selectorA < 7){selectorA++;}
                    if(kDown & KEY_LEFT && selectorA > 0){selectorA--;}
                    if(kDown & KEY_UP && selector > 0){selector--;}
                    if(kDown & KEY_DOWN){selector++;}
                    char texts[8][32] = {
                        ".bmp direct",
                        ".bmp 8bpp",
                        ".bmp 4bpp",
                        "NES",
                        "SNES",
                        "GBA [NW]",
                        ".acs",
                        ".pal"
                    };
                    iprintf("You have selected the option: %d",selector);
                    iprintf("\nCurrent format: %s",texts[selectorA]);
                    iprintf("\n\nPress A to do the action");
                    iprintf("\nPress B to go back");
                }
        }
        
        oamUpdate(&oamSub);
        //updateFPS();
        //AVfillDMA(pixelsTopVRAM,0,60,C_BLACK);
        //AVfillDMA(pixelsTopVRAM,0,fps,C_GREEN);
    }
    return 0;
}