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

int importPal1555(const char* path);
int exportPal1555(const char* path);

int importSNES8bpp(const char* path, u16* surface);
int exportSNES8bpp(const char* path, u16* surface, int height);

void saveBMP_indexed(const char* filename, uint16_t* palette, uint16_t* surface);
int loadBMP_indexed(const char* filename, uint16_t* palette, uint16_t* surface);

void saveBMP(const char* filename, uint16_t* palette, uint16_t* surface);
int loadBMP_direct(const char* filename, uint16_t* surface);

void saveBMP_4bpp(const char* filename, uint16_t* palette, uint16_t* surface);
int loadBMP_4bpp(const char* filename, uint16_t* palette, uint16_t* surface);

//acs
void exportACS(const char* path, u16* surface);
void importACS(const char* path, u16* surface);

//macros
#define formatDirectBMP 0
#define format8bppBMP 1
#define format4bppBMP 2
#define formatNES 3
#define formatGBC 4
#define formatSNES4 5
#define formatGBA4 6
#define formatPCX 7
#define formatPAL 8
#define formatSNES8 9
#define formatPal1555 10
#define formatACS 11

#endif // FORMATS_H