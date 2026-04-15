//gran parte de todo el código que está aquí (no todo) no fué hecho por mi, esto porque simplemente estoy dando soporte a formatos relativamente genéricos (todos menos .acs) y no quería gastar meses programando cada formato
//por eso mismo, si alguien me ayudó con algún código lo dirá un comentario, si no entonces fué hecho por mi o directamente ChatGPT lol
//momento vibe coding lmfao

/*
    To-do list
    GBA tiled 8bpp
    finish .acs
*/
#include <nds.h>
#include <stdio.h>
#include <math.h>
#include "formatsglobals.h"
#include "formats.h"

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

extern int fileOffset;
extern u32 kDown;

//funciones auxiliares para formatos retro
FILE* openFileOffset(const char* path, int* dataSize){
    const int pageSize = paletteBpp<<11;
    const int bytesPerRow = (paletteBpp<<7)>>3;

    FILE* f = fopen(path, "r+b");
    if (!f) {
        printf("File not found %s\n", path);
        return NULL;
    }

    // Tamaño total del archivo
    fseek(f, 0, SEEK_END);
    int fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);


    // offset circular
    int maxOffset = fileSize - pageSize;
    if (maxOffset < 0) maxOffset = 0;

    if (fileOffset < 0) {
        fileOffset = maxOffset;
    }
    else if (fileOffset > maxOffset) {
        fileOffset = 0;
    }


    int remainingSize = fileSize - fileOffset;

    // Tamaño real a leer
    *dataSize = remainingSize;
    if (*dataSize > pageSize) {
        *dataSize = pageSize;
    }

    // Filas necesarias (no potencia aún)
    int rowsNeeded = remainingSize / bytesPerRow;
    if (remainingSize % bytesPerRow != 0) rowsNeeded++;

    // Calcular exponente mínimo tal que (1<<surfaceYres) >= rowsNeeded
    surfaceYres = 0;
    int height = 1;
    while (height < rowsNeeded && surfaceYres < 7) {
        height <<= 1;
        surfaceYres++;
    }

    surfaceYres = min(surfaceYres, 7);
    surfaceXres = 7;

    fseek(f, fileOffset, SEEK_SET);
    return f;
}


int importNES(const char* path, u16* surface) {
    int dataSize = 0;
    FILE* f = openFileOffset(path,&dataSize);
    if(f == NULL){return -1;}

    // Leer datos
    u8* chrData = (u8*)backup;
    fread(chrData, 1, dataSize, f);
    fclose(f);
    //limpiar surface
    memset(surface, 0, 32768);

    // Decodificar tiles
    int numTiles = dataSize >> 4; // 16 bytes por tile

    for (int t = 0; t < numTiles; t++) {
        int tileX = (t & 15)<< 3;
        int tileY = (t>>4)  << 3;

        // Evitar escribir fuera de la surface
        if (tileY >= 128) break;

        const u8* tile = &chrData[t << 4];

        for (int row = 0; row < 8; row++) {
            u8 plane0 = tile[row];
            u8 plane1 = tile[row + 8];

            for (int col = 0; col < 8; col++) {
                int bit = 7 - col;
                u16 pixel =
                    (((plane1 >> bit) & 1) << 1) |
                     ((plane0 >> bit) & 1);

                int x = tileX + col;
                int y = tileY + row;

                if (x < 128 && y < 128) {
                    surface[x + (y << 7)] = pixel;
                }
            }
        }
    }

    return 0;
}

