#ifndef FORMATS_H
#define FORMATS_H

#include <nds.h>   // para u16, dmaFillHalfWords y cosas de NDS

int importNES(const char* path, u16* surface);

int exportNES(const char* path, u16* surface, int height);

int importSNES(const char* path, u16* surface);

int exportSNES(const char* path, u16* surface, int height);

int importPal(const char* path);

int exportPal(const char* path);

bool decodeAcs(const char* file_path, u16* surface);

void writeBmpHeader(FILE *f);

void saveBMP(const char* filename, uint16_t* palette, uint16_t* surface);

void saveBMP_indexed(const char* filename, uint16_t* palette, uint16_t* surface);

int loadBMP_indexed(const char* filename, uint16_t* palette, uint16_t* surface);

int saveBMP_4bpp(const char* filename, uint16_t* palette, uint16_t* surface);

int loadBMP_4bpp(const char* filename, uint16_t* palette, uint16_t* surface);

#endif // FORMATS_H