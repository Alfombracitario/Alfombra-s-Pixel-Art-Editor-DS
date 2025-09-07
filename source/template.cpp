/*
    ADVERTENCIA: este será el código con más bitshifts y comentarios inecesarios que verás, suerte tratando de entender algo!
     -Alfombra de madeera, Septiembre de 2025
*/

/*
    To-Do list:
    No redibujar todo al pintar
    Exportar e importar archivos/paletas
    mejorar el zoom
    Permitir bpp personalizados y resoluciones distintas
    Poder cambiar el tamaño o tipo de pincel

    (puede que a futuro añada más)

*/
#include <nds.h>
#include <stdio.h>
#include <time.h>
#include <fat.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "GFXinput.h"

#define SCREEN_W 256
#define SCREEN_H 256
#define SURFACE_X 64
#define SURFACE_Y 0
#define SURFACE_W 128
#define SURFACE_H 256

//Lujos de entender RGB: escribí esto sin testear
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

u16* pixelsTopVRAM = (u16*)BG_GFX;
u16* pixelsVRAM = (u16*)BG_GFX_SUB;

//pixeles en RAM (para que sea más rápido trabajar con ellos)
u16 pixelsTop[49152];
u16 pixels[49152];

//iniciar la surface (datos default)
u16 surface[16384]__attribute__((section(".iwram"))); // 16 KB en IWRAM
//por temas de RAM, el máximo que puede medir una imagen es 128x192 que son 24kb aprox así puedo dejar 8kb a cosas random
u16 stack[16384];// para operaciones temporales

u16 backup[65536];//para undo/redo

u16 toolsImg[2304];//imagen de las herramientas

//palletas
int palleteSize = 256;
int palletePos = 0;
int palleteBpp = 4;
int palleteOffset = 0;//solo se usa en modos como 4bpp
u16 pallete[256]__attribute__((section(".iwram")));
u8 palEdit[3];
//otras variables
int prevtpx = 0;
int prevtpy = 0;

int mainSurfaceXoffset = 0;
int mainSurfaceYoffset = 0;
u8 subSurfaceXoffset = 0;
u8 subSurfaceYoffset = 0;
u8 subSurfaceZoom = 3;//8 veces más cerca

//Solo acepto potencias de 2   >:3
u8 surfaceXres = 7;
u8 surfaceYres = 7;

bool showGrid = false;

// Variables globales para controlar el modo actual
enum subMode { SUB_TEXT, SUB_BITMAP };
subMode currentSubMode = SUB_BITMAP;

enum consoleMode { MODE_NO, LOAD_PALLETE, SAVE_PALLETE, LOAD_IMAGE, SAVE_IMAGE};
consoleMode currentConsoleMode = MODE_NO;

int selector = 0;//selector para la consola

bool stylusPressed = false;
//botones
typedef struct {
    int x, y;
    int w, h;
    bool pressed;
} Button;

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


//función para pasar de RAM a VRAM
inline void submitVRAM()
{
    // Transferir pixelsTop a VRAM principal
    int offset = (mainSurfaceYoffset<<8)+mainSurfaceXoffset;
    int size = (1 << surfaceXres) << surfaceYres;
    dmaCopyHalfWords(3,pixelsTop+offset, pixelsTopVRAM+offset, size<<2);//solo copia la pantalla

    // Transferir pixels a VRAM secundaria
    dmaCopyHalfWords(3,pixels, pixelsVRAM, 49152 * sizeof(u16));
}



// Helper color
inline u16 ARGB(int r, int g, int b, int a = 1) {
    return ((a & 1) << 15)       // Alpha en bit 15
         | ((b & 31) << 10)      // Azul
         | ((g & 31) << 5)       // Verde
         |  (r & 31);            // Rojo
}

inline u16 InvertColor(u16 color)
{
    return (~color) | 0x8000; //recordemos que alpha debe ser 1
}

// Dibuja un pixel (sub)
inline void setPixel(int x, int y, u16 color) {
    pixels[(y<<8) + x] = color;
}
//lo lee (sub)
inline u16 readPixel(int x, int y)
{
    return pixels[(y<<8) + x];
}
inline void drawRectangle(int x,int width, int y,int height, u16 color){//No uso DMA porque por lo general los rectangulos no son muy grandes
    int xlimit = x+width;
    int ylimit = y+height;
    for(int i = y; i < ylimit; i++)//eje vertical
    {
        int _y = i<<8;
        for(int j = x; j < xlimit; j++)//eje horizontal
        {
            pixels[_y + j] = color;
        }
    }
}

