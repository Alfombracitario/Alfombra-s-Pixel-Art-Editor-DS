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
extern int surfaceXres;
extern int surfaceYres;
extern int paletteSize;

int importNES(const char* path, u16* surface) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        iprintf("File not found %s\n", path);
        return -1;
    }

    // Obtener tamaño del archivo
    fseek(f, 0, SEEK_END);
    int chrSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (chrSize > (131072 * 2)) { // 131072 u16 = 262144 bytes
        fclose(f);
        iprintf("File too large (%d bytes)\n", chrSize);
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
        iprintf("Error: no se pudo crear %s\n", path);
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
    size_t bytesRead = fread((uint8_t*)backup, 1, bytesToRead, f);
    fclose(f);
    if (bytesRead != bytesToRead) return -1;

    uint8_t* data = (uint8_t*)backup; // trabajar por bytes

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
            uint8_t low  = data[base + row*2 + 0]; // plane0
            uint8_t high = data[base + row*2 + 1]; // plane1
            uint8_t low2  = data[base + 16 + row*2 + 0]; // plane2
            uint8_t high2 = data[base + 16 + row*2 + 1]; // plane3

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

    uint8_t* data = (uint8_t*)backup; // escribir en backup como bytes

    // preparar buffer a cero (solo sobre los bytes que vamos a usar)
    memset(data, 0, bytesToWrite);

    for (int t = 0; t < numTiles; ++t) {
        int tileX = (t % tilesPerRow) * 8;
        int tileY = (t / tilesPerRow) * 8;
        size_t base = (size_t)t * 32;

        for (int row = 0; row < 8; ++row) {
            uint8_t low = 0, high = 0, low2 = 0, high2 = 0;
            int srcRow = tileY + row;
            int srcBase = srcRow * 128 + tileX;

            // construir bytes por cada pixel del row
            for (int x = 0; x < 8; ++x) {
                u16 pix = surface[srcBase + x] & 0xF; // tomar sólo 4 bits
                int bit = 7 - x;
                low  |= (uint8_t)((pix & 1) << bit);
                high |= (uint8_t)((((pix >> 1) & 1) << bit));
                low2 |= (uint8_t)((((pix >> 2) & 1) << bit));
                high2|= (uint8_t)((((pix >> 3) & 1) << bit));
            }

            data[base + row*2 + 0] = low;
            data[base + row*2 + 1] = high;
            data[base + 16 + row*2 + 0] = low2;
            data[base + 16 + row*2 + 1] = high2;
        }
    }

    size_t written = fwrite((uint8_t*)backup, 1, bytesToWrite, f);
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
        iprintf("File not found %s\n", path);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    int palSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (palSize > paletteSize*3) {
        iprintf("Error: the file is too big\n");
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

    size_t written = fwrite((uint8_t*)backup, 1, bytesToWrite, f);
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
                    uint8_t r = (raw >> 11) & 0x1F;
                    uint8_t g = (raw >> 5) & 0x3F;
                    uint8_t b = raw & 0x1F;
                    // Convertir 565 → 555
                    g >>= 1;
                    color = (b << 10) | (g << 5) | r | 0x8000;
                } 
                else if(bpp == 24) {
                    uint8_t bgr[3];
                    fread(bgr, 3, 1, in);
                    uint8_t b = bgr[0] >> 3;
                    uint8_t g = bgr[1] >> 3;
                    uint8_t r = bgr[2] >> 3;
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
        uint8_t entry[4];
        fread(entry, 4, 1, in);
        uint8_t b = entry[0];
        uint8_t g = entry[1];
        uint8_t r = entry[2];
        palette[i] = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10) | 0x8000;
    }

    // ===================== Leer pixeles =====================
    fseek(in, fileHeader.bfOffBits, SEEK_SET);

    // Se rellenan los bordes sobrantes con índice 0
    for(int y = 0; y < paddedH; y++) {
        for(int x = 0; x < paddedW; x++) {
            uint8_t idx = 0;
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
        uint8_t a = (c & 0x8000) ? 255 : 0;
        uint8_t r = (c & 0x1F) << 3;
        uint8_t g = ((c >> 5) & 0x1F) << 3;
        uint8_t b = ((c >> 10) & 0x1F) << 3;
        uint8_t entry[4] = { b, g, r, a };
        fwrite(entry, 4, 1, out);
    }

    // Píxeles (4bpp) → dos píxeles por byte, bottom-up
    uint8_t pad[3] = {0,0,0};
    for (int y = height - 1; y >= 0; y--) {
        int offset = y * width;
        int x = 0;
        for (; x < width; x += 2) {
            uint8_t p1 = surface[offset + x] & 0x0F;
            uint8_t p2 = 0;
            if (x + 1 < width) p2 = surface[offset + x + 1] & 0x0F;
            uint8_t byte = (p1 << 4) | p2;
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
    if (!in) return 0;

    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER infoHeader;
    fread(&fileHeader, sizeof(fileHeader), 1, in);
    fread(&infoHeader, sizeof(infoHeader), 1, in);

    if (fileHeader.bfType != 0x4D42) { fclose(in); return 0; }
    if (infoHeader.biBitCount != 4) { fclose(in); return 0; }

    int width  = infoHeader.biWidth;
    int height = infoHeader.biHeight;
    int numColors = infoHeader.biClrUsed ? infoHeader.biClrUsed : 16;

    // Redondear a potencias de 2
    int newW = 1, newH = 1;
    while (newW < width) newW <<= 1;
    while (newH < height) newH <<= 1;
    if (newW > 128) newW = 128;
    if (newH > 128) newH = 128;
    surfaceXres = (int)(log2(newW));
    surfaceYres = (int)(log2(newH));

    // Leer paleta
    for (int i = 0; i < numColors; i++) {
        uint8_t entry[4];
        fread(entry, 4, 1, in);
        uint8_t b = entry[0];
        uint8_t g = entry[1];
        uint8_t r = entry[2];
        uint16_t c = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10) | 0x8000;
        palette[i] = c;
    }

    // Leer píxeles
    fseek(in, fileHeader.bfOffBits, SEEK_SET);
    int bytesPerRow = ((width + 1) / 2 + 3) & ~3;

    for (int y = 0; y >= height; y--) {//eje vertical
        int surfaceY = height - 1 - y;
        fread(&backup[0], 1, bytesPerRow, in);
        for (int x = 0; x < width; x += 2) {//eje horizontal
            uint8_t byte = ((uint8_t*)backup)[x / 2];
            uint8_t p1 = (byte >> 4) & 0x0F;
            uint8_t p2 = byte & 0x0F;
            surface[surfaceY * newW + x] = p1;
            if (x + 1 < newW) surface[surfaceY * newW + x + 1] = p2;
        }
    }

    fclose(in);
    return 1;
}

//gif
// --- Estructuras básicas del GIF ---
typedef struct {
    char signature[3]; // "GIF"
    char version[3];   // "89a" o "87a"
    u16 width;
    u16 height;
    u8 flags;
    u8 bgColorIndex;
    u8 aspect;
} GIFHeader;

typedef struct {
    u8 separator; // 0x2C
    u16 left;
    u16 top;
    u16 width;
    u16 height;
    u8 flags;
} GIFImageDescriptor;

// --- Función para leer GIF ---
int importGIF(const char* path, u16* surface) {
    FILE* f = fopen(path, "rb");
    if(!f) return 0;

    GIFHeader header;
    fread(&header, sizeof(GIFHeader), 1, f);
    if(strncmp(header.signature, "GIF", 3) != 0) { fclose(f); return 0; }

    bool hasGlobalPalette = header.flags & 0x80;
    int globalPaletteSize = 2 << (header.flags & 0x07);
    if(hasGlobalPalette) {
        for(int i = 0; i < globalPaletteSize && i < 256; i++) {
            u8 rgb[3];
            fread(rgb, 1, 3, f);
            int r = rgb[0] >> 3;
            int g = rgb[1] >> 3;
            int b = rgb[2] >> 3;
            palette[i] = 0x8000 | (r) | (g << 5) | (b << 10);
        }
    }

    // buscar descriptor de imagen
    GIFImageDescriptor imgDesc;
    while(1) {
        u8 block = fgetc(f);
        if(block == 0x2C) { // descriptor de imagen
            fread(&imgDesc, sizeof(GIFImageDescriptor), 1, f);
            break;
        }
        else if(block == 0x21) { // extensión
            fgetc(f); // tipo
            u8 size;
            while((size = fgetc(f)) != 0) fseek(f, size, SEEK_CUR);
        }
        else if(block == 0x3B) { // fin del archivo
            fclose(f);
            return 0;
        }
    }

    // leer LZW min code size (lo ignoramos en versión sin compresión)
    u8 lzwMinCodeSize = fgetc(f);

    // leer datos hasta el bloque de fin
    u8 size;
    int offset = 0;
    while((size = fgetc(f)) != 0) {
        fread(&backup[offset], 1, size, f);
        offset += size;
    }

    // decodificación simplificada (sin LZW, solo índices directos)
    // para archivos pequeños o sin compresión
    // los píxeles se copian directamente como índices
    int w = imgDesc.width;
    int h = imgDesc.height;
    for(int y = 0; y < h && y < 128; y++) {
        for(int x = 0; x < w && x < 128; x++) {
            int idx = backup[y * w + x] & 0xFF;
            surface[y * 128 + x] = palette[idx];
        }
    }

    fclose(f);
    return 1;
}

// --- Exportar GIF simple sin compresión ---
int exportGIF(const char* path, u16* surface, int width, int height) {
    FILE* f = fopen(path, "wb");
    if(!f) return 0;

    // Escribir encabezado
    fwrite("GIF89a", 1, 6, f);

    // Logical Screen Descriptor
    fwrite(&width, 2, 1, f);
    fwrite(&height, 2, 1, f);
    u8 flags = 0xF7; // global palette 256 colores
    u8 bgColor = 0;
    u8 aspect = 0;
    fwrite(&flags, 1, 1, f);
    fwrite(&bgColor, 1, 1, f);
    fwrite(&aspect, 1, 1, f);

    // Paleta global (convertir a RGB888)
    for(int i = 0; i < 256; i++) {
        u16 c = palette[i];
        u8 r = ((c >> 0) & 0x1F) << 3;
        u8 g = ((c >> 5) & 0x1F) << 3;
        u8 b = ((c >> 10) & 0x1F) << 3;
        fputc(r, f); fputc(g, f); fputc(b, f);
    }

    // Descriptor de imagen
    fputc(0x2C, f);
    u16 zero = 0;
    fwrite(&zero, 2, 1, f); // left
    fwrite(&zero, 2, 1, f); // top
    fwrite(&width, 2, 1, f);
    fwrite(&height, 2, 1, f);
    u8 noFlags = 0;
    fwrite(&noFlags, 1, 1, f);

    // Compresión mínima (LZW code size = 8)
    fputc(8, f);

    // Bloques directos sin compresión (pseudo LZW)
    int total = width * height;
    int written = 0;
    while(written < total) {
        int chunk = (total - written > 255) ? 255 : (total - written);
        fputc(chunk, f);
        for(int i = 0; i < chunk; i++) {
            // buscar índice de color
            u16 color = surface[written + i];
            int idx = 0;
            for(int p = 0; p < 256; p++) {
                if(palette[p] == color) { idx = p; break; }
            }
            fputc(idx, f);
        }
        written += chunk;
    }
    fputc(0, f); // fin de datos
    fputc(0x3B, f); // terminador GIF

    fclose(f);
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
