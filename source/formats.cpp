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

    const int tilesPerRow = 128 / 8; // 16
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

    const int tilesPerRow = 128 / 8; // 16
    const int tilesHigh = height / 8;
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

//======================================================ACS==================================================================================================|
// Tabla de resoluciones predefinidas
const u16 res[] = {4,8,12,16,24,32,48,64,96,128,196,256,382,512,1024};

int use_command(u8 command, int _i, u8* image, int image_size) {
    int a = 0; // pixels advanced
    int action = command & 7;
    int parameter = (command & 248) >> 3;
    
    switch (action) {
        case 0: { // repeat X
            u8 c = image[_i];
            int rep = parameter;
            for (int k = 0; k < rep; k++) {
                if (_i + a + 1 >= image_size) break;
                image[_i + a + 1] = c;
                a++;
            }
        } break;
        
        case 1: { // mirror X
            int rep = parameter;
            for (int k = 0; k < rep; k++) {
                if (_i + a + 1 >= image_size || _i - a - 1 < 0) break;
                u8 c = image[_i - a - 1];
                image[_i + a + 1] = c;
                a++;
            }
        } break;
        
        case 2: { // pattern X
            int rep = parameter & 3;
            int back = (parameter >> 2) & 7;
            for (int k = 0; k < rep; k++) {
                for (int l = 0; l < back; l++) {
                    if (_i + a >= image_size || _i - back + a < 0) break;
                    u8 c = image[_i - back + a];
                    image[_i + a] = c;
                    a++;
                }
            }
        } break;
    }
    return a;
}