int exportNES(const char* path, u16* surface, int height) {
    int dataSize = 0;
    FILE* f = openFileOffset(path,&dataSize);
    if(f == NULL){return -1;}


    int tilesPerRow = 16;
    int tilesPerCol = height>>3;
    int numTiles = tilesPerRow * tilesPerCol;

    u8 tile[16];

    for (int t = 0; t < numTiles; t++) {
        int tileX = (t & 15) <<3;
        int tileY = (t >> 4) <<3;

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
    return 0;
}

int importGBC(const char* path, u16* surface) {
    int dataSize = 0;
    FILE* f = openFileOffset(path,&dataSize);
    if(f == NULL){return -1;}

    // Leer todo a backup (temporal)
    int size = fread(backup, 1, sizeof(backup), f);
    fclose(f);
    if(size <= 0) return -2;

    memset(surface, 0, 32768);

    const int numTiles = size>>4;
    const int tilesPerRow = 16;

    for(int t = 0; t < numTiles; t++) {
        int tileX = (t % tilesPerRow)<<3;
        int tileY = (t>>4)<<3;
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
    int dataSize = 0;
    FILE* f = openFileOffset(path,&dataSize);
    if(f == NULL){return -1;}


    int tilesPerRow = 16;
    int numTilesY = height>>3;
    int totalTiles = tilesPerRow * numTilesY;

    u8 tile[16];

    for(int t = 0; t < totalTiles; t++) {
        int tileX = (t % tilesPerRow)<<3;
        int tileY = (t / tilesPerRow)<<3;
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
        fwrite(tile, 1, 16, f);
    }

    fclose(f);
    return 0;
}


//SNES
int importSNES(const char* path, u16* surface) {
    int dataSize = 0;
    FILE* f = openFileOffset(path,&dataSize);
    if(f == NULL){return -1;}

    // número de tiles (32 bytes por tile)
    long numTiles = dataSize>>5;
    if (numTiles <= 0) { fclose(f); return -1; }

    // límite máximo: 128x128 pix => 16x16 tiles = 256 tiles
    if (numTiles > 256) numTiles = 256;

    // leer archivo entero en backup (como bytes)
    size_t bytesToRead = (size_t)(numTiles<<5);
    size_t bytesRead = fread((u8*)backup, 1, bytesToRead, f);
    fclose(f);
    if (bytesRead != bytesToRead) return -1;
    
    u8* data = (u8*)backup; // trabajar por bytes

    // Limpiar surface por si sobra espacio
    memset(surface, 0, 32768);

    // Decodificar tiles
    for (int t = 0; t < numTiles; ++t) {
        int tileX = (t & 15) <<3;
        int tileY = (t >>4 ) <<3;
        size_t base = (size_t)t<<5;

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
    return 0;
}

int exportSNES(const char* path, u16* surface, int height) {
    if (!path || !surface) return -1;
    if (height <= 0 || height > 128) return -1;
    if (height % 8 != 0) return -1;

    int dataSize = 0;
    FILE* f = openFileOffset(path,&dataSize);
    if(f == NULL){return -1;}


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
    int dataSize = 0;
    FILE* f = openFileOffset(path,&dataSize);
    if(f == NULL){return -1;}

    // Leer todo el archivo a backup (temporal)
    int size = fread(backup, 1, sizeof(backup), f);
    fclose(f);
    if(size <= 0) return -2;

    int numTiles = size / 32;
    int tilesPerRow = 128 / 8;

    memset(surface, 0, 32768);

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
    int dataSize = 0;
    FILE* f = openFileOffset(path,&dataSize);
    if(f == NULL){return -1;}

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
        fwrite(tile, 1, 32, f);
    }

    fclose(f);
    return 0;
}
//por más sorprendente e increíble que parezca, yo hice la ingienería inversa y programación de las paletas! (lol)
int importPal(const char* path, u16* pal) {
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

        pal[i] = 0x8000 | (b << 10) | (g << 5) | r;
    }
    return 0;
}


int exportPal(const char* path, u16* pal){
    FILE* f = fopen(path, "wb");
    if (!f) return -1;

    int bytesToWrite = paletteSize * 3;

    u8* out = (u8*)backup; // buffer temporal

    for (int i = 0; i < paletteSize; i++)
    {
        u16 color = pal[i];

        u8 r = color & 31;
        u8 g = (color >> 5) & 31;
        u8 b = (color >> 10) & 31;

        int idx = i * 3;

        out[idx]     = r << 3;
        out[idx + 1] = g << 3;
        out[idx + 2] = b << 3;
    }

    size_t written = fwrite(out, 1, bytesToWrite, f);
    fclose(f);

    if (written != bytesToWrite) return -1;
    return (int)written;
}
int importPal1555(const char* path, u16* pal) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("File not found %s\n", path);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    int palSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (palSize > paletteSize<<1) {
        printf("Error: the file is too big\n");
        fclose(f);
        return -1;
    }

    u8* chrData = (u8*)backup; // buffer temporal
    fread(chrData, 1, palSize, f);
    fclose(f);

    int colors = palSize>>1;

    for (int i = 0; i < colors; i++) {
        int idx = i<<1;

        int hi = chrData[idx];
        int lo = chrData[idx + 1];

        pal[i] = (hi<<8)|lo;
    }
    return 0;
}


