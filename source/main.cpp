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

    Migrar el sistema usando sprites.
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
#include "GFXinput.h"
#include "GFXselector24.h"
#include "GFXselector16.h"

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
static PrintConsole topConsole;
static PrintConsole bottomConsole;

u16* pixelsTopVRAM = (u16*)BG_GFX;
u16* pixelsVRAM = (u16*)BG_GFX_SUB;
u16 pixelsTop[49152];
u16 pixels[49152];

u16 surface[16384]__attribute__((section(".iwram"))); // 16 KB en IWRAM
u16 pallete[256]__attribute__((section(".iwram"))); // 512 bytes en IWRAM

//por temas de RAM, el máximo que puede medir una imagen es 128x192 que son 24kb aprox así puedo dejar 8kb a cosas random

u16 stack[16384];// para operaciones temporales

u16 backup[65536];//para undo/redo

//palletas
int palleteSize = 256;
int palletePos = 0;
int palleteBpp = 8;
int palleteOffset = 0;//solo se usa en modos como 4bpp
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

enum consoleMode { MODE_NO, LOAD_PALLETE, SAVE_PALLETE, LOAD_IMAGE, SAVE_IMAGE, IMAGE_SETTINGS};
consoleMode currentConsoleMode = MODE_NO;

int selector = 0;//selector para la consola

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
inline void submitVRAM()
{
    // Transferir pixelsTop a VRAM principal
    int offset = (mainSurfaceYoffset<<8)+mainSurfaceXoffset;
    int size = (1 << surfaceXres) << surfaceYres;
    dmaCopyHalfWords(3,pixelsTop+offset, pixelsTopVRAM+offset, size<<2);//solo copia la pantalla

    // Transferir pixels a VRAM secundaria
    dmaCopyHalfWords(3,pixels, pixelsVRAM, 49152 * sizeof(u16));
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

inline void drawGrid(u16 color)
{
    int separation = 1<<subSurfaceZoom;
    int rep = 128>>subSurfaceZoom;
    for(int i = 0; i < rep; i++)
    {
        AVdrawHlineDMA(pixels,64,192,i*separation,color);
        AVdrawVline(pixels,0,128,64+(i*separation),color);
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
    AVdrawRectangleHollow(pixels,192+(posx<<2),4,64+(posy<<2),4,_col);

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
        AVdrawRectangle(pixels,192,_barColAmount[i]<<1,(i<<3)+40,8,_barCol[i]);//barra de color
        AVdrawRectangle(pixels,192+(_barColAmount[i]<<1),64-(_barColAmount[i]<<1),(i<<3)+40,8,C_BLACK);//area negra de fondo
    }

    AVdrawRectangle(pixels,192,64,32,8,_col);//rectángulo de arriba (color mezclado)

    //obtenemos la coordenada de la paleta (otra vez)
    posx = *palletePos & 15;
    posy = *palletePos >>4;
    AVdrawRectangleHollow(pixels,192+(posx<<2),4,64+(posy<<2),4,AVinvertColor(_col));//dibujamos el nuevo contorno
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
    int xoffset   = subSurfaceXoffset * blockSize;
    int yoffset   = subSurfaceYoffset * blockSize;

    int dstY = 0;// fila destino

    for (int i = 0; i < blockSize; i++) {
        int srcY = i + mainSurfaceYoffset + yoffset;// 
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

void drawColorPalette()
{
    for(int i = 0; i < 16; i++)//vertical
    {
        for(int j = 0; j < 16; j++)//horizontal
        {
            //esto es muuy lento pero bueeno que se le va a hacer
            AVdrawRectangle(pixels,192+(j<<2),4,64+(i<<2),4,pallete[(j<<4)+i]);

        }
    }
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
//gracias Zhennyak! (el hizo el código base para convertir a bmp, lo transformé para que sea compatible con el programa)

void writeBmpHeader(FILE *f) {
    // Cabecera BMP simple 16bpp sin compresión
    unsigned char header[54] = {
        'B','M',            // Firma
        0,0,0,0,            // Tamaño del archivo (se rellena abajo)
        0,0,0,0,            // Reservado
        54,0,0,0,           // Offset datos (54 bytes de header)
        40,0,0,0,           // Tamaño infoheader (40 bytes)
        128,0,0,0,          // Ancho
        128,0,0,0,          // Alto
        1,0,                // Planos
        16,0,               // Bits por pixel
        3,0,0,0,            // Compresión BI_BITFIELDS (3 = usar masks)
        0,0,0,0,            // Tamaño de imagen (se puede dejar 0)
        0,0,0,0,            // Resolución X
        0,0,0,0,            // Resolución Y
        0,0,0,0,            // Colores usados
        0,0,0,0             // Colores importantes
    };

    // Máscaras de color (para 16bpp RGB565)
    unsigned int masks[3] = {
        0xF800, // Rojo
        0x07E0, // Verde
        0x001F  // Azul
    };

    // Calcula tamaño total (header + pixeles)
    int fileSize = 54 + 128 * 128 * 2 + 12; // +12 por masks
    header[2] = (unsigned char)(fileSize);
    header[3] = (unsigned char)(fileSize >> 8);
    header[4] = (unsigned char)(fileSize >> 16);
    header[5] = (unsigned char)(fileSize >> 24);

    fwrite(header, 1, 54, f);
    fwrite(masks, 4, 3, f); // Escribir las masks
}

// Guarda BMP usando paleta + surface en 16bpp directo
void saveBMP(const char* filename, uint16_t* palette, uint16_t* surface) {
    FILE* out = fopen(filename, "wb");
    if(!out) {
        return;
    }

    // Escribir cabecera BMP
    writeBmpHeader(out);

    // Escribir píxeles (desde abajo hacia arriba porque BMP lo requiere)
    for(int y = 127; y >= 0; y--) {
        for(int x = 0; x < 128; x++) {
            uint16_t color = pallete[surface[(y<<7)+ x]];
            
            //reordenamos el color para poder escribirlo correctamente
            //1555 ABGR
            u8 b = color & 31;
            u8 g = (color >> 5) & 31;
            u8 r = (color >> 10) & 31;
            color = (r<<10) | g | (b >>10) | 0x8000;
            fwrite(&color, 2, 1, out);
        }
    }

    fclose(out);
}
#pragma pack(push, 1) // asegurar alineación exacta
typedef struct {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BITMAPFILEHEADER;

typedef struct {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BITMAPINFOHEADER;
#pragma pack(pop)
void saveBMP_indexed(const char* filename, uint16_t* palette, uint16_t* surface) {
    FILE* out = fopen(filename, "wb");
    if(!out) return;

    int width = 128, height = 128;
    int numColors = 256; // máximo

    // --- File header ---
    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;

    int paletteSize = numColors * 4; // cada entrada es BGRA (4 bytes)
    int pixelArraySize = width * height; // 1 byte por pixel
    int fileSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + paletteSize + pixelArraySize;

    fileHeader.bfType = 0x4D42; // "BM"
    fileHeader.bfSize = fileSize;
    fileHeader.bfReserved1 = 0;
    fileHeader.bfReserved2 = 0;
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + paletteSize;

    // --- Info header ---
    infoHeader.biSize = sizeof(BITMAPINFOHEADER);
    infoHeader.biWidth = width;
    infoHeader.biHeight = height; // positivo = bottom-up
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 8; // indexado
    infoHeader.biCompression = 0;
    infoHeader.biSizeImage = pixelArraySize;
    infoHeader.biXPelsPerMeter = 2835;
    infoHeader.biYPelsPerMeter = 2835;
    infoHeader.biClrUsed = numColors;
    infoHeader.biClrImportant = numColors;

    fwrite(&fileHeader, sizeof(fileHeader), 1, out);
    fwrite(&infoHeader, sizeof(infoHeader), 1, out);

    // --- Paleta: convertir de ARGB1555 a BGRA8888 ---
    for(int i=0; i<numColors; i++) {
        uint16_t c = palette[i];
        uint8_t a = (c & 0x8000) ? 255 : 0;
        uint8_t r = (c & 0x1F)<<3;
        uint8_t g = ((c >> 5)  & 0x1F)<<3;
        uint8_t b = ((c >> 10)  & 0x1F)<<3;
        uint8_t entry[4] = { b, g, r, a }; // BMP espera BGRA
        fwrite(entry, 4, 1, out);
    }

    // --- Píxeles (índices), bottom-up ---
    for(int y = height-1; y >= 0; y--) {
        for(int x = 0; x < width; x++) {
            uint8_t idx = (uint8_t)(surface[y*width + x] & 0xFF);
            fwrite(&idx, 1, 1, out);
        }
    }

    fclose(out);
}
int loadBMP_indexed(const char* filename, uint16_t* palette, uint16_t* surface) {
    FILE* in = fopen(filename, "rb");
    if(!in) return 0;

    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;

    fread(&fileHeader, sizeof(fileHeader), 1, in);
    fread(&infoHeader, sizeof(infoHeader), 1, in);

    if(fileHeader.bfType != 0x4D42) { fclose(in); return 0; }
    if(infoHeader.biBitCount != 8) { fclose(in); return 0; } // solo 8bpp soportado

    int width = infoHeader.biWidth;
    int height = infoHeader.biHeight;
    int numColors = infoHeader.biClrUsed ? infoHeader.biClrUsed : 256;

    // Leer paleta (BGRA → ARGB1555)
    for(int i=0; i<numColors; i++) {
        uint8_t entry[4];
        fread(entry, 4, 1, in);
        uint8_t b = entry[0];
        uint8_t g = entry[1];
        uint8_t r = entry[2];
        //uint8_t a = entry[3];   a nadie le importa el alpha :>
        uint16_t c =
            (r>>3) |
            ((g>>3) << 5)  |
            ((b>>3) << 10)  |
            (0x8000);
        palette[i] = c;
    }

    // Leer surface
    fseek(in, fileHeader.bfOffBits, SEEK_SET);
    for(int y = height-1; y >= 0; y--) {
        for(int x = 0; x < width; x++) {
            uint8_t idx;
            fread(&idx, 1, 1, in);
            surface[y*width + x] = idx;
        }
    }

    fclose(in);
    return 1;
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

        case 3://cargar
            textMode();
            kbd->OnKeyPressed = OnKeyPressed;
            //keyboardShow();
            currentConsoleMode = LOAD_PALLETE;
        break;
        case 7://guardar
            textMode();
            kbd->OnKeyPressed = OnKeyPressed;
            //keyboardShow();
            currentConsoleMode = SAVE_PALLETE;

        break;
    }
    return actions;
}

void applyActions(int actions) {
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

void floodFill(u16 *surface, int x, int y, u16 oldColor, u16 newColor, int xres, int yres) {//NECESITA SER ARREGLADO!
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

//=========== Herramientas extras=========|
void flipH()//de momento hace un flip de la pantalla entera para testear
{
    for(int j = 0; j < 1<<surfaceYres; j++)//copiamos inversamente
    {
        int y = j<<surfaceYres;
        for(int i = 0; i < 1<<surfaceXres; i++)
        {
            stack[y+((1<<surfaceXres) - i)] = surface[i+y];
        }
    }
    //ahora pegamos le contenido del stack 
    dmaCopyHalfWords(3,stack, surface, 128*128*2);
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

    // --- Ahora cambiamos al modo bitmap normal ---
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

    //iniciamos el sprite para dibujar : )
    oamInit(&oamMain, SpriteMapping_1D_128, false);
    oamInit(&oamSub, SpriteMapping_1D_128, false);
    oamClear(&oamMain, 0, 128);
    oamClear(&oamSub, 0, 128);

    u16 *gfxSub = oamAllocateGfx(&oamSub, SpriteSize_32x32, SpriteColorFormat_Bmp);
    dmaCopy(GFXselector24Bitmap, gfxSub, GFXselector24BitmapLen);

    oamSet(&oamSub, 0,
        0, 16, // X, Y
        0, // Priority
        15, // Palette index, but it is the alpha value of bitmap sprites
        SpriteSize_32x32, SpriteColorFormat_Bmp, // Size, format
        gfxSub,  // Graphics offset
        -1, // Affine index
        false, // Double size
        false, // Hide
        false, false, // H flip, V flip
        false); // Mosaic
    
    gfxSub = oamAllocateGfx(&oamSub, SpriteSize_16x16, SpriteColorFormat_Bmp);
    dmaCopy(GFXselector16Bitmap, gfxSub, GFXselector16BitmapLen);

    oamSet(&oamSub, 1,
        0, 0, // X, Y
        0, // Priority
        15, // Palette index, but it is the alpha value of bitmap sprites
        SpriteSize_16x16, SpriteColorFormat_Bmp, // Size, format
        gfxSub,  // Graphics offset
        -1, // Affine index
        false, // Double size
        false, // Hide
        false, false, // H flip, V flip
        false); // Mosaic
    
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
            if(keysDown() & KEY_L)
            {
                showGrid = !showGrid;
            }
            //===========================================PALETAS=========================================================
            palletePos = palletePos & (palleteSize-1);//mantiene la paleta dentro de un límite
            if(palletePos < 0){palletePos = 0;}
            //recordar que debo hacer cambios dependiendo del bpp


            // leer touch
            touchPosition touch;
            touchRead(&touch);
            //PLACEHOLDER

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
                        int row = touch.py > 40 ? 2 : 0;
                        //convertir col+row a un valor único
                        currentTool = (ToolType)(row + col);

                        //además dibujamos un contorno en dónde seleccionamos
                        oamSetXY(&oamSub,0,col*24, (row*12)+16);//sprite 0 es el contorno 24x24
                        stylusPressed = true;
                    }
                    else//apunta a otra parte de la izquierda
                    {

                        if(touch.py >= 64 && stylusPressed == false)//revisar botones inferiores
                        {
                            //hardcodeado porque lol
                            int selected = touch.px>>4;
                            switch(selected)
                            {
                                case 2:
                                    //flipV
                                break;
                                case 3:
                                    flipH();
                                    drawSurfaceMain(surfaceXres, surfaceYres);
                                    drawSurfaceBottom(surfaceXres, surfaceYres);//actualizar surface, sí, es necsario
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
                        pallete[palletePos] = _col;
                        //dibujar en la pantalla
                        AVdrawRectangle(pixels,192+((palletePos & 15)<<2),4, 64+((palletePos>>4)<<2) ,4,_col);
                        
                        drawSurfaceMain(surfaceXres, surfaceYres);
                        drawSurfaceBottom(surfaceXres, surfaceYres);//actualizar surface, sí, es necsario

                        //dibujar arriba el nuevo color generado
                        AVdrawRectangleDMA(pixels,192,64,32,8,_col);

                        //dibujar el contorno del color seleccionado
                        _col = AVinvertColor(_col);
                        AVdrawRectangleHollow(pixels,192+((palletePos & 15)<<2),4, 64+((palletePos>>4)<<2) ,4,_col);
                    }
                    else//seleccionar un color en la paleta
                    {
                        if(stylusPressed == false)
                        {
                            int row = (touch.py-64)>>2;
                            int col = (touch.px-196)>>2;

                            updatePal(((row<<4)+col)-palletePos,&palletePos);   
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
            if(showGrid)
            {
                drawGrid(AVinvertColor(pallete[palleteOffset]));
            }
            submitVRAM();
        }
        else//si estamos en modo consola de texto
        {
                //mostrar qué se está haciendo y revisar input
                //consoleSelect(&topConsole);
                swiWaitForVBlank();
                consoleClear();

                //keyboardUpdate(); // importante para mantener el teclado funcional

                //u32 keys = keysDown();

                if(keysDown() & KEY_B)
                {
                    bitmapMode();//simplemente salir de este menú
                }

                if(currentConsoleMode == SAVE_PALLETE)
                {
                    iprintf("Save file:\n");
                    if(keysDown() & KEY_A)//se guarda el archivo
                    {
                        /*sprintf(path, "sd:/AlfombraPixelArtEditor/%d.pal", selector);
                        saveArray(path,pallete,512);//512 bytes
                        sprintf(path, "sd:/AlfombraPixelArtEditor/%d.bin", selector);
                        saveArray(path,surface,32768);
                        sprintf(path, "sd:/AlfombraPixelArtEditor/%d.bmp", selector);
                        saveBMP(path, pallete, surface);//guarda un bmp extra por ahora :)
                        */
                        sprintf(path, "sd:/AlfombraPixelArtEditor/%d.bmp", selector);
                        saveBMP_indexed(path,pallete,surface);
                        bitmapMode();
                        
                    }
                }
                if(currentConsoleMode == LOAD_PALLETE)
                {
                    iprintf("Load file:\n");
                    if(keysDown() & KEY_A)//se carga el archivo
                    {
                        /*sprintf(path, "sd:/AlfombraPixelArtEditor/%d.pal", selector);
                        loadArray(path,pallete,256);
                        sprintf(path, "sd:/AlfombraPixelArtEditor/%d.bin", selector);
                        loadArray(path,surface,32768);*/
                        sprintf(path, "sd:/AlfombraPixelArtEditor/%d.bmp", selector);
                        loadBMP_indexed(path,pallete,surface);
                        drawColorPalette();
                        bitmapMode();
                    }
                }
                if(keysDown() & KEY_UP && selector > 0)
                {
                    selector--;
                }
                if(keysDown() & KEY_DOWN)
                {
                    selector++;
                }

                //dibujar en la pantalla inferior
                //iprintf("%s", text); // mostrar texto
                iprintf("You have selected the option: %d",selector);
                iprintf("\nPress A to do the action");
                iprintf("\nPress B to go back");
        }
        
        oamUpdate(&oamSub);
        //updateFPS();
        //AVfillDMA(pixelsTopVRAM,0,60,C_BLACK);
        //AVfillDMA(pixelsTopVRAM,0,fps,C_GREEN);
    }
    return 0;
}