bool decodeAcs(const char* file_path, u16* surface) {//traducido con ia, funciona horrible, tendré que hacerlo a mano :(
    FILE* file = fopen(file_path, "rb");
    if (!file) return false;
    
    // Cargar archivo completo en backup
    fseek(file, 0, SEEK_END);
    u32 size = ftell(file);
    fseek(file, 0, SEEK_SET);
    fread(backup, 1, size, file);
    fclose(file);
    
    u32 pos = 0;
    u8* buffer = (u8*)backup;
    
    // Macro READ
    #define READ (buffer[pos++])
    
    u8 b = READ;
    u8 b2 = 0, b3 = 0;
    
    // HEADER: Leer información de resolución
    u8 format = b;
    b = READ;
    
    u16 xres, yres;
    
    // Leer resolución X
    u8 xres_index = (b & 0xF0) >> 4;
    if (xres_index == 15) {
        b2 = READ;
        b3 = READ;
        xres = (b2 << 8) | b3;
    } else {
        xres = res[xres_index];
    }
    
    // Leer resolución Y
    u8 yres_index = b & 0x0F;
    if (yres_index == 15) {
        b2 = READ;
        b3 = READ;
        yres = (b2 << 8) | b3;
    } else {
        yres = res[yres_index];
    }
    
    // Verificar que la resolución no exceda 128x128
    if (xres > 128 || yres > 128) {
        return false;
    }
    
    // Leer cantidad de colores en paleta
    u8 PalCount = READ;
    
    // Determinar bits por pixel
    u8 BPP = 0;
    
    if (PalCount < 256) {
        BPP = 8;
        if (PalCount < 16) {
            BPP = 4;
            if (PalCount <= 4) {
                BPP = 2;
                if (PalCount <= 2) {
                    BPP = 1;
                }
            }
        }
    }
    if (PalCount == 0) {
        BPP = 0;
    }
    
    // Inicializar paleta con alpha=1 (0x8000)
    for (int i = 0; i < 256; i++) {
        palette[i] = 0x8000; // Alpha siempre 1
    }
    
    // Generar paleta
    if (PalCount != 0) {
        switch (format) {
            default: { // ARGB 1555
                for (u16 i = 0; i < PalCount; i++) {
                    b = READ;
                    b2 = READ;
                    u16 color_val = (b << 8) | b2;
                    // Convertir ARGB1555 a RGB (mantener alpha=1)
                    palette[i] = 0x8000 | (color_val & 0x7FFF);
                }
            } break;
            
            case 1: { // grayscale 8-bit
                for (u16 i = 0; i < PalCount; i++) {
                    b = READ;
                    u8 gray = b >> 3; // Convertir a 5 bits
                    palette[i] = 0x8000 | (gray << 10) | (gray << 5) | gray;
                }
            } break;
            
            case 2: { // grayscale 4-bit packed
                for (u16 i = 0; i < PalCount; i += 2) {
                    b = READ;
                    u8 gray1 = (b & 0xF0) >> 4;
                    u8 gray2 = b & 0x0F;
                    
                    gray1 = gray1 >> 3; // Convertir a 5 bits
                    gray2 = gray2 >> 3;
                    
                    palette[i] = 0x8000 | (gray1 << 10) | (gray1 << 5) | gray1;
                    if (i + 1 < PalCount) {
                        palette[i + 1] = 0x8000 | (gray2 << 10) | (gray2 << 5) | gray2;
                    }
                }
            } break;
            
            case 3: { // ARGB 8888
                for (u16 i = 0; i < PalCount; i++) {
                    u8 a = READ; // Alpha (no se usa, siempre 1)
                    u8 r = READ;
                    u8 g = READ;
                    u8 b_val = READ;
                    
                    // Convertir RGB888 a RGB555
                    u8 r5 = r >> 3;
                    u8 g5 = g >> 3;
                    u8 b5 = b_val >> 3;
                    
                    palette[i] = 0x8000 | (r5 << 10) | (g5 << 5) | b5;
                }
            } break;
        }
    }
    
    // Crear buffer de imagen temporal
    int image_size = xres * yres;
    u8* image = (u8*)backup; // Usar segunda mitad del backup para la imagen
    
    // Leer datos de píxeles
    u32 data_remaining = size - pos;
    
    for (u32 i = 0; i < data_remaining; i++) {
        b = READ;
        u8 c = 0;
        
        switch (BPP) {
            case 0: { // color directo
                switch (format) {
                    default: { // ARGB 1555
                        if (b & 128) {
                            b2 = READ;
                            i++;
                            u16 color_val = (b << 8) | b2;
                            // Convertir a índice de paleta (buscar color existente)
                            u16 rgb_color = color_val & 0x7FFF;
                            u8 palette_index = 0;
                            
                            // Buscar el color en la paleta
                            for (u8 p = 0; p < PalCount; p++) {
                                if ((palette[p] & 0x7FFF) == rgb_color) {
                                    palette_index = p;
                                    break;
                                }
                            }
                            
                            int pixel_index = i >> 1;
                            if (pixel_index < image_size) {
                                image[pixel_index] = palette_index;
                            }
                        } else {
                            b = READ; // leer comando
                            int pixels_advanced = use_command(b, i >> 1, image, image_size);
                            i += pixels_advanced << 1;
                            i++; // del READ anterior
                        }
                    } break;
                    
                    case 1: { // grayscale 8-bit
                        // Buscar o asignar en paleta
                        u8 gray = b >> 3;
                        u16 gray_color = 0x8000 | (gray << 10) | (gray << 5) | gray;
                        u8 palette_index = 0;
                        
                        for (u8 p = 0; p < PalCount; p++) {
                            if (palette[p] == gray_color) {
                                palette_index = p;
                                break;
                            }
                        }
                        
                        if (i < image_size) {
                            image[i] = palette_index;
                        }
                    } break;
                    
                    case 3: { // ARGB 8888
                        u8 r = READ;
                        i++;
                        if (b > 0 && r != 0) {
                            u8 g = READ;
                            i++;
                            u8 b_val = READ;
                            i++;
                            
                            // Convertir a RGB555
                            u8 r5 = r >> 3;
                            u8 g5 = g >> 3;
                            u8 b5 = b_val >> 3;
                            u16 rgb_color = 0x8000 | (r5 << 10) | (g5 << 5) | b5;
                            
                            // Buscar en paleta
                            u8 palette_index = 0;
                            for (u8 p = 0; p < PalCount; p++) {
                                if (palette[p] == rgb_color) {
                                    palette_index = p;
                                    break;
                                }
                            }
                            
                            int pixel_index = i >> 2;
                            if (pixel_index < image_size) {
                                image[pixel_index] = palette_index;
                            }
                        } else {
                            b = READ; // leer comando
                            int pixels_advanced = use_command(b, i >> 2, image, image_size);
                            i += pixels_advanced << 2;
                            i++; // del READ anterior
                        }
                    } break;
                }
            } break;
            
            case 1: { // 1bpp
                for (u8 _i = 0; _i < 8; _i++) {
                    c = (b >> _i) & 1;
                    int pixel_index = (i << 3) + _i;
                    if (pixel_index < image_size) {
                        image[pixel_index] = c;
                    }
                }
            } break;
            
            case 2: { // 2bpp
                for (u8 _i = 0; _i < 4; _i++) {
                    u8 m = _i << 1;
                    c = (b >> m) & 3;
                    int pixel_index = (i << 2) + _i;
                    if (pixel_index < image_size) {
                        image[pixel_index] = c;
                    }
                }
            } break;
            
            case 4: { // 4bpp
                for (u8 _i = 0; _i < 2; _i++) {
                    u8 m = _i << 2;
                    c = (b >> m) & 15;
                    int pixel_index = (i << 1) + _i;
                    if (pixel_index < image_size) {
                        image[pixel_index] = c;
                    }
                }
            } break;
            
            case 8: { // 8bpp con comandos
                if (b != 0) {
                    c = b;
                    if (i < image_size) {
                        image[i] = c;
                    }
                } else {
                    b = READ; // leer byte de comando
                    i += use_command(b, i, image, image_size);
                    i++; // del READ anterior
                }
            } break;
        }
    }
    
    // Copiar imagen decodificada a surface
    for (int y = 0; y < yres && y < 128; y++) {
        for (int x = 0; x < xres && x < 128; x++) {
            int src_index = y * xres + x;
            int dst_index = y * 128 + x;
            surface[dst_index] = image[src_index];
        }
    }
    
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

    for (int y = height - 1; y >= 0; y--) {
        int surfaceY = height - 1 - y;
        fread(&backup[0], 1, bytesPerRow, in);
        for (int x = 0; x < width; x += 2) {
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