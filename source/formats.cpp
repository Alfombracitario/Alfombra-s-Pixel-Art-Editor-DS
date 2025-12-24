//gran parte de todo el código que está aquí (no todo) no fué hecho por mi, esto porque simplemente estoy dando soporte a formatos relativamente genéricos (todos menos .acs) y no quería gastar meses programando cada formato
//por eso mismo, si alguien me ayudó con algún código lo dirá un comentario, si no entonces fué hecho por mi o directamente ChatGPT lol
//momento vibe coding lmfao
#include <nds.h>
#include <stdio.h>
#include <math.h>

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

extern u16 backup[131072];
extern u16 palette[256];
extern u16 stack[16384];
extern int surfaceXres;
extern int surfaceYres;
extern int paletteSize;
extern int paletteBpp;

// ============================================================================
//  Funciones auxiliares (puedes moverlas arriba o hacerlas inline)
// ============================================================================

// Hash rápido para fragmentos pequeños
static inline uint32_t fastHash(const uint16_t* p, int len){
    uint32_t h = 2166136261u;
    for(int i=0;i<len;i++){
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

// Hash invertido (para mirror)
static inline uint32_t fastHashRev(const uint16_t* p, int len){
    uint32_t h = 2166136261u;
    for(int i=len-1; i>=0; i--){
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

// Confirmar mirror
static inline int isMirror(const uint16_t* a, const uint16_t* b, int len){
    for(int i=0;i<len;i++){
        if(a[i] != b[len-1-i]) return 0;
    }
    return 1;
}

// Confirmar igual
static inline int isEqual(const uint16_t* a, const uint16_t* b, int len){
    for(int i=0;i<len;i++){
        if(a[i] != b[i]) return 0;
    }
    return 1;
}

int importNES(const char* path, u16* surface) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("File not found %s\n", path);
        return -1;
    }

    // Obtener tamaño del archivo
    fseek(f, 0, SEEK_END);
    int chrSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (chrSize > (131072 * 2)) { // 131072 u16 = 262144 bytes
        fclose(f);
        printf("File too large (%d bytes)\n", chrSize);
        return -2;
    }

    u8* chrData = (u8*)backup; // usar el buffer ya reservado
    fread(chrData, 1, chrSize, f);
    fclose(f);

    // Decodificar los tiles
    int numTiles = chrSize >> 4;
    int tilesPerRow = 16; // 16 tiles por fila

    for (int t = 0; t < numTiles; t++) {
        int tileX = (t % tilesPerRow) << 3;
        int tileY = (t / tilesPerRow) << 3;

        const u8* tile = &chrData[t << 4];

        for (int row = 0; row < 8; row++) {
            u8 plane0 = tile[row];
            u8 plane1 = tile[row + 8];

            for (int col = 0; col < 8; col++) {
                int bit = 7 - col;
                u16 pixel = (((plane1 >> bit) & 1) << 1) | ((plane0 >> bit) & 1);

                int x = tileX + col;
                int y = tileY + row;
                surface[x + (y << 7)] = pixel;
            }
        }
    }

    return 0;
}

int exportNES(const char* path, u16* surface, int height) {
    FILE* f = fopen(path, "wb");
    if (!f) {
        printf("Error: no se pudo crear %s\n", path);
        return -1;
    }

    int tilesPerRow = 128 / 8; // 16 tiles
    int tilesPerCol = height / 8;
    int numTiles = tilesPerRow * tilesPerCol;

    u8 tile[16];

    for (int t = 0; t < numTiles; t++) {
        int tileX = (t % tilesPerRow) * 8;
        int tileY = (t / tilesPerRow) * 8;

        for (int row = 0; row < 8; row++) {
            u8 plane0 = 0;
            u8 plane1 = 0;

            for (int col = 0; col < 8; col++) {
                int x = tileX + col;
                int y = tileY + row;
                u16 pixel = surface[x + (y << 7)] & 3;

                plane0 |= (pixel & 1) << (7 - col);
                plane1 |= ((pixel >> 1) & 1) << (7 - col);
            }

            tile[row]     = plane0;
            tile[row + 8] = plane1;
        }

        fwrite(tile, 1, 16, f);
    }

    fclose(f);
    return numTiles * 16; // bytes escritos
}
int importGBC(const char* path, u16* surface) {
    FILE* file = fopen(path, "rb");
    if(!file) return -1;

    // Leer todo a backup (temporal)
    int size = fread(backup, 1, sizeof(backup), file);
    fclose(file);
    if(size <= 0) return -2;

    int numTiles = size / 16;
    int tilesPerRow = 128 / 8;

    for(int t = 0; t < numTiles; t++) {
        int tileX = (t % tilesPerRow) * 8;
        int tileY = (t / tilesPerRow) * 8;
        if(tileY >= 128) break; // fuera del canvas

        u8* tile = (u8*)&backup[t * 8]; // cada tile = 16 bytes = 8 líneas * 2 bytes
        for(int y = 0; y < 8; y++) {
            u8 low  = tile[y * 2];
            u8 high = tile[y * 2 + 1];
            for(int x = 0; x < 8; x++) {
                int bit = 7 - x;
                u8 color = ((high >> bit) & 1) << 1 | ((low >> bit) & 1);
                surface[(tileY + y) * 128 + (tileX + x)] = color; // solo el índice (0–3)
            }
        }
    }

    return 0;
}

int exportGBC(const char* path, u16* surface, int height) {
    FILE* file = fopen(path, "wb");
    if(!file) return -1;

    int tilesPerRow = 128 / 8;
    int numTilesY = height / 8;
    int totalTiles = tilesPerRow * numTilesY;

    u8 tile[16];

    for(int t = 0; t < totalTiles; t++) {
        int tileX = (t % tilesPerRow) * 8;
        int tileY = (t / tilesPerRow) * 8;
        for(int y = 0; y < 8; y++) {
            u8 low = 0, high = 0;
            for(int x = 0; x < 8; x++) {
                u16 color = surface[(tileY + y) * 128 + (tileX + x)] & 3; // 2 bits
                int bit = 7 - x;
                low  |= (color & 1) << bit;
                high |= ((color >> 1) & 1) << bit;
            }
            tile[y * 2] = low;
            tile[y * 2 + 1] = high;
        }
        fwrite(tile, 1, 16, file);
    }

    fclose(file);
    return 0;
}


//SNES
int importSNES(const char* path, u16* surface) {
    if (!path || !surface) return -1;
    FILE* f = fopen(path, "rb");
    if (!f) return -1;

    // obtener tamaño de archivo
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fileSize <= 0) { fclose(f); return -1; }

    // número de tiles (32 bytes por tile)
    long numTiles = fileSize / 32;
    if (numTiles <= 0) { fclose(f); return -1; }

    // límite máximo: 128x128 pix => 16x16 tiles = 256 tiles
    if (numTiles > 256) numTiles = 256;

    // leer archivo entero en backup (como bytes)
    size_t bytesToRead = (size_t)(numTiles * 32);
    size_t bytesRead = fread((u8*)backup, 1, bytesToRead, f);
    fclose(f);
    if (bytesRead != bytesToRead) return -1;

    u8* data = (u8*)backup; // trabajar por bytes

    const int tilesPerRow = 16; // 16
    // Limpiar surface por si sobra espacio
    // (opcional; puedes comentar si no quieres limpiar)
    for (int i = 0; i < 128*128; ++i) surface[i] = 0;

    // Decodificar tiles
    for (long t = 0; t < numTiles; ++t) {
        int tileX = (t % tilesPerRow) * 8;
        int tileY = (t / tilesPerRow) * 8;
        size_t base = (size_t)t * 32;

        // cada tile: bytes 0-15 -> bitplanes 0&1 (2bpp chunk)
        //             bytes 16-31 -> bitplanes 2&3 (2bpp chunk)
        for (int row = 0; row < 8; ++row) {
            u8 low  = data[base + row*2 + 0]; // plane0
            u8 high = data[base + row*2 + 1]; // plane1
            u8 low2  = data[base + 16 + row*2 + 0]; // plane2
            u8 high2 = data[base + 16 + row*2 + 1]; // plane3

            int dstRow = tileY + row;
            int dstBase = dstRow * 128 + tileX;

            // expand 8 pixels in the row
            // bit 7 = leftmost pixel, bit 0 = rightmost
            for (int x = 0; x < 8; ++x) {
                int bit = 7 - x;
                u16 v = (u16)( ((low  >> bit) & 1)
                             | (((high >> bit) & 1) << 1)
                             | (((low2 >> bit) & 1) << 2)
                             | (((high2>> bit) & 1) << 3) );
                surface[dstBase + x] = v; // guardar índice 0..15 en u16
            }
        }
    }

    return (int)numTiles;
}


int exportSNES(const char* path, u16* surface, int height) {
    if (!path || !surface) return -1;
    if (height <= 0 || height > 128) return -1;
    if (height % 8 != 0) return -1;

    FILE* f = fopen(path, "wb");
    if (!f) return -1;

    const int tilesPerRow = 16; // 16
    const int tilesHigh = height>>3;
    const int numTiles = tilesPerRow * tilesHigh;
    const size_t bytesToWrite = (size_t)numTiles * 32;

    u8* data = (u8*)backup; // escribir en backup como bytes

    // preparar buffer a cero (solo sobre los bytes que vamos a usar)
    memset(data, 0, bytesToWrite);

    for (int t = 0; t < numTiles; ++t) {
        int tileX = (t % tilesPerRow) * 8;
        int tileY = (t / tilesPerRow) * 8;
        size_t base = (size_t)t * 32;

        for (int row = 0; row < 8; ++row) {
            u8 low = 0, high = 0, low2 = 0, high2 = 0;
            int srcRow = tileY + row;
            int srcBase = srcRow * 128 + tileX;

            // construir bytes por cada pixel del row
            for (int x = 0; x < 8; ++x) {
                u16 pix = surface[srcBase + x] & 0xF; // tomar sólo 4 bits
                int bit = 7 - x;
                low  |= (u8)((pix & 1) << bit);
                high |= (u8)((((pix >> 1) & 1) << bit));
                low2 |= (u8)((((pix >> 2) & 1) << bit));
                high2|= (u8)((((pix >> 3) & 1) << bit));
            }

            data[base + row*2 + 0] = low;
            data[base + row*2 + 1] = high;
            data[base + 16 + row*2 + 0] = low2;
            data[base + 16 + row*2 + 1] = high2;
        }
    }

    size_t written = fwrite((u8*)backup, 1, bytesToWrite, f);
    fclose(f);

    if (written != bytesToWrite) return -1;
    return (int)written;
}
//GBA
int importGBA(const char* path, u16* surface) {
    FILE* file = fopen(path, "rb");
    if(!file) return -1;

    // Leer todo el archivo a backup (temporal)
    int size = fread(backup, 1, sizeof(backup), file);
    fclose(file);
    if(size <= 0) return -2;

    int numTiles = size / 32;
    int tilesPerRow = 128 / 8;

    for(int t = 0; t < numTiles; t++) {
        int tileX = (t % tilesPerRow) * 8;
        int tileY = (t / tilesPerRow) * 8;
        if(tileY >= 128) break; // fuera del canvas

        u8* tile = (u8*)&backup[t * 16]; // 32 bytes = 16 u16
        for(int y = 0; y < 8; y++) {
            for(int x = 0; x < 8; x++) {
                u8 byte = tile[y * 4 + (x >> 1)];
                u8 color;
                if(x & 1) color = byte >> 4;
                else      color = byte & 0x0F;
                surface[(tileY + y) * 128 + (tileX + x)] = color;
            }
        }
    }

    return 0;
}

int exportGBA(const char* path, u16* surface, int height) {
    FILE* file = fopen(path, "wb");
    if(!file) return -1;

    int tilesPerRow = 128 / 8;
    int numTilesY = height / 8;
    int totalTiles = tilesPerRow * numTilesY;

    u8 tile[32]; // 4 bytes por fila * 8 filas

    for(int t = 0; t < totalTiles; t++) {
        int tileX = (t % tilesPerRow) * 8;
        int tileY = (t / tilesPerRow) * 8;
        for(int y = 0; y < 8; y++) {
            for(int bx = 0; bx < 4; bx++) {
                u8 c0 = surface[(tileY + y) * 128 + (tileX + bx*2)] & 0x0F;
                u8 c1 = surface[(tileY + y) * 128 + (tileX + bx*2 + 1)] & 0x0F;
                tile[y * 4 + bx] = (c1 << 4) | c0;
            }
        }
        fwrite(tile, 1, 32, file);
    }

    fclose(file);
    return 0;
}
//por más sorprendente e increíble que parezca, yo hice la ingienería inversa y programación de las paletas! (lol)
int importPal(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("File not found %s\n", path);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    int palSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (palSize > paletteSize*3) {
        printf("Error: the file is too big\n");
        fclose(f);
        return -1;
    }

    u8* chrData = (u8*)backup; // buffer temporal
    fread(chrData, 1, palSize, f);
    fclose(f);

    int colors = palSize / 3;

    for (int i = 0; i < colors; i++) {
        int idx = i * 3;

        u16 r = chrData[idx]     >> 3;
        u16 g = chrData[idx + 1] >> 3;
        u16 b = chrData[idx + 2] >> 3;

        palette[i] = 0x8000 | (b << 10) | (g << 5) | r;
    }
    return 0;
}


int exportPal(const char* path){
    FILE* f = fopen(path, "wb");
    if (!f) return -1;

    int bytesToWrite = paletteSize*3;
    for(int i = 0; i < paletteSize; i++)
    {
        //obtener los colores
        u16 color = palette[i];
        u8 r = color & 31;
        u8 g = (color >> 5) & 31;
        u8 b = (color >> 10) & 31;
        
        backup[i] = r<<3;i++;
        backup[i] = g<<3;i++;
        backup[i] = b<<3;i++;
    }

    size_t written = fwrite((u8*)backup, 1, bytesToWrite, f);
    fclose(f);

    if (written != bytesToWrite) return -1;
    return (int)written;
}
//PCX
int importPCX(const char* path, u16* surface) {
    FILE* f = fopen(path, "rb");
    if(!f) return -1;

    // Leer cabecera (128 bytes)
    u8 header[128];
    fread(header, 1, 128, f);

    int xmin = header[4] | (header[5] << 8);
    int ymin = header[6] | (header[7] << 8);
    int xmax = header[8] | (header[9] << 8);
    int ymax = header[10] | (header[11] << 8);
    int width  = xmax - xmin + 1;
    int height = ymax - ymin + 1;

    // Saltar posible tabla de colores EGA (si existe)
    int bytesPerLine = header[66] | (header[67] << 8);

    // Leer datos comprimidos (RLE)
    int dataSize = fread(backup, 1, sizeof(backup), f);

    // Buscar paleta VGA al final: 0x0C seguido de 768 bytes
    fseek(f, -769, SEEK_END);
    if(fgetc(f) != 0x0C) { fclose(f); return -2; }

    for(int i = 0; i < 256; i++) {
        u8 r = fgetc(f);
        u8 g = fgetc(f);
        u8 b = fgetc(f);
        // Convertir a RGB1555 y activar bit alpha (0x8000)
        palette[i] = 0x8000 | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
    }

    fclose(f);

    // Decodificar RLE
    f = fopen(path, "rb");
    fseek(f, 128, SEEK_SET);
    int pos = 0;
    for(int y = 0; y < height; y++) {
        int x = 0;
        while(x < width) {
            int byte = fgetc(f);
            if(byte == EOF) break;
            if((byte & 0xC0) == 0xC0) {
                int count = byte & 0x3F;
                int value = fgetc(f);
                for(int i = 0; i < count && x < width; i++) {
                    surface[y * 128 + x++] = value;
                }
            } else {
                surface[y * 128 + x++] = byte;
            }
        }
    }
    fclose(f);
    return 0;
}

int exportPCX(const char* path, u16* surface, int width, int height) {
    FILE* f = fopen(path, "wb");
    if(!f) return -1;

    u8 header[128] = {0};
    header[0] = 10;   // Manufacturer = ZSoft
    header[1] = 5;    // Version
    header[2] = 1;    // Encoding = RLE
    header[3] = 8;    // Bits per pixel
    header[4] = 0; header[5] = 0; // xmin
    header[6] = 0; header[7] = 0; // ymin
    header[8] = (width - 1) & 0xFF;
    header[9] = (width - 1) >> 8;
    header[10] = (height - 1) & 0xFF;
    header[11] = (height - 1) >> 8;
    header[12] = width & 0xFF;
    header[13] = width >> 8;
    header[65] = 1;   // planes
    header[66] = width & 0xFF; // bytes per line
    header[67] = width >> 8;
    header[68] = 8;   // palette info

    fwrite(header, 1, 128, f);

    // RLE encoding
    for(int y = 0; y < height; y++) {
        int x = 0;
        while(x < width) {
            int value = surface[y * 128 + x];
            int count = 1;
            while(x + count < width && surface[y * 128 + x + count] == value && count < 63)
                count++;

            if(count > 1 || (value & 0xC0) == 0xC0) {
                fputc(0xC0 | count, f);
                fputc(value, f);
            } else {
                fputc(value, f);
            }
            x += count;
        }
    }

    // Paleta VGA: marcador 0x0C + 256 colores RGB
    fputc(0x0C, f);
    for(int i = 0; i < 256; i++) {
        u16 c = palette[i];
        u8 r = ((c >> 10) & 31) << 3;
        u8 g = ((c >> 5) & 31) << 3;
        u8 b = (c & 31) << 3;
        fputc(r, f);
        fputc(g, f);
        fputc(b, f);
    }

    fclose(f);
    return 0;
}
//gracias Zhennyak! (el hizo el código base para convertir a bmp, lo transformé para que sea compatible con el programa)
void writeBmpHeader(FILE *f) {
    int xres = 1<<surfaceXres;
    int yres = 1<<surfaceYres;

    // Cabecera BMP simple 16bpp sin compresión
    unsigned char header[54] = {
        'B','M',            // Firma
        0,0,0,0,            // Tamaño del archivo (se rellena abajo)
        0,0,0,0,            // Reservado
        54,0,0,0,           // Offset datos (54 bytes de header)
        40,0,0,0,           // Tamaño infoheader (40 bytes)
        xres,0,0,0,          // Ancho
        yres,0,0,0,          // Alto
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
    int fileSize = 54 + xres * yres * 2 + 12; // +12 por masks
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
            uint16_t color = palette[surface[(y<<7)+ x]];
            
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

int loadBMP_direct(const char* filename, uint16_t* surface) {
    FILE* in = fopen(filename, "rb");
    if(!in) return 0;

    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;

    fread(&fileHeader, sizeof(fileHeader), 1, in);
    fread(&infoHeader, sizeof(infoHeader), 1, in);

    // Verificar firma y tipo de BMP soportado
    if(fileHeader.bfType != 0x4D42) { fclose(in); return 0; }
    if(infoHeader.biBitCount != 16 && infoHeader.biBitCount != 24) { fclose(in); return 0; }

    int width  = infoHeader.biWidth;
    int height = infoHeader.biHeight;
    int bpp    = infoHeader.biBitCount;

    // ===================== Calcular surfaceXres y surfaceYres =====================
    int newW = 1, newH = 1;
    int expW = 0, expH = 0;

    while(newW < width && expW < 7) { newW <<= 1; expW++; }
    while(newH < height && expH < 7) { newH <<= 1; expH++; }

    surfaceXres = expW;
    surfaceYres = expH;

    int paddedW = 1 << surfaceXres;
    int paddedH = 1 << surfaceYres;

    // ===================== Leer pixeles =====================
    fseek(in, fileHeader.bfOffBits, SEEK_SET);

    // Cada línea BMP está alineada a múltiplos de 4 bytes
    int bytesPerPixel = bpp / 8;
    int rowSize = ((width * bytesPerPixel + 3) & ~3);

    for(int y = 0; y < paddedH; y++) {
        for(int x = 0; x < paddedW; x++) {
            uint16_t color = 0;

            if(y < height && x < width) {
                long pos = fileHeader.bfOffBits + (height - 1 - y) * rowSize + x * bytesPerPixel;
                fseek(in, pos, SEEK_SET);

                if(bpp == 16) {
                    // BMP de 16 bits suele usar formato 565 o 555
                    uint16_t raw;
                    fread(&raw, 2, 1, in);

                    // Intentar detectar 565 y convertir a 1555
                    u8 r = (raw >> 11) & 0x1F;
                    u8 g = (raw >> 5) & 0x3F;
                    u8 b = raw & 0x1F;
                    // Convertir 565 → 555
                    g >>= 1;
                    color = (b << 10) | (g << 5) | r | 0x8000;
                } 
                else if(bpp == 24) {
                    u8 bgr[3];
                    fread(bgr, 3, 1, in);
                    u8 b = bgr[0] >> 3;
                    u8 g = bgr[1] >> 3;
                    u8 r = bgr[2] >> 3;
                    color = (b << 10) | (g << 5) | r | 0x8000;
                }
            }
            surface[y * paddedW + x] = color;
        }
    }

    fclose(in);
    return 1;
}

void saveBMP_indexed(const char* filename, uint16_t* palette, uint16_t* surface) {
    FILE* out = fopen(filename, "wb");
    if(!out) return;

    int width = 1<<surfaceXres, height = 1<<surfaceYres;
    int numColors = paletteSize; // máximo

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
        u8 a = (c & 0x8000) ? 255 : 0;
        u8 r = (c & 0x1F)<<3;
        u8 g = ((c >> 5)  & 0x1F)<<3;
        u8 b = ((c >> 10)  & 0x1F)<<3;
        u8 entry[4] = { b, g, r, a }; // BMP espera BGRA
        fwrite(entry, 4, 1, out);
    }

    // --- Píxeles (índices), bottom-up ---
    for(int y = height-1; y >= 0; y--) {
        for(int x = 0; x < width; x++) {
            u8 idx = (u8)(surface[y*width + x] & 0xFF);
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
    if(infoHeader.biBitCount != 8)  { fclose(in); return 0; } // solo 8bpp soportado

    int width  = infoHeader.biWidth;
    int height = infoHeader.biHeight;
    int numColors = infoHeader.biClrUsed ? infoHeader.biClrUsed : 256;

    // ===================== Calcular surfaceXres y surfaceYres =====================
    // Redondear a la siguiente potencia de 2 (máximo 128)
    int newW = 1, newH = 1;
    int expW = 0, expH = 0;

    while(newW < width && expW < 7) { newW <<= 1; expW++; }
    while(newH < height && expH < 7) { newH <<= 1; expH++; }

    surfaceXres = expW;  // guardar exponente (0=1px, 7=128px)
    surfaceYres = expH;

    int paddedW = 1 << surfaceXres;
    int paddedH = 1 << surfaceYres;

    // ===================== Leer paleta (BGRA → ARGB1555) =====================
    for(int i = 0; i < numColors; i++) {
        u8 entry[4];
        fread(entry, 4, 1, in);
        u8 b = entry[0];
        u8 g = entry[1];
        u8 r = entry[2];
        palette[i] = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10) | 0x8000;
    }

    // ===================== Leer pixeles =====================
    fseek(in, fileHeader.bfOffBits, SEEK_SET);

    // Se rellenan los bordes sobrantes con índice 0
    for(int y = 0; y < paddedH; y++) {
        for(int x = 0; x < paddedW; x++) {
            u8 idx = 0;
            if(y < height && x < width) {
                // Recordar que los BMP están invertidos verticalmente
                fseek(in, fileHeader.bfOffBits + ((height - 1 - y) * width) + x, SEEK_SET);
                fread(&idx, 1, 1, in);
            }
            surface[y * paddedW + x] = idx;
        }
    }

    fclose(in);
    return 1;
}


void saveBMP_4bpp(const char* filename, uint16_t* palette, uint16_t* surface) {
    FILE* out = fopen(filename, "wb");
    if (!out) return;

    int width  = 1 << surfaceXres;
    int height = 1 << surfaceYres;
    int numColors = 16;

    int bytesPerRow = ((width + 1) / 2 + 3) & ~3; // alineado a 4 bytes
    int pixelArraySize = bytesPerRow * height;
    int paletteSize = numColors * 4;
    int fileSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + paletteSize + pixelArraySize;

    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;

    fileHeader.bfType = 0x4D42; // "BM"
    fileHeader.bfSize = fileSize;
    fileHeader.bfReserved1 = 0;
    fileHeader.bfReserved2 = 0;
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + paletteSize;

    infoHeader.biSize = sizeof(BITMAPINFOHEADER);
    infoHeader.biWidth = width;
    infoHeader.biHeight = height;
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 4;
    infoHeader.biCompression = 0;
    infoHeader.biSizeImage = pixelArraySize;
    infoHeader.biXPelsPerMeter = 2835;
    infoHeader.biYPelsPerMeter = 2835;
    infoHeader.biClrUsed = numColors;
    infoHeader.biClrImportant = numColors;

    fwrite(&fileHeader, sizeof(fileHeader), 1, out);
    fwrite(&infoHeader, sizeof(infoHeader), 1, out);

    // Paleta (ARGB1555 → BGRA8888)
    for (int i = 0; i < numColors; i++) {
        uint16_t c = palette[i];
        u8 a = (c & 0x8000) ? 255 : 0;
        u8 r = (c & 0x1F) << 3;
        u8 g = ((c >> 5) & 0x1F) << 3;
        u8 b = ((c >> 10) & 0x1F) << 3;
        u8 entry[4] = { b, g, r, a };
        fwrite(entry, 4, 1, out);
    }

    // Píxeles (4bpp) → dos píxeles por byte, bottom-up
    u8 pad[3] = {0,0,0};
    for (int y = height - 1; y >= 0; y--) {
        int offset = y * width;
        int x = 0;
        for (; x < width; x += 2) {
            u8 p1 = surface[offset + x] & 0x0F;
            u8 p2 = 0;
            if (x + 1 < width) p2 = surface[offset + x + 1] & 0x0F;
            u8 byte = (p1 << 4) | p2;
            fwrite(&byte, 1, 1, out);
        }
        int rowBytes = (width + 1) / 2;
        int padLen = (4 - (rowBytes % 4)) & 3;
        fwrite(pad, 1, padLen, out);
    }

    fclose(out);
}

int loadBMP_4bpp(const char* filename, uint16_t* palette, uint16_t* surface) {
    FILE* in = fopen(filename, "rb");
    if(!in) return 0;

    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;

    fread(&fileHeader, sizeof(fileHeader), 1, in);
    fread(&infoHeader, sizeof(infoHeader), 1, in);

    if(fileHeader.bfType != 0x4D42) { fclose(in); return 0; }
    if(infoHeader.biBitCount != 4)  { fclose(in); return 0; } // solo 4bpp

    int width  = infoHeader.biWidth;
    int height = infoHeader.biHeight;
    int numColors = infoHeader.biClrUsed ? infoHeader.biClrUsed : 16;

    // ===================== Calcular surfaceXres y surfaceYres =====================
    int newW = 1, newH = 1;
    int expW = 0, expH = 0;

    while(newW < width  && expW < 7) { newW <<= 1; expW++; }
    while(newH < height && expH < 7) { newH <<= 1; expH++; }

    surfaceXres = expW;
    surfaceYres = expH;

    int paddedW = 1 << surfaceXres;
    int paddedH = 1 << surfaceYres;

    // ===================== Leer paleta (BGRA → ARGB1555) =====================
    for(int i = 0; i < numColors; i++) {
        u8 entry[4];
        fread(entry, 4, 1, in);
        u8 b = entry[0];
        u8 g = entry[1];
        u8 r = entry[2];
        palette[i] = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10) | 0x8000;
    }

    // ===================== Leer pixeles =====================
    fseek(in, fileHeader.bfOffBits, SEEK_SET);

    // Bytes por fila (alineado a 4 bytes)
    int rowBytes = ((width + 1) / 2 + 3) & ~3;

    for(int y = 0; y < paddedH; y++) {
        for(int x = 0; x < paddedW; x++) {
            u8 idx = 0;

            if(y < height && x < width) {
                int bmpY = height - 1 - y;
                int byteOffset = fileHeader.bfOffBits
                    + bmpY * rowBytes
                    + (x >> 1);

                fseek(in, byteOffset, SEEK_SET);

                u8 byte;
                fread(&byte, 1, 1, in);

                if(x & 1)
                    idx = byte & 0x0F;        // nibble bajo
                else
                    idx = (byte >> 4) & 0x0F; // nibble alto
            }

            surface[y * paddedW + x] = idx;
        }
    }

    fclose(in);
    return 1;
}
//TGA
// --- Estructura del encabezado TGA ---
typedef struct {
    u8  idLength;
    u8  colorMapType;
    u8  imageType;
    u16 colorMapStart;
    u16 colorMapLength;
    u8  colorMapDepth;
    u16 xOrigin;
    u16 yOrigin;
    u16 width;
    u16 height;
    u8  bpp;
    u8  descriptor;
} TGAHeader;

// --- Importar TGA indexado 8bpp ---
int importTGA(const char* path, u16* surface) {
    FILE* f = fopen(path, "rb");
    if(!f) return 0;

    TGAHeader header;
    fread(&header, sizeof(TGAHeader), 1, f);

    // Verificar tipo (1 = color map, sin compresión)
    if(header.imageType != 1 || header.bpp != 8 || header.colorMapType != 1) {
        fclose(f);
        return 0; // no es un TGA indexado válido
    }

    // Saltar campo ID si existe
    if(header.idLength > 0)
        fseek(f, header.idLength, SEEK_CUR);

    // Leer paleta (hasta 256 colores)
    int colors = header.colorMapLength;
    for(int i = 0; i < colors && i < 256; i++) {
        u8 rgb[3];
        fread(rgb, 1, 3, f);
        int r = rgb[2] >> 3; // nota: BGR en TGA
        int g = rgb[1] >> 3;
        int b = rgb[0] >> 3;
        palette[i] = 0x8000 | (r) | (g << 5) | (b << 10);
    }

    // Leer pixeles indexados
    int w = header.width;
    int h = header.height;
    int size = w * h;
    fread(backup, 1, size, f);

    // Copiar al surface (limitado a 128x128)
    for(int y = 0; y < h && y < 128; y++) {
        for(int x = 0; x < w && x < 128; x++) {
            u8 index = ((u8*)backup)[(h - 1 - y) * w + x]; // TGA va de abajo hacia arriba
            surface[y * 128 + x] = palette[index];
        }
    }

    fclose(f);
    return 1;
}

// --- Exportar TGA indexado 8bpp sin compresión ---
int exportTGA(const char* path, u16* surface, int width, int height) {
    FILE* f = fopen(path, "wb");
    if(!f) return 0;

    TGAHeader header;
    memset(&header, 0, sizeof(header));
    header.colorMapType = 1;
    header.imageType = 1; // uncompressed, color-mapped
    header.colorMapStart = 0;
    header.colorMapLength = 256;
    header.colorMapDepth = 24;
    header.width = width;
    header.height = height;
    header.bpp = 8;
    header.descriptor = 0x00; // origen inferior izquierdo

    fwrite(&header, sizeof(TGAHeader), 1, f);

    // Paleta (convertir a BGR888)
    for(int i = 0; i < 256; i++) {
        u16 c = palette[i];
        u8 r = ((c >> 0) & 0x1F) << 3;
        u8 g = ((c >> 5) & 0x1F) << 3;
        u8 b = ((c >> 10) & 0x1F) << 3;
        fputc(b, f); fputc(g, f); fputc(r, f);
    }

    // Imagen indexada (invertida verticalmente)
    for(int y = height - 1; y >= 0; y--) {
        for(int x = 0; x < width; x++) {
            u16 color = surface[y * 128 + x];
            int idx = 0;
            for(int p = 0; p < 256; p++) {
                if(palette[p] == color) { idx = p; break; }
            }
            fputc(idx, f);
        }
    }

    fclose(f);
    return 1;
}

//advertencia, estos importadores y exportadores de ACS están optimizados específicamente para este editor de pixel art,
//ACS es un poco más complejo,
//pero simplemente ignoré ciertos aspectos para que encaje en el hardware de la DS y la estructura de este editor de pixelart
#define ACScolModeARGB1555   0
#define ACScolModeARGB8888   1
#define ACScolModeRGB888     2
#define ACScolModeGrayScale8 3
#define ACScolModeGrayScale4 4
#define ACStotalModes 5

#define ACSmirror 01
#define ACSpattern 00
#define ACSrepeat 1
//función auxiliar
inline void readCommand7(u8 byte, int* pInd, u16* surface){
    //determinar cual es el tipo de comando contra el que estamos tratando
    switch((byte>>5) & 0b011){
        case ACSpattern:{
            //000P PPRR
            u8 repeat = (byte & 0b11)+1;
            u8 pixels = ((byte>>2) & 0b111)+2;

            int rInd = *pInd-pixels;
            u8 iterations = repeat*pixels;
            for(int i = 0; i < iterations; i++){
                surface[(*pInd)++] = surface[rInd++];
            }
        break;}
        case ACSmirror:{
            //001x xxxx
            //debemos leer para atrás y escribirlo hacia adelante
            int mInd = *pInd-1;
            u8 repeat = (byte & 0b11111)+2;
            for(int i = 0; i < repeat; i++){
                surface[(*pInd)++] = surface[mInd--];
            }
        break;}

        default:{//repetición simple (01xxxxxx)
            //obtener último color
            u16 col = surface[*pInd-1];
            u8 repeat = (byte & 0b111111)+1;
            for(int i = 0; i<repeat;i++){
                surface[(*pInd)++] = col;
            }
        break;}
    }
}

inline void readCommand8(u8 byte, int* pInd, u16* surface){
    switch(byte>>6){
        case ACSpattern:{//repeat pattern
            //CCP PPRRR
            u8 repeat = (byte & 0b111)+1;
            u8 pixels = ((byte>>3) & 0b111)+2;

            int rInd = *pInd-pixels;
            u8 iterations = repeat*pixels;
            for(int i = 0; i < iterations; i++){
                surface[(*pInd)++] = surface[rInd++];
            }
        break;}

        case ACSmirror:{//Mirror
            //debemos leer para atrás y escribirlo hacia adelante
            int mInd = *pInd-1;
            u8 repeat = (byte & 0b111111)+2;
            for(int i = 0; i < repeat; i++){
                surface[(*pInd)++] = surface[mInd--];
            }
        break;}

        default:{//repetición simple, caso 10 u 11
            //obtener último indice
            u16 pixel = surface[*pInd-1];
            u8 repeat = (byte & 0b1111111)+1;
            for(int i = 0; i<repeat;i++){
                surface[(*pInd)++] = pixel;
            }
        break;}
    }
}
void importACS(const char* path, u16* surface){
    u8* data = (u8*)backup;

    // abrir archivo
    FILE* f = fopen(path, "rb");
    if(!f){
        return;
    }

    // obtener tamaño del archivo
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // asegurarse que cabe en backup
    if(size > sizeof(backup)){
        fclose(f);
        return;
    }

    // leer archivo dentro de backup[]
    fread(data, 1, size, f);
    fclose(f);

    //comprobar si se ha abierto un archivo válido
    u8 val = data[0];
    if(val != 1){return;}//archivo ACS con compresión, modo imagen y versión 0
    int ind = 1;
    int resTable[16] = {0,4,8,16,24,32,48,64,96,128,192,256,320,512,1024,-1};
    val = data[ind++];
    
    int resX = resTable[val>>4];
    int resY = resTable[val & 0xF];

    if(resX == -1){
        resX = (data[ind++]<<8) | data[ind++];
    }
    if(resY == -1){
        resY = (data[ind++]<<8) | data[ind++];
    }

    u32 imgRes = resX*resY;

    //leer el tercer encabezado (byte 0x02)
    val = data[ind++];
    u8 bpp = val>>6;//últimos dos bits
    
    paletteBpp = (1<<(bpp & 0b11));//almacenar a palette bpp en su formato

    u8 colorMode = (val>>3) & 0b111;//colorMode
    if(colorMode > ACStotalModes){return;}//no es un formato soportado por este lector

    //leemos el colorCount
    u8 colorCount = data[ind++];

    if(colorCount > 0){//lector en modo indexeado
        switch(colorMode){
            case ACScolModeARGB1555:
                for(int i = 0; i < colorCount; i++){
                    palette[i] = (data[ind++]<<8) | data[ind++];//ACS usa el mismo formato que la DS y la SNES en color
                }
            break;

            case ACScolModeARGB8888:
                for(int i = 0; i < colorCount; i++){//proceso un poco más lento porque debe adaptarse
                    u8 a = data[ind++]>>7;
                    u8 r = data[ind++]>>3;
                    u8 g = data[ind++]>>3;
                    u8 b = data[ind++]>>3;
                    palette[i] = (a<<15)|(r<<10)|(g<<5)|(b);
                }
            break;

            case ACScolModeGrayScale4:
                for(int i = 0; i < colorCount; i++){//proceso un poco más lento porque debe adaptarse
                    u8 val = data[ind++];
                    
                    u8 hi = (val & 0xF0)>>3;
                    u8 lo = (val & 0x0F)<<1;
                    palette[i++] = 0x8000 | (hi<<10) | (hi<<5) | hi;

                    if(i >= colorCount){break;}//comprobar si aún quedan colores por escribir
                    palette[i] = 0x8000 | (lo<<10) | (lo<<5) | lo;
                }
            break;

            case ACScolModeGrayScale8:
                for(int i = 0; i < colorCount; i++){
                    u8 val = data[ind++]>>3;
                    palette[i] = 0x8000 | (val<<10) | (val<<5) | val;
                }
            break;

            case ACScolModeRGB888:
                for(int i = 0; i < colorCount; i++){
                    u8 r = data[ind++]>>3;
                    u8 g = data[ind++]>>3;
                    u8 b = data[ind++]>>3;

                    palette[i] = 0x8000 | (r<<10) | (g<<5) | b;
                }
            break;
        }

        //se terminó de leer los colores
        if(resX == 0 || resY == 0){return;}//existe un modo oculto de solo paleta, así que ya podemos salir de acá
        //ya que estamos en el modo index, debemos leer los byte controls y esas cosas
        int ByteCtrlCout = (data[ind++]<<8)|data[ind++];
        int CommandsCount = (data[ind++]<<8)|data[ind++];
        int Bind = ind;//indice del bytecontrol
        int cInd = ind+ByteCtrlCout;
        int ctrlPos = 0;
        ind += (ByteCtrlCout+CommandsCount);
        //la cantidad de pixeles se asume

        //FIN DEL HEADER
        
        //variables globales
        int pInd = 0;//indice de pixel
        u8 part = 0;
        // --------------------------------- LEER TODOS LOS DATOS DE LOS PIXELES ---------------------------------- |
        switch(bpp){
            case 3:  // --------------------------- 8 BPP --------------------------------
                while(pInd < imgRes){
                    u8 ctrlByte = data[Bind + (ctrlPos >> 3)];
                    u8 bit      = (ctrlByte >> (7 - (ctrlPos & 7))) & 1;
                    ctrlPos++;

                    if(bit == 0){
                        // leer índice directo
                        surface[pInd++] = data[ind++];//surface está en modo index
                    }
                    else{//el bit es un comando!
                        readCommand8(data[cInd++], &pInd, surface);//avanzamos uno en el lector de comandos
                    }
                }
            break;

            case 2:  // --------------------------- 4 BPP --------------------------------
                while(pInd < imgRes){
                    u8 ctrlByte = data[Bind + (ctrlPos >> 3)];
                    u8 bit      = (ctrlByte >> (7 - (ctrlPos & 7))) & 1;
                    ctrlPos++;

                    //ind es para leer pixeles desde data
                    //pInd es para escribir pixeles en surface
                    if(bit == 0){
                        // Leer nibble alto si pInd par, bajo si impar
                        u8 raw = data[ind];
                        u8 index;
                        if(part == 0){index = raw >> 4; part++;}//hi nibble
                        else{index = raw & 0x0F; ind++; part = 0;}//lo nibble
                        surface[pInd++] = index;
                    }
                    else{
                        readCommand8(data[cInd++], &pInd, surface);
                    }
                }
            break;

            case 1:  // --------------------------- 2 BPP --------------------------------
                while(pInd < imgRes){
                    u8 ctrlByte = data[Bind + (ctrlPos >> 3)];
                    u8 bit      = (ctrlByte >> (7 - (ctrlPos & 7))) & 1;
                    ctrlPos++;

                    if(bit == 0){
                        // 4 píxeles por byte
                        int shift = 6-(part<<1);part++;
                        u8 index = (data[ind] >> shift) & 0b11;
                        surface[pInd++] = index;
                        if(shift == 0) ind++;//solo avanzar un index si ya avanzamos un byte
                    }
                    else{
                        readCommand8(data[cInd++], &pInd, surface);
                    }
                }
            break;

            case 0:  // --------------------------- 1 BPP --------------------------------
                while(pInd < imgRes){//sí, también hay compresión aquí aunque por lo general usaría más espacio
                    u8 ctrlByte = data[Bind + (ctrlPos >> 3)];
                    u8 bit      = (ctrlByte >> (7 - (ctrlPos & 7))) & 1;
                    ctrlPos++;

                    if(bit == 0){
                        // 8 píxeles por byte
                        int shift = 7-(part);part++;
                        u8 index = (data[ind] >> shift) & 1;
                        surface[pInd++] = index;
                        if(shift == 0) ind++;//solo avanzar un index si ya avanzamos un byte
                    }
                    else{
                        readCommand8(data[cInd++], &pInd, surface);
                    }
                }
            break;
        }
    }else{// ------------------ LECTOR EN MODO DIRECTO!!111 -------------------
        //forzar al modo 16bits
        paletteBpp = 16;
        int pInd = 0;
        switch(colorMode)
        {
            //ind es el indice de LECTURA DEL BINARIO
            case ACScolModeARGB1555://el mejor modo de todos (en mi opinión)
                while(pInd < imgRes){//i siempre representará el indice de pixel en esta parte
                    u8 hi = data[ind++];
                    u8 lo = data[ind++];
                    if(hi < 0x80){//puede ser un comando! (no tiene alpha)
                        if((hi|lo) != 0){//es un comando!
                            readCommand7(hi, &pInd, surface);
                            //este modo usa 2 bytes, ahora tenemos un byte impar
                            //hay que repetir el proceso, pero haciendo un crimen: retrocedemos un ind
                            ind--; continue;
                        }
                        else{//es un pixel transparente!
                            surface[pInd++] = 0;
                            continue;
                        }
                    }
                    //si no era ni transparente ni comando, escribimos el color
                    surface[pInd++] = (hi<<8)|lo;
                }
            break;

            case ACScolModeARGB8888:
                for(int i = 0; i<imgRes; i++){
                    //el color debe ser convertido por limitaciones de hardware de la DS
                    u8 a = data[ind++]>>7;
                    u8 r = data[ind++]>>3;
                    u8 g = data[ind++]>>3;
                    u8 b = data[ind++]>>3;

                    if(a == 0){//puede ser un comando!
                        if((r|g|b) != 0){//es un comando!
                            readCommand7(r,&i,surface);//R guarda el comando en este modo
                            //retrocedemos el indice del lector binario (crimen!)
                            ind-=2;continue;//partimos leyendo ahora desde G
                        }
                        else{//es un pixel transparente!
                            surface[i] = 0;
                            continue;
                        }
                    }
                    //si no era ni transparente ni comando, escribimos el color
                    surface[i] = (a<<15)|(r<<10)|(g<<5)|b;
                }
            break;


            //estos casos tienen un comportamiento raro, usando control bytes pero su lectura es direct color.
            case ACScolModeGrayScale4:{
                //primero creamos la paleta de GrayScale4
                for(int i = 0; i < 16; i++){
                    palette[i] = 0x8000|(i<<11)|(1<<6)|(1<<1);
                }
                //luego asignamos los offsets para el correcto funcionamiento
                
                //luego usamos el mismo código de read indexed 4bpp
            break;}

            case ACScolModeGrayScale8:{
                //primero creamos la paleta de GrayScale4
                for(int i = 0; i < 32; i++){
                    palette[i] = 0x8000|(i<<10)|(i<<5)|i;
                }
                //offsets
                int ByteCtrlCout = (data[ind++]<<8)|data[ind++];
                int CommandsCount = (data[ind++]<<8)|data[ind++];
                int Bind = ind;//indice del bytecontrol
                int cInd = ind+ByteCtrlCout;
                int ctrlPos = 0;
                ind += (ByteCtrlCout+CommandsCount);
                //código
                while(pInd < imgRes){
                    u8 ctrlByte = data[Bind + (ctrlPos >> 3)];
                    u8 bit      = (ctrlByte >> (7 - (ctrlPos & 7))) & 1;
                    ctrlPos++;
                    if(bit == 0){
                        //la surface tecnicamente está en modo index
                        surface[pInd++] = data[ind++]>>3;
                    }else{readCommand8(data[cInd++], &pInd, surface);}
                }
            break;}

            case ACScolModeRGB888:
            break;
        }
    }
}

void exportACS(const char* path, u16* surface){
    //todo: revisar 2bpp y 1bpp
    //agregar los otros comandos de exportación


    // revisar si la imagen es válida (solo aplica a esta app)
    if(surfaceXres < 2 || surfaceXres >= 8){ return; }
    if(surfaceYres < 2 || surfaceYres >= 8){ return; }
    u8* data = (u8*)backup;

    // -------------------------------------------------------------------
    // ENCABEZADO (3 bytes + 1 de config de paleta)
    // -------------------------------------------------------------------
    u8 ver         = 0;//versión 0
    u8 compression = 1;//usa compresión
    u8 fType       = 0;//imagen

    // Byte 0
    data[0] = (ver << 4) | (fType << 2) | (compression);

    // resoluciones ACS (reducido específicamente para esta app)
    u8 resTable[8] = {15,15,1,2,3,5,7,9};
    data[1] = (resTable[surfaceXres] << 4) | (resTable[surfaceYres]);
    data[2] = 0;// byte configuraciones color/bpp
    data[3] = 0;// byte 3 = cantidad de colores de paleta

    // índice del primer byte libre después del header
    int ind = 4;

    // -------------------------------------------------------------------
    // CONTADOR DE COLORES
    // usamos data[4 ... 32771] como tabla de colores (32768 bytes)
    // -------------------------------------------------------------------
    u8* table = &data[4];

    int totalPixels = (1 << surfaceXres) * (1 << surfaceYres);
    int unique = 0;
    int gray   = 1;
    int maxCol = 0;

    // limpiar la tabla de colores sin desbordar
    memset(table, 0, 32768);
    for(int i = 0; i < totalPixels; i++){
        u16 px = surface[i] & 0x7FFF; // ignorar bit alpha ARGB1555

        if(!table[px]){
            table[px] = 1;
            unique++;

            if(px > maxCol) maxCol = px;

            if(gray){
                int r = (px >> 10) & 0x1F;
                int g = (px >>  5) & 0x1F;
                int b =  px        & 0x1F;

                if(!(r == g && g == b)){
                    gray = 0;
                }
            }
        }
    }

    // -------------------------------------------------------------------
    // CONFIG. COLOR Y BPP
    // -------------------------------------------------------------------
    u8 colorConfig = gray ? ACScolModeGrayScale8 : ACScolModeARGB1555; // 3 = grayscale, 0 = ARGB1555
    printf("\nColor Config: %d",colorConfig);
    u8 bpp = 0; // 1bpp, 2bpp, 4bpp, 8bpp
    if(paletteBpp == 16){
        maxCol = unique;
    }

    if(maxCol < 4){
        bpp = 1;   // 2bpp
    }else if(maxCol < 16){
        bpp = 2;   // 4bpp
    }else if(maxCol < 256){
        bpp = 3;   // 8bpp
    }

    data[2] = (bpp << 6) | (colorConfig << 3);
    //los otros tres bits están reservados

    // BYTE 3: cantidad de colores en paleta
    u8 colorCount;
    if(maxCol > 255){
        // modo directo → no usa paleta
        colorCount = 0;
    }else{
        colorCount = (u8)maxCol;
    }
    data[3] = colorCount;

    // PALETA (este editor de pixel art solo exporta a dos formatos de manera nativa)
    if(colorCount > 0){

        if(colorConfig == ACScolModeARGB1555){
            for(int i = 0; i < colorCount; i++){
                u16 col = palette[i];
                data[ind++] = col >> 8;
                data[ind++] = col & 0xFF;
            }
        }else{
            // Grayscale8
            for(int i = 0; i < colorCount; i++){
                u8 g = (palette[i] & 0b11111) << 3;
                data[ind++] = g;
            }
        }

    }

    // -------------------------------------------------------------------
    // ESCRITURA DE PÍXELES
    // -------------------------------------------------------------------
    if(colorCount == 0){
        printf("\nUsing direct mode");
        // ====================== MODO DIRECTO ============================
        int iPix = 0;
        u16 lastColor = 0x81;//color inválido en ARGB1555
        const int MAXPAT = 9;
        const int MINPAT = 2; 
        printf("\nLooking for patterns, \nthis will take a time");
        while(iPix < totalPixels){
            u16 curr = surface[iPix];

            // 1) patrones
            /*int bestSize     = 0;
            int isMirrorFlag = 0;
            int repeatCount  = 0;

            //detección de comandos aún no implementada en encoder

            if(bestSize > 0){// escritura de comandos
                if(isMirrorFlag == 0){
                    // REPEAT PATTERN: 0CCP PPRR
                    u8 cmd = ACSpattern<<5;
                    cmd |= ((repeatCount - 1) & 0b11);        // RR repeats
                    cmd |= ((bestSize    - 2) & 0b111) << 2;   // PPP pixels to read

                    data[ind++] = cmd;
                    lastColor = surface[iPix + bestSize - 1];
                    iPix += bestSize * (repeatCount + 1);
                    continue;
                }else{
                    // MIRROR: 0CCM MMMM
                    u8 cmd = ACSmirror<<5;
                    cmd |= ((bestSize - 2) & 0b11111);

                    data[ind++] = cmd;
                    lastColor = surface[iPix + bestSize - 1];
                    iPix += bestSize;
                    continue;
                }
            }*/

            // 2) repetición del color (implementado)
            if(curr == lastColor){//comprueba hacia atrás porque el comando copia color
                int run = 1;//un pixel repetido
                while(iPix + run < totalPixels && run < 64){
                    if(surface[iPix+run] == lastColor) run++;
                    else break;
                }
                u8 cmd = (0b01000000 | ((run-1) & 0b111111));//nunca debería ser mayor a 63
                data[ind++] = cmd;

                iPix += run;//sumarle al indice de pixeles
                continue;
            }
            // 3) pixel directo ARGB1555
            data[ind++] = curr >> 8;
            data[ind++] = curr & 0xFF;

            lastColor = curr;
            iPix++;
        }
    }else{
        // ====================== MODO INDEXADO ===========================
        u8* cmdBuf = (u8*)stack;
        u8* pixBuf = (u8*)stack+8192;
        u8* ctrlBuf= (u8*)stack+24576;

        int cmdInd  = 0;
        int ctrlInd = 0;
        int pixInd  = 0;

        int  ctrlBit     = 0;
        u8   currentCtrl = 0;

        int  iPix        = 0;
        const int MAXPAT = 9;
        const int MINPAT = 2;
        const int MINMIRROR = 2;
        const int MAXMIRROR = 31;
        u16 lastRaw   = 0;
        u8  lastIndex = 0;

        while(iPix < totalPixels){

            // cerrar byte-control si está lleno
            if(ctrlBit == 8){
                ctrlBuf[ctrlInd++] = currentCtrl;
                currentCtrl = 0;
                ctrlBit = 0;
            }

            u8 index = surface[iPix]; // surface ya indexada

            int bestSize   = 0;   // tamaño del patrón normal
            int bestRepeat = 0;   // repeticiones encontradas
            int bestMirror = 0;   // tamaño del mirror (independiente)
            int mode       = 0;   // 1 = pattern, 2 = mirror, 0 = nada

            int maxHist = iPix;   // cantidad de pixeles antes del actual

            //-----------------------------------------------------
            // 1) DETECCIÓN DE PATRONES NORMALES + REPEATS
            //-----------------------------------------------------
            /*for(int size = MAXPAT; size >= MINPAT; size--)
            {
                if(size > maxHist) continue;                 // no hay suficientes píxeles atrás
                if(iPix + size > totalPixels) continue;      // no cabe un bloque actual

                // ¿Bloque anterior == bloque actual?
                int ok = 1;
                for(int k = 0; k < size; k++){
                    u8 a = surface[iPix - size + k] & 0xFF;  // bloque previo
                    u8 b = surface[iPix + k]        & 0xFF;  // bloque actual
                    if(a != b){ ok = 0; break; }
                }

                if(!ok) continue;

                //-----------------------------------------
                // Encontrado: ahora buscar REPETICIONES
                //-----------------------------------------
                int rep = 1;  // ya hay 1 repetición
                while(1){
                    int startA = iPix;
                    int startB = iPix + rep * size;

                    if(startB + size > totalPixels) break;  // no cabe otro bloque

                    int ok2 = 1;
                    for(int k = 0; k < size; k++){
                        u8 a = surface[startA + k]       & 0xFF;
                        u8 b = surface[startB + k]       & 0xFF;
                        if(a != b){ ok2 = 0; break; }
                    }

                    if(!ok2) break;
                    rep++;
                }

                bestSize   = size;
                bestRepeat = rep;
                mode       = 1;
                break;
            }

            //-----------------------------------------------------
            // 2) DETECCIÓN DE MIRROR (SOLO SI NO HAY PATRÓN NORMAL)
            //-----------------------------------------------------
            if(mode == 0)
            {
                for(int size = MAXMIRROR; size >= MINMIRROR; size--)
                {
                    if(size > maxHist) continue;                 // no hay suficientes atrás
                    if(iPix + size > totalPixels) continue;      // no cabe el bloque actual

                    // ¿Coincide bloque actual con los últimos "size" reversados?
                    int ok = 1;
                    for(int k = 0; k < size; k++){
                        u8 a = surface[iPix - 1 - k] & 0xFF;     // leyendo hacia atrás
                        u8 b = surface[iPix + k]     & 0xFF;     // bloque actual
                        if(a != b){ ok = 0; break; }
                    }

                    if(ok){
                        bestMirror = size;
                        mode       = 2;
                        break;
                    }
                }
            }

            if(mode == 1)
            {
                // ======== PATTERN ========
                currentCtrl |= (1 << (7 - ctrlBit));

                // bestSize → PPP, bestRepeat → RRR
                u8 sizeField   = bestSize - 2;   // size mínimo = 2
                u8 repeatField = bestRepeat - 1; // decoder suma 1

                u8 cmd = (ACSpattern << 6)
                    | (sizeField   << 3)
                    | (repeatField);

                cmdBuf[cmdInd++] = cmd;

                // avanzamos bestSize * bestRepeat
                iPix += bestSize * bestRepeat;

                lastRaw   = surface[iPix-1] & 0x7FFF;
                lastIndex = lastRaw & 0xFF;

                ctrlBit++;
                continue;
            }

            else if(mode == 2)
            {
                // ======== MIRROR ========
                currentCtrl |= (1 << (7 - ctrlBit));

                // size guardado como (size-2)
                u8 sizeField = bestMirror - 2;  

                u8 cmd = (ACSmirror << 6)
                    | (sizeField & 0b111111);

                cmdBuf[cmdInd++] = cmd;

                iPix += bestMirror;

                lastRaw   = surface[iPix-1] & 0x7FFF;
                lastIndex = lastRaw & 0xFF;

                ctrlBit++;
                continue;
            }

            */
            // 2) repetición simple (command8)
            if(iPix > 0 && index == lastIndex){

                int run = 1;
                while(iPix + run < totalPixels && run < 128){
                    u8 nxt = surface[iPix+run];//es un indice, se trunca solo
                    if(nxt == lastIndex) run++;
                    else break;
                }

                // marcar comando en byte-control (bit más significativo primero)
                currentCtrl |= (1 << (7 - ctrlBit));

                u8 cmd = 0b10000000 | ((run - 1));//en teoría, nunca debe poder tener un valor mayor a 127
                cmdBuf[cmdInd++] = cmd; 

                iPix += run;
                ctrlBit++;
                continue;
            }


            // 3) pixel crudo
            // bit de control = 0
            pixBuf[pixInd++] = index;

            lastRaw   = index;
            lastIndex = index;
            iPix++;
            ctrlBit++;
        }

        // guardar último byte-control si quedó incompleto
        if(ctrlBit > 0){
            ctrlBuf[ctrlInd++] = currentCtrl;
        }

        // header de conteos
        data[ind++] = (ctrlInd >> 8)  & 0xFF;
        data[ind++] = (ctrlInd      ) & 0xFF;

        data[ind++] = (cmdInd  >> 8)  & 0xFF;
        data[ind++] = (cmdInd       ) & 0xFF;
        //el lector no pide cantidad de pixeles

        // 1) byte-controls
        for(int k = 0; k < ctrlInd; k++){
            data[ind++] = ctrlBuf[k];
        }
        printf("\n%d byte controls",ctrlInd);

        // 2) comandos
        for(int k = 0; k < cmdInd; k++){
            data[ind++] = cmdBuf[k];
        }
        printf("\n%d Commands",cmdInd);

        // 3) pixeles en index
        switch(bpp){

            case 0: {//1bpp
                int bitPos = 7;
                u8 byte = 0;

                for(int k = 0; k < pixInd; k++){
                    byte |= (pixBuf[k] & 1) << bitPos;
                    if(--bitPos < 0){
                        data[ind++] = byte;
                        bitPos = 7;
                        byte = 0;
                    }
                }
                if(bitPos != 7)
                    data[ind++] = byte;
            } break;

            case 1: {//2bpp
                int bitPos = 6; // 2 bits por pixel
                u8 byte = 0;

                for(int k = 0; k < pixInd; k++){
                    byte |= (pixBuf[k] & 3) << bitPos;
                    bitPos -= 2;

                    if(bitPos < 0){
                        data[ind++] = byte;
                        bitPos = 6;
                        byte = 0;
                    }
                }
                if(bitPos != 6)
                    data[ind++] = byte;
            } break;

            case 2: {//4bpp
                int high = 1;
                u8 byte = 0;

                for(int k = 0; k < pixInd; k++){
                    if(high){
                        byte = (pixBuf[k] & 0xF) << 4;
                        high = 0;
                    } else {
                        byte |= (pixBuf[k] & 0xF);
                        data[ind++] = byte;//escribir byte
                        high = 1;
                        byte = 0;
                    }
                }
                if(!high)
                    data[ind++] = byte;//terminar de escribir byte
            } break;

            case 3: // 8bpp
                for(int k = 0; k < pixInd; k++){
                    data[ind++] = pixBuf[k];
                }
            break;
        }
        printf("\n%d indexed bytes",pixInd);
    }
    
    int finalSize = ind;
    printf("\nProcess finished\n%d bytes.",finalSize);
    FILE* f = fopen(path, "wb");
    if(!f){
        return;
    }

    fwrite(data, 1, finalSize, f);
    fclose(f);
}