inline void fillDMA(u16 *arr, int start, int end, u16 value) {//array, x0, x1, color
    int count = end - start;
    dmaFillHalfWords(value, &arr[start], count<<1);
}


inline void drawRectangleDMA(int x, int width, int y, int height, u16 color) {
    int xto = x+width;
    height += y;
    for (int i = y; i < height; i++) {
        int _i = i<<8;
        fillDMA(pixels,_i+x,_i+xto,color);//pixel es el acceso directo a la VRAM
    }
}

inline void drawRectangleMainDMA(int x, int width, int y, int height, u16 color) {
    int xto = x+width;
    height += y;
    for (int i = y; i < height; i++) {
        int _i = i<<8;
        fillDMA(pixelsTop,_i+x,_i+xto,color);//pixel es el acceso directo a la VRAM
    }
}

inline void drawRectangleHollow(int x, int width, int y, int height, u16 color) {
    int xlimit = x + width;
    int ylimit = y + height;

    // línea superior e inferior
    for (int i = x; i < xlimit; i++) {
        pixels[(y << 8) + i]         = color;       // top
        pixels[((ylimit - 1) << 8) + i] = color;    // bottom
    }

    // líneas laterales
    for (int j = y + 1; j < (ylimit - 1); j++) {
        pixels[(j << 8) + x]         = color;       // left
        pixels[(j << 8) + (xlimit - 1)] = color;    // right
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

inline void drawVlineSub(int y0, int y1, int x, u16 color)
{
    y0 = (y0<<8)+x;
    y1 = (y1<<8)+x;
    for(int i = y0; i < y1; i+=256)
    {
        pixels[i] = color;
    }
}
inline void drawHlineSub(int x0, int x1, int y, u16 color)
{
    y = y<<8;
    fillDMA(pixels,y+x0,y+x1,color);
}
inline void drawGrid()
{
    int separation = 1<<subSurfaceZoom;
    int rep = 128>>subSurfaceZoom;
    for(int i = 0; i < rep; i++)
    {
        drawHlineSub(64,192,i*separation,C_WHITE);
        drawVlineSub(0,128,64+(i*separation),C_WHITE);
    }
}

inline void updatePal(int increment, int *palletePos)
{
    //primero debemos saber si estamos en un rango válido
    if(*palletePos + increment < 0 || *palletePos + increment > palleteSize)
    {
        return;
    }

    //obtenemos la coordenada de la paleta
    int posx = *palletePos & 15;
    int posy = *palletePos >>4;

    u16 _col = pallete[*palletePos];

    //reparamos el slot anterior
    drawRectangleHollow(192+(posx<<2),4,64+(posy<<2),4,_col);

    //nueva información
    *palletePos+=increment;
    _col = pallete[*palletePos];
    //obtener cada color RGB
    u8 r = (_col & 31);
    u8 g = (_col & 992)>>5;
    u8 b = (_col & 31744)>>10;
    u8 _barColAmount[3] = {r, g, b};

    //rectangulos de abajo
    u16 _barCol[3] = { C_RED, C_GREEN, C_BLUE };

    for(int i = 0; i < 3; i++)
    {
        drawRectangle(192,_barColAmount[i]<<1,(i<<3)+40,8,_barCol[i]);//barra de color
        drawRectangle(192+(_barColAmount[i]<<1),64-(_barColAmount[i]<<1),(i<<3)+40,8,C_BLACK);//area negra de fondo
    }

    drawRectangle(192,64,32,8,_col);//rectángulo de arriba (color mezclado)

    //obtenemos la coordenada de la paleta (otra vez)
    posx = *palletePos & 15;
    posy = *palletePos >>4;
    drawRectangleHollow(192+(posx<<2),4,64+(posy<<2),4,InvertColor(_col));//dibujamos el nuevo contorno
}

//=========================================================DRAW SURFACE========================================================================

//por algún motivo inline bajaba el rendimiento
void drawSurfaceMain(int xsize = 7,int ysize = 7,int palleteOffset = 0)
{
    int xres = 1<<xsize;   // ancho
    int yres = 1<<ysize;   // alto

    if(palleteOffset == 0)
    {
        for(int i = 0; i < yres; i++) // eje Y
        {
            int _y = i <<xsize; // fila en surface
            int y  = ((i+mainSurfaceYoffset)<<8) + mainSurfaceXoffset; // fila en pantalla

            for(int j = 0; j < xres; j++) // eje X
            {
                pixelsTop[y+j] = pallete[surface[_y+j]];
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
                pixelsTop[y+j] = pallete[palleteOffset + surface[_y+j]];
            }
        }
    }
}
void drawSurfaceBottom(int xsize = 7, int ysize = 7) {
    int xres = 1 << xsize;
    int yres = 1 << ysize;

    // rama sin zoom: copia directa por DMA
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

    // rama con zoom: escalado por filas + DMA
    int blockSize = 128 >> subSurfaceZoom;
    int yrepeat   = 1 << subSurfaceZoom;
    int xoffset   = subSurfaceXoffset * blockSize;
    int yoffset   = subSurfaceYoffset * blockSize;

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
    //placeholder
    drawGrid();
}

//====================================================================Compatibilidad con modos gráficos====================================|

inline void SubTextMode()
{
    if(currentSubMode == SUB_TEXT) return; // ya estamos en texto
    currentSubMode = SUB_TEXT;

    videoSetModeSub(MODE_0_2D);           // modo tile para texto
    vramSetBankC(VRAM_C_SUB_BG);          // VRAM C asignada a fondos
    consoleDemoInit();                     // inicializa consola de debug
}

inline void SubBitmapMode()
{
    if(currentSubMode == SUB_BITMAP) return; // ya estamos en bitmap
    currentSubMode = SUB_BITMAP;

    videoSetModeSub(MODE_5_2D);           // modo bitmap 16-bit
    vramSetBankC(VRAM_C_SUB_BG);          // VRAM C asignada al bitmap
    bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0); // fondo bitmap
}

inline void clearTopBitmap()
{
    u8 r = 16;
    u8 b = 16;
    u16 _col = 0;
    for(int j = 0; j < 192; j++)//all the top screen
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
        _col = ARGB(r,0,b);
        for(int i = 0; i < 256; i++)//rellenar toda la línea horizontal
        {
                pixelsTopVRAM[(j<<8)+i] = _col;
        }
    }
}
inline void drawTools()
{
    //pegar la imagen de los botones grandes
    for(int i = 16; i < 64; i++)//eje vertical
    {
        int y = (i-16)*48;
        int _y = i<<8;
        for(int j = 0; j < 48; j++)//eje horizontal
        {
            pixels[_y+j] = toolsImg[y+j];
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
    if(keys & KEY_Y)     actions |= ACTION_ZOOM_IN;
    if(keys & KEY_X)     actions |= ACTION_ZOOM_OUT;

    return actions;
}

inline int getActionsFromTouch(int button) {
    int actions = ACTION_NONE;

    switch(button) {
        case 1: actions |= ACTION_LEFT; break;
        case 2: actions |= ACTION_RIGHT; break;
        case 5: actions |= ACTION_UP; break;
        case 6: actions |= ACTION_DOWN; break;
        case 0: actions |= ACTION_ZOOM_IN; break;
        case 4: actions |= ACTION_ZOOM_OUT; break;

        //no pongo código aquí porque va en otra parte
        case 3:
            SubTextMode();
        break;
        case 7:
            SubTextMode();
        break;
    }
    return actions;
}

inline void applyActions(int actions) {
    if(actions & ACTION_UP && subSurfaceYoffset > 0) {
        subSurfaceYoffset--;
    }
    if(actions & ACTION_DOWN && subSurfaceYoffset < (1 << subSurfaceZoom) - 1) {
        subSurfaceYoffset++;
    }
    if(actions & ACTION_LEFT && subSurfaceXoffset > 0) {
        subSurfaceXoffset--;
    }
    if(actions & ACTION_RIGHT && subSurfaceXoffset < (1 << subSurfaceZoom) - 1) {
        subSurfaceXoffset++;
    }
    if(actions & ACTION_ZOOM_IN) {
        subSurfaceZoom++;
    }
    if(actions & ACTION_ZOOM_OUT && subSurfaceZoom > 0) {
        subSurfaceZoom--;
        subSurfaceXoffset >>= 1;
        subSurfaceYoffset >>= 1;
    }
}
inline void floodFill(u16 *surface, int x, int y, u16 oldColor, u16 newColor, int xres, int yres) {//NECESITA SER ARREGLADO!
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

inline void applyTool(int x, int y, bool dragging) {
    switch (currentTool) {
        case TOOL_BRUSH:
            if (dragging) {
                drawLineSurface(prevtpx, prevtpy, x, y, palletePos, surfaceXres);
            } else {
                surface[(y << surfaceXres) + x] = palletePos;
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
            updatePal(surface[(y << surfaceXres) + x]-palletePos,&palletePos);
            break;

        case TOOL_BUCKET:
            floodFill(surface, x, y, surface[(y << surfaceXres) + x], palletePos, surfaceXres, surfaceYres);
            break;
    }
}
//=======================================================CONSOLA DE TEXTO==================================================================|
#define MAX_FILES 256

char* files[MAX_FILES];
int fileCount = 0;
FILE* currentFile = NULL;


void saveFile(const char* path, const char* data, size_t length) {
    FILE* f = fopen(path, "wb"); // write binary
    if(!f) return; // error
    fwrite(data, 1, length, f);
    fclose(f);
}


void openFile(const char* path) {
    if(currentFile) fclose(currentFile);  // cerrar si ya había otro abierto
    currentFile = fopen(path, "rb");      // "rb" = read binary
    if(!currentFile) {
        // error al abrir
    }
}

void openFolder(const char* path) {
    DIR* dir = opendir(path);
    if (!dir) return;

    struct dirent* entry;
    fileCount = 0;
    while ((entry = readdir(dir)) != NULL && fileCount < MAX_FILES) {
        files[fileCount] = strdup(entry->d_name);  // guarda el nombre
        fileCount++;
    }
    closedir(dir);
}

void exitConsole()
{
    //yo me encargo de esto
}

void consoleInput()
{
    if(keysDown() & KEY_DOWN)
    {
        if(selector > 0)selector--;
    }
    if(keysDown() & KEY_UP)
    {
        if(selector > 0)selector++;
    }
}

void drawConsoleInfo() {
    consoleClear();
    for(int i=0; i<fileCount; i++) {
        if(i == selector) iprintf("> %s\n", files[i]);
        else iprintf("  %s\n", files[i]);
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
//====================================================================MAIN=================================================================|
int main(void) {

    //lo primero, limpiar basura
    for(int i = 0; i < 16383; i++)
    {
        surface[i] = 0;
        stack[i] = 0;
    }
    for(int i = 1; i < palleteSize; i++) {
        pallete[i] = C_BLACK;
    }
    for(int i = 0; i < 192*256; i++)
    {
        pixels[i] = 0;
        pixelsTop[i] = 0;
        pixelsTopVRAM[i] = 0;
        pixelsVRAM[i] = 0;
    }
    initFPS();
    // --- Inicializar video temporalmente en modo consola (pantalla superior) ---
    videoSetMode(MODE_0_2D);                // modo texto
    vramSetBankA(VRAM_A_MAIN_BG);           // VRAM A a BG principal
    consoleDemoInit();                      // inicializar consola en la pantalla principal

    // Intentar montar la SD
    bool sd_ok = fatInitDefault();

    if (!sd_ok) {
        // poner pantalla inferior en modo texto temporal
        videoSetModeSub(MODE_0_2D);
        vramSetBankC(VRAM_C_SUB_BG);
        consoleDemoInit();

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
        // restaurar pantalla inferior a bitmap
        videoSetModeSub(MODE_5_2D);
        vramSetBankC(VRAM_C_SUB_BG);
        bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    }


    // --- Ahora cambiamos al modo bitmap normal ---
    videoSetMode(MODE_5_2D);                // pantalla superior bitmap
    vramSetBankA(VRAM_A_MAIN_BG);
    bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    videoSetModeSub(MODE_5_2D);             // pantalla inferior bitmap
    vramSetBankC(VRAM_C_SUB_BG);
    bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    // Cargar imagen inicial
    decompress(GFXinputBitmap, BG_GFX_SUB, LZ77Vram);

    // Transferir pixels a la RAM
    dmaCopyHalfWords(3,pixelsVRAM,pixels , 49152 * sizeof(u16));

    //copiar la imagen de los botones grandes
    for(int i = 16; i < 64; i++)//eje vertical
    {
        int y = (i-16)*48;
        for(int j = 0; j < 48; j++)//eje horizontal
        {
            toolsImg[y+j] = pixels[(i<<8)+j];
        }
    }

    //aquí está cómo obtener los offsets de main surface :)
    mainSurfaceXoffset = 128-((1<<surfaceXres)>>1);
    mainSurfaceYoffset = 96-((1<<surfaceYres)>>1);

    clearTopBitmap();
    //antes de entrar al loop iniciaré unos datos más por si acaso
    updatePal(0, &palletePos);
    drawSurfaceBottom(surfaceXres, surfaceYres);


    //========================================================================WHILE LOOP!!!!!!!!!==========================================|
    while(pmMainLoop()) {
        swiWaitForVBlank();
        scanKeys();
        //reiniciar algunos inputs
        int actions = ACTION_NONE;

        // salir con START
        if(keysDown() & KEY_START) break;

        // keys
        if(currentSubMode == SUB_BITMAP)
        {
            if(keysHeld() & KEY_R)//zoom y offsets
            {
                actions |= getActionsFromKeys(keysDown());
            }
            else//paleta de colores
            {
                if(keysDown() & KEY_DOWN)
                {
                    updatePal(16, &palletePos);
                }
                if(keysDown() & KEY_UP)
                {
                    updatePal(-16, &palletePos);
                }
                if(keysDown() & KEY_LEFT)
                {
                    updatePal(-1, &palletePos);
                }
                if(keysDown() & KEY_RIGHT)
                {
                    updatePal(1, &palletePos);
                }
            }
            //===========================================PALETAS=========================================================
            palletePos = palletePos & (palleteSize-1);//mantiene la paleta dentro de un límite
            if(palletePos < 0){palletePos = 0;}
            //recordar que debo hacer cambios dependiendo del bpp


            // leer touch
            touchPosition touch;
            touchRead(&touch);

            if (keysHeld() & KEY_TOUCH) {
                if (touch.px >= SURFACE_X && touch.px < (SURFACE_W + SURFACE_X) && touch.py < (1 << surfaceYres)) {//apunta a la surface

                    int shift     = subSurfaceZoom;
                    int blockSize = (1 << surfaceXres) >> shift;
                    int xfrom     = subSurfaceXoffset * blockSize;
                    int yfrom     = subSurfaceYoffset * blockSize;

                    int localX = touch.px - SURFACE_X;
                    int localY = touch.py;

                    int srcX = xfrom + (localX >> shift);
                    int srcY = yfrom + (localY >> shift);

                    // --- ejecutar la herramienta seleccionada ---
                    applyTool(srcX, srcY, stylusPressed);

                    prevtpx = srcX;
                    prevtpy = srcY;

                    drawSurfaceMain(surfaceXres, surfaceYres);
                    drawSurfaceBottom(surfaceXres, surfaceYres);
                    stylusPressed = true;
                }
                if(touch.px < 64)//apunta a la parte izquierda
                {
                    if(touch.px < 48 && touch.py > 16 && touch.py < 64 && stylusPressed == false)//herramientas
                    {
                        int col = touch.px > 24 ? 1 : 0;
                        int row = touch.py > 32 ? 2 : 0;
                        //convertir col+row a un valor único
                        currentTool = (ToolType)(row + col);

                        //además dibujamos un contorno en dónde seleccionamos
                        drawTools();//Borramos el marco anterior
                        drawRectangleHollow(col*24,24,16+(row*12),24,C_WHITE);
                        stylusPressed = true;
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
                        drawRectangle(192,amount<<1,(index<<3)+40,8,_barCol[index]);
                        //Limpiar el area
                        drawRectangle(192+(amount<<1),64-(amount<<1),(index<<3)+40,8,C_BLACK);

                        //actualizar el color
                        u16 _col = palEdit[0];
                        _col += palEdit[1]<<5;
                        _col += palEdit[2]<<10;
                        _col += 32768;//encender pixel
                        pallete[palletePos] = _col;
                        //dibujar en la pantalla
                        drawRectangle(192+((palletePos & 15)<<2),4, 64+((palletePos>>4)<<2) ,4,_col);
                        
                        drawSurfaceMain(surfaceXres, surfaceYres);
                        drawSurfaceBottom(surfaceXres, surfaceYres);//actualizar surface, sí, es necsario

                        //dibujar arriba el nuevo color generado
                        drawRectangleDMA(192,64,32,8,_col);

                        //dibujar el contorno del color seleccionado
                        _col = InvertColor(_col);
                        drawRectangleHollow(192+((palletePos & 15)<<2),4, 64+((palletePos>>4)<<2) ,4,_col);
                    }
                    else//seleccionar un color en la paleta
                    {
                        int row = (touch.py-64)>>2;
                        int col = (touch.px-196)>>2;

                        updatePal(((row<<4)+col)-palletePos,&palletePos);
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
        }
        else//si estamos en modo consola de texto
        {
            
        }

    //final del loop
    submitVRAM();
    updateFPS();
    //dibujar los FPS (penca lol)
    fillDMA(pixelsTopVRAM,0,60,C_BLACK);
    fillDMA(pixelsTopVRAM,0,fps,C_GREEN);
    }
    return 0;
}