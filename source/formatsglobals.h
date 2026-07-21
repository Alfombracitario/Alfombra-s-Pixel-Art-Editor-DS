#ifndef FORMATSGLOBALS_H
#define FORMATSGLOBALS_H

#define SURFACE_W 128
#define SURFACE_H 128
#define surfaceSize 128*128
#define BACKUP_SIZE (surfaceSize * 80)


extern u16 surface[surfaceSize];
extern u16 backup[BACKUP_SIZE];
extern u16 palette[256];
extern u16 stack[surfaceSize];
extern int surfaceXres;
extern int surfaceYres;
extern int paletteSize;
extern int paletteBpp;

#endif