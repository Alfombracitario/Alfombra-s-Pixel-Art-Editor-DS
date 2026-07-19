#ifndef FORMATSGLOBALS_H
#define FORMATSGLOBALS_H

#define surfaceSize SURFACE_H *SURFACE_W
#define SURFACE_W 128
#define SURFACE_H 128

extern u16 surface[surfaceSize];
extern u16 backup[131072];
extern u16 palette[256];
extern u16 stack[16384];
extern int surfaceXres;
extern int surfaceYres;
extern int paletteSize;
extern int paletteBpp;

#endif