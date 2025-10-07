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
    FILE* f = fopen(path, "rb");
    if (!f) {
        iprintf("File not found %s\n", path);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    int chrSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (chrSize > (131072 * 2)) {
        fclose(f);
        iprintf("File too large (%d bytes)\n", chrSize);
        return -2;
    }

    u8* chrData = (u8*)backup;
    fread(chrData, 1, chrSize, f);
    fclose(f);

    int numTiles = chrSize >> 5; // 32 bytes por tile
    int tilesPerRow = 16; // 16 tiles por fila

    for (int t = 0; t < numTiles; t++) {
        int tileX = (t % tilesPerRow) << 3;
        int tileY = (t / tilesPerRow) << 3;

        const u8* tile = &chrData[t << 5];

        for (int row = 0; row < 8; row++) {
            u8 p0 = tile[row];
            u8 p1 = tile[row + 8];
            u8 p2 = tile[row + 16];
            u8 p3 = tile[row + 24];

            for (int col = 0; col < 8; col++) {
                int bit = 7 - col;
                u16 pixel =
                    (((p3 >> bit) & 1) << 3) |
                    (((p2 >> bit) & 1) << 2) |
                    (((p1 >> bit) & 1) << 1) |
                    ((p0 >> bit) & 1);

                int x = tileX + col;
                int y = tileY + row;
                surface[x + (y << 7)] = pixel;
            }
        }
    }

    return 0;
}

int importPal(const char* path){
    //importa una paleta en el formato de YYCHR
    //RGB 888
    FILE* f = fopen(path, "rb");
    if (!f) {
        iprintf("File not found %s\n", path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    int palSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if(palSize > 768)
    {
        iprintf("Error: the file is too big");
        return -1;
    }
    //copiar los datos al array para leer desde ahí (es más rápido!)
    u8* chrData = (u8*)backup;
    fread(chrData, 1, palSize, f);
    fclose(f);

    //ahora podemos decodificar el array!

    int colors = palSize / 3;//AYUDA UNA DIVISIÓN AAAA (perdoname DSi por esto)

    for (int i = 0; i < colors; i++) {
        int idx = i * 3;

        u16 r = backup[idx]     >> 3;
        u16 g = backup[idx + 1] >> 3;
        u16 b = backup[idx + 2] >> 3;

        palette[i] = 0x8000 | (b << 10) | (g << 5) | r;
    }
    return 0;
}

int exportPal(const char* path){
    return 0;
}