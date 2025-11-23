#ifndef FORMATS_H
#define FORMATS_H

#include <nds.h>

int importNES(const char* path, u16* surface);
int exportNES(const char* path, u16* surface, int height);

int importGBC(const char* path, u16* surface);
int exportGBC(const char* path, u16* surface, int height);

int importSNES(const char* path, u16* surface);
int exportSNES(const char* path, u16* surface, int height);

int importGBA(const char* path, u16* surface);
int exportGBA(const char* path, u16* surface, int height);

int importPCX(const char* path, u16* surface);
int exportPCX(const char* path, u16* surface, int width, int height);

int importPal(const char* path);
int exportPal(const char* path);

int importGIF(const char* path, u16* surface);
int exportGIF(const char* path, u16* surface, int width, int height);

int importTGA(const char* path, u16* surface);
int exportTGA(const char* path, u16* surface, int width, int height);

//void writeBmpHeader(FILE *f);
void saveBMP_indexed(const char* filename, uint16_t* palette, uint16_t* surface);
int loadBMP_indexed(const char* filename, uint16_t* palette, uint16_t* surface);

void saveBMP(const char* filename, uint16_t* palette, uint16_t* surface);
int loadBMP_direct(const char* filename, uint16_t* surface);

int saveBMP_4bpp(const char* filename, uint16_t* palette, uint16_t* surface);
int loadBMP_4bpp(const char* filename, uint16_t* palette, uint16_t* surface);

//acs
void exportACS(const char* path, u16* surface);
void importACS(const char* path, u16* surface);

#endif // FORMATS_H