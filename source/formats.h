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

#endif // FORMATS_H