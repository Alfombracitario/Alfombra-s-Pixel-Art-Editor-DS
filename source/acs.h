#ifndef ACS_H
#define ACS_H
#include <nds.h>
#include "formatsglobals.h"
void exportACS(const char* path, u16* surface, u16* pal);
void importACS(const char* path, u16* surface, u16* pal);

#endif