int exportPal1555(const char* path, u16* pal){
    FILE* f = fopen(path, "wb");
    if (!f) return -1;

    int bytesToWrite = paletteSize>>1;

    u8* out = (u8*)backup; // buffer temporal

    for (int i = 0; i < paletteSize; i++)
    {
        u16 color = pal[i];

        u8 hi = color>>8;
        u8 lo = color & 0xFF;

        int idx = i<<1;

        out[idx]     = hi;
        out[idx + 1] = lo;
    }

    size_t written = fwrite(out, 1, bytesToWrite, f);
    fclose(f);

    if (written != bytesToWrite) return -1;
    return (int)written;
}


//PCX
int importPCX(const char* path, u16* surface, u16* pal) {
    FILE* f = fopen(path, "rb");
    if(!f) return -1;

    // Leer cabecera (128 bytes)
    u8 header[128];
    fread(header, 1, 128, f);

    memset(surface, 0, 32768);

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
        pal[i] = 0x8000 | ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3);
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
int exportPCX(const char* path, u16* surface, u16* pal, int width, int height) {
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
        u16 c = pal[i];
        u8 r = ((c >> 10) & 31) << 3;
        u8 g = ((c >> 5) & 31) << 3;
        u8 b = (c & 31) << 3;
        fputc(b, f);
        fputc(g, f);
        fputc(r, f);
    }

    fclose(f);
    return 0;
}
//gracias Zhennyak! (el hizo el código base para convertir a bmp, lo transformé para que sea compatible con el programa)
void writeBmpHeader(FILE *f) {
    int xres = 1 << surfaceXres;
    int yres = 1 << surfaceYres;

    int rowSize   = (xres * 3 + 3) & ~3; // padding a múltiplo de 4
    int imageSize = rowSize * yres;
    int fileSize  = 54 + imageSize;

    unsigned char header[54] = {
        'B','M',                 // Firma
        0,0,0,0,                 // Tamaño archivo
        0,0,0,0,                 // Reservado
        54,0,0,0,                // Offset píxeles
        40,0,0,0,                // Tamaño DIB
        0,0,0,0,                 // Ancho
        0,0,0,0,                 // Alto
        1,0,                     // Planos
        24,0,                    // Bits por pixel (24bpp)
        0,0,0,0,                 // BI_RGB
        0,0,0,0,                 // Tamaño imagen
        0,0,0,0,                 // Resolución X
        0,0,0,0,                 // Resolución Y
        0,0,0,0,                 // Colores usados
        0,0,0,0                  // Colores importantes
    };

    // File size
    header[2] = fileSize;
    header[3] = fileSize >> 8;
    header[4] = fileSize >> 16;
    header[5] = fileSize >> 24;

    // Width
    header[18] = xres;
    header[19] = xres >> 8;
    header[20] = xres >> 16;
    header[21] = xres >> 24;

    // Height
    header[22] = yres;
    header[23] = yres >> 8;
    header[24] = yres >> 16;
    header[25] = yres >> 24;

    // Image size
    header[34] = imageSize;
    header[35] = imageSize >> 8;
    header[36] = imageSize >> 16;
    header[37] = imageSize >> 24;

    fwrite(header, 1, 54, f);
}
// Guarda BMP usando paleta + surface en 16bpp directo
void saveBMP(const char* filename, uint16_t* pal, uint16_t* surface) {
    FILE* out = fopen(filename, "wb");
    if(!out) {
        return;
    }

    // Escribir cabecera BMP
    writeBmpHeader(out);

    // Escribir píxeles (desde abajo hacia arriba porque BMP lo requiere)
    if(paletteBpp == 16){
        for(int y = 127; y >= 0; y--) {
            for(int x = 0; x < 128; x++) {
                uint16_t color = surface[(y<<7)+ x];
                u8 b = color & 31;
                u8 g = (color >> 5) & 31;
                u8 r = (color >> 10) & 31;
                int col = (b<<19)|(g<<11)|(r<<3);//escribo en RGB888 porque mi lector soporta eso
                fwrite(&col, 3, 1, out);
            }
        }  
    }
    else{
        for(int y = 127; y >= 0; y--) {
            for(int x = 0; x < 128; x++) {
                uint16_t color = pal[surface[(y<<7)+ x]];
                u8 b = color & 31;
                u8 g = (color >> 5) & 31;
                u8 r = (color >> 10) & 31;
                int col = (b<<19)|(g<<11)|(r<<3);//escribo en RGB888 porque mi lector soporta eso
                fwrite(&col, 3, 1, out);
            }
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

    memset(surface, 0, 32768);

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

void saveBMP_indexed(const char* filename, uint16_t* pal, uint16_t* surface) {
    FILE* out = fopen(filename, "wb");
    if(!out) return;

    int width = 1<<surfaceXres, height = 1<<surfaceYres;
    int numColors = paletteSize; // máximo

    // --- File header ---
    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;

    int palSizeBytes = numColors * 4; // cada entrada es BGRA (4 bytes)
    int pixelArraySize = width * height; // 1 byte por pixel
    int fileSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + palSizeBytes + pixelArraySize;

    fileHeader.bfType = 0x4D42; // "BM"
    fileHeader.bfSize = fileSize;
    fileHeader.bfReserved1 = 0;
    fileHeader.bfReserved2 = 0;
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + palSizeBytes;

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
        uint16_t c = pal[i];
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

int loadBMP_indexed(const char* filename, uint16_t* pal, uint16_t* surface) {
    FILE* in = fopen(filename, "rb");
    if(!in) return 0;

    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;

    memset(surface, 0, 32768);

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
        pal[i] = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10) | 0x8000;
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


void saveBMP_4bpp(const char* filename, uint16_t* pal, uint16_t* surface) {
    FILE* out = fopen(filename, "wb");
    if (!out) return;

    int width  = 1 << surfaceXres;
    int height = 1 << surfaceYres;
    int numColors = 16;

    int bytesPerRow = ((width + 1) / 2 + 3) & ~3; // alineado a 4 bytes
    int pixelArraySize = bytesPerRow * height;
    int palSizeBytes = numColors * 4;
    int fileSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + palSizeBytes + pixelArraySize;

    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;

    fileHeader.bfType = 0x4D42; // "BM"
    fileHeader.bfSize = fileSize;
    fileHeader.bfReserved1 = 0;
    fileHeader.bfReserved2 = 0;
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + palSizeBytes;

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
        uint16_t c = pal[i];
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


int loadBMP_4bpp(const char* filename, uint16_t* pal, uint16_t* surface) {
    FILE* in = fopen(filename, "rb");
    if(!in) return 0;

    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;

    memset(surface, 0, 32768);
    
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
        pal[i] = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10) | 0x8000;
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

int importSNES8bpp(const char* path, u16* surface) {
    int dataSize = 0;
    FILE* f = openFileOffset(path, &dataSize);
    if (!f) return -1;

    // 64 bytes por tile
    long numTiles = dataSize >> 6;
    if (numTiles <= 0) { fclose(f); return -1; }

    // límite: 16x16 tiles = 256 tiles (128x128 px)
    if (numTiles > 256) numTiles = 256;

    size_t bytesToRead = (size_t)numTiles << 6;
    size_t bytesRead = fread((u8*)backup, 1, bytesToRead, f);
    fclose(f);
    if (bytesRead != bytesToRead) return -1;

    u8* data = (u8*)backup;

    // limpiar surface
    memset(surface, 0, 32768);

    for (int t = 0; t < numTiles; ++t) {
        int tileX = (t & 15) << 3;
        int tileY = (t >> 4) << 3;
        size_t base = (size_t)t << 6;

        for (int row = 0; row < 8; ++row) {
            // leer los 8 bitplanes
            u8 p0 = data[base + row*2 + 0];
            u8 p1 = data[base + row*2 + 1];
            u8 p2 = data[base + 16 + row*2 + 0];
            u8 p3 = data[base + 16 + row*2 + 1];
            u8 p4 = data[base + 32 + row*2 + 0];
            u8 p5 = data[base + 32 + row*2 + 1];
            u8 p6 = data[base + 48 + row*2 + 0];
            u8 p7 = data[base + 48 + row*2 + 1];

            int dstBase = (tileY + row) * 128 + tileX;

            for (int x = 0; x < 8; ++x) {
                int bit = 7 - x;
                u16 v =
                    ((p0 >> bit) & 1) |
                    (((p1 >> bit) & 1) << 1) |
                    (((p2 >> bit) & 1) << 2) |
                    (((p3 >> bit) & 1) << 3) |
                    (((p4 >> bit) & 1) << 4) |
                    (((p5 >> bit) & 1) << 5) |
                    (((p6 >> bit) & 1) << 6) |
                    (((p7 >> bit) & 1) << 7);

                surface[dstBase + x] = v; // 0..255
            }
        }
    }
    return 0;
}

int exportSNES8bpp(const char* path, u16* surface, int height) {
    if (!path || !surface) return -1;
    if (height <= 0 || height > 128) return -1;
    if (height % 8 != 0) return -1;

    int dataSize = 0;
    FILE* f = openFileOffset(path,&dataSize);
    if(f == NULL){return -1;}


    const int tilesPerRow = 16;
    const int tilesHigh = height >> 3;
    const int numTiles = tilesPerRow * tilesHigh;
    const size_t bytesToWrite = (size_t)numTiles << 6;

    u8* data = (u8*)backup;
    memset(data, 0, bytesToWrite);

    for (int t = 0; t < numTiles; ++t) {
        int tileX = (t % tilesPerRow) << 3;
        int tileY = (t / tilesPerRow) << 3;
        size_t base = (size_t)t << 6;

        for (int row = 0; row < 8; ++row) {
            u8 p0=0,p1=0,p2=0,p3=0,p4=0,p5=0,p6=0,p7=0;
            int srcBase = (tileY + row) * 128 + tileX;

            for (int x = 0; x < 8; ++x) {
                u16 pix = surface[srcBase + x] & 0xFF;
                int bit = 7 - x;

                p0 |= ((pix >> 0) & 1) << bit;
                p1 |= ((pix >> 1) & 1) << bit;
                p2 |= ((pix >> 2) & 1) << bit;
                p3 |= ((pix >> 3) & 1) << bit;
                p4 |= ((pix >> 4) & 1) << bit;
                p5 |= ((pix >> 5) & 1) << bit;
                p6 |= ((pix >> 6) & 1) << bit;
                p7 |= ((pix >> 7) & 1) << bit;
            }

            data[base + row*2 + 0]  = p0;
            data[base + row*2 + 1]  = p1;
            data[base + 16 + row*2] = p2;
            data[base + 16 + row*2 + 1] = p3;
            data[base + 32 + row*2] = p4;
            data[base + 32 + row*2 + 1] = p5;
            data[base + 48 + row*2] = p6;
            data[base + 48 + row*2 + 1] = p7;
        }
    }

    size_t written = fwrite(data, 1, bytesToWrite, f);
    fclose(f);
    if (written != bytesToWrite) return -1;

    return (int)written;
}

// Smallest N where 2^N >= value  (result clamped to [0,7])
static int ceilLog2(unsigned int v) {
    int n = 0;
    while ((1u << n) < v && n < 7) n++;
    return n;
}

// ARGB1555 → RGBA8888
static u32 argb1555_to_rgba8888(u16 c) {
    // NDS ARGB1555: bit15=A, bits14-10=R, bits9-5=G, bits4-0=B
    u8 a = (c >> 15) & 1  ? 255 : 0;
    u8 r = ((c >> 10) & 0x1F) * 255 / 31;
    u8 g = ((c >>  5) & 0x1F) * 255 / 31;
    u8 b = ( c        & 0x1F) * 255 / 31;
    return ((u32)b << 24) | ((u32)g << 16) | ((u32)r << 8) | a;
}

// RGBA8888 → ARGB1555
static u16 rgba8888_to_argb1555(u8 r, u8 g, u8 b, u8 a) {
    u16 A = (a >= 128) ? 1 : 0;
    u16 R = (r * 31 + 127) / 255;
    u16 G = (g * 31 + 127) / 255;
    u16 B = (b * 31 + 127) / 255;
    return (A << 15) | (B << 10) | (G << 5) | R;
}

// Find or add color in palette; returns index or -1 if palette is full
static int palette_find_or_add(u16* pal, u16 color) {
    for (int i = 0; i < paletteSize; i++)
        if (pal[i] == color) return i;
    if (paletteSize >= 256) return -1;
    pal[paletteSize] = color;
    return paletteSize++;
}

int png_export(const char *path, const u16 *surf, const u16 *pal) {
    int w = 1 << surfaceXres;
    int h = 1 << surfaceYres;

    u8 *rgba = (u8 *)backup;

    if (paletteBpp != 16) {
        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                u16 idx  = surf[i * w + j] & 0xFF;
                u16 c    = (idx < 256) ? pal[idx] : 0;
                u32 rgba8 = argb1555_to_rgba8888(c);
                int p     = (i * w + j) * 4;
                rgba[p+0] = (rgba8 >> 24) & 0xFF;
                rgba[p+1] = (rgba8 >> 16) & 0xFF;
                rgba[p+2] = (rgba8 >>  8) & 0xFF;
                rgba[p+3] =  rgba8        & 0xFF;
            }
        }
    } else {
        for (int i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                u16 c    = surf[i * w + j];
                u32 rgba8 = argb1555_to_rgba8888(c);
                int p     = (i * w + j) * 4;
                rgba[p+0] = (rgba8 >> 24) & 0xFF;
                rgba[p+1] = (rgba8 >> 16) & 0xFF;
                rgba[p+2] = (rgba8 >>  8) & 0xFF;
                rgba[p+3] =  rgba8        & 0xFF;
            }
        }
    }

    unsigned error = lodepng_encode32_file(path, rgba, (unsigned)w, (unsigned)h);
    return (int)error;
}

int png_import(const char *path, u16 *surf, u16 *pal) {
    unsigned imgW, imgH;

    u8 *rgba = (u8 *)backup;

    u8 *tmp = NULL;
    unsigned error = lodepng_decode32_file(&tmp, &imgW, &imgH, path);
    if (error) return (int)error;

    if (imgW > 128 || imgH > 128) {
        free(tmp);
        return -1;
    }
    memcpy(rgba, tmp, imgW * imgH * 4);
    free(tmp);

    u8 *bitset = (u8 *)stack;
    memset(bitset, 0, 8192);

    int uniqueColors = 0;
    for (unsigned y = 0; y < imgH; y++) {
        for (unsigned x = 0; x < imgW; x++) {
            int p = (y * imgW + x) * 4;
            u16 c = rgba8888_to_argb1555(rgba[p], rgba[p+1], rgba[p+2], rgba[p+3]);
            if (!(bitset[c >> 3] & (1 << (c & 7)))) {
                bitset[c >> 3] |= (1 << (c & 7));
                uniqueColors++;
            }
        }
    }

    paletteBpp = (uniqueColors <= 256) ? 8 : 16;

    surfaceXres = ceilLog2(imgW);
    surfaceYres = ceilLog2(imgH);

    int sw = 1 << surfaceXres;
    int sh = 1 << surfaceYres;

    memset(surf, 0, sw * sh * sizeof(u16));

    if (paletteBpp == 8) {
        paletteSize = 0;
        for (unsigned y = 0; y < imgH; y++) {
            for (unsigned x = 0; x < imgW; x++) {
                int p   = (y * imgW + x) * 4;
                u16 c   = rgba8888_to_argb1555(rgba[p], rgba[p+1], rgba[p+2], rgba[p+3]);
                int idx = palette_find_or_add(pal, c);
                surf[y * sw + x] = (u16)idx;
            }
        }
    } else {
        for (unsigned y = 0; y < imgH; y++) {
            for (unsigned x = 0; x < imgW; x++) {
                int p = (y * imgW + x) * 4;
                surf[y * sw + x] = rgba8888_to_argb1555(rgba[p], rgba[p+1], rgba[p+2], rgba[p+3]);
            }
        }
    }

    paletteSize = 256;
    return 0;
}