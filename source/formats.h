#ifndef FORMATS_H
#define FORMATS_H

#include <nds.h>
#include "acs.h"
#include "png/lodepng.h"

int importNES(const char* path, u16* surface);
int exportNES(const char* path, u16* surface, int height);

int importGBC(const char* path, u16* surface);
int exportGBC(const char* path, u16* surface, int height);

int importSNES(const char* path, u16* surface);
int exportSNES(const char* path, u16* surface, int height);

int importGBA(const char* path, u16* surface);
int exportGBA(const char* path, u16* surface, int height);

int importPCX(const char* path, u16* surface, u16* pal);
int exportPCX(const char* path, u16* surface, u16* pal, int width, int height);

int importPal(const char* path, u16* pal);
int exportPal(const char* path, u16* pal);

int importPal1555(const char* path, u16* pal);
int exportPal1555(const char* path, u16* pal);

int importSNES8bpp(const char* path, u16* surface);
int exportSNES8bpp(const char* path, u16* surface, int height);

void saveBMP_indexed(const char* filename, uint16_t* pal, uint16_t* surface);
int  loadBMP_indexed(const char* filename, uint16_t* pal, uint16_t* surface);

void saveBMP(const char* filename, uint16_t* pal, uint16_t* surface);
int  loadBMP_direct(const char* filename, uint16_t* surface);

void saveBMP_4bpp(const char* filename, uint16_t* pal, uint16_t* surface);
int  loadBMP_4bpp(const char* filename, uint16_t* pal, uint16_t* surface);

//png
int png_import(const char *path, u16 *surf, u16 *pal);
int png_export(const char *path, const u16 *surf, const u16 *pal);

//macros
#define formatDirectBMP 0
#define format8bppBMP   1
#define format4bppBMP   2
#define formatNES       3
#define formatGBC       4
#define formatSNES4     5
#define formatGBA4      6
#define formatPCX       7
#define formatPAL       8
#define formatSNES8     9
#define formatPal1555   10
#define formatACS       11
#define formatPNG       12
#define formatGIF       13

#define MaxFormats 13

const char texts[MaxFormats][24] = {
    ".bmp [direct]",
    ".bmp [8bpp]",
    ".bmp [4bpp]",
    ".bin [NES]",
    ".bin [GB 2bpp]",
    ".bin [SNES 4bpp]",
    ".bin [GBA 4bpp]",
    ".pcx",
    ".pal [YY-CHR]",
    ".bin [SNES 8bpp]",
    ".pal [ARGB 1555]",
    ".acs [Custom format]",
    ".png"
};
const char formats[MaxFormats][6] = {
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
    ".pal",
    ".acs",
    ".png"
};

#endif // FORMATS_H