#ifndef FILES_H
#define FILES_H

#include <nds.h>
#include "formats.h"
#include <fat.h>
#include <dirent.h>

#define MAX_TEXT_LENGTH 32
#define MAX_FILES 256
#define MAX_NAME_LEN 32
#define SCREEN_LINES 20

extern char fname[MAX_TEXT_LENGTH];
extern DIR* currentDir;
extern struct dirent entryList[MAX_FILES];
extern int fileCount;
extern u8 sortedIdx[MAX_FILES];

extern char path[257];
extern char currentFilePath[257];  // faltaba
extern char format[6];             // faltaba
extern u16* bgPreviewGfx;          // faltaba

extern int selector;
extern int selectorA;

// funciones — faltaban todas
void listFiles();
void saveFile(int, char*, u16*, u16*);
void loadFile(int, char*, u16*, u16*);
int  enterFolder(int);
int  goBack();
void buildCurrentFilePath();
void previewFile(const char*);

#endif