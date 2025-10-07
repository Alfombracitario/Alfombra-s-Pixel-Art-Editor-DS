#ifndef FORMATS_H
#define FORMATS_H

#include <nds.h>   // para u16, dmaFillHalfWords y cosas de NDS

int importNES(const char* path, u16* surface);

int exportNES(const char* path, u16* surface, int height);

int importSNES(const char* path, u16* surface);

void importPal(const char* path);

void exportPal(const char* path);

#endif // FORMATS_H