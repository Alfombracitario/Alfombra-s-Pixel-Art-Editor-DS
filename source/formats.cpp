#include <nds.h>
#include <stdio.h>

// =========================
// NES (.chr) IMPORT / EXPORT desde SD
// =========================

// Importa archivo NES (.chr) desde SD a surface[]
// path: ruta del archivo en la SD (ej: "/nes/graphics.chr")
// surface: tu buffer 128x64 o 128x128 (u16)
extern u16 backup[131072]; // declarado en main.cpp
extern u16 palette[256];

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


// Exporta surface[] como archivo NES (.chr) en la SD
// path: ruta del archivo a guardar
// surface: buffer 128x64 o 128x128
// height: altura de la surface (64 o 128)
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

// Exporta desde surface (128x128 u16 con índices 0..15) a archivo SNES 4bpp.
// height = altura en píxeles a exportar (debe ser múltiplo de 8 y <= 128).
// Retorna número de bytes escritos (>0) o -1 en error.
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
int importPal(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        iprintf("File not found %s\n", path);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    int palSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (palSize > 768) {
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
    return 0;
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

bool decodeAcs(const char* file_path, u16* surface) {
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
    u8 PPB = 0;
    
    if (PalCount < 256) {
        BPP = 8;
        PPB = 1;
        if (PalCount < 16) {
            BPP = 4;
            PPB = 2;
            if (PalCount <= 4) {
                BPP = 2;
                PPB = 4;
                if (PalCount <= 2) {
                    BPP = 1;
                    PPB = 8;
                }
            }
        }
    }
    if (PalCount == 0) {
        BPP = 0;
        PPB = 0;
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