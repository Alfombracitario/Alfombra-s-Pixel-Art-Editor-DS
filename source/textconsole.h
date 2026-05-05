#pragma once
#include <nds.h>

// Enums compartidos con main.cpp
enum subMode { SUB_TEXT, SUB_BITMAP };
enum consoleMode { MODE_NO, LOAD_file, SAVE_file, IMAGE_SETTINGS, MODE_NEWIMAGE };

// Variables de main.cpp que textconsole necesita
extern u32 kDown;
extern u32 kHeld;
extern u32 kUp;
extern touchPosition touch;
extern int holdTimer;
extern bool redraw;
extern bool updated;
extern bool accurate;
extern bool nesMode;
extern bool hasClipboard;
extern bool usesPages;
extern int imgFormat;
extern int fileOffset;
extern int resX;
extern int resY;
extern int surfaceXres;
extern int surfaceYres;
extern int paletteBpp;
extern int subSurfaceZoom;
extern int palettePos;
extern u16 palette[];
extern u16 surface[];
extern subMode currentSubMode;
extern consoleMode currentConsoleMode;

// Funciones de main.cpp que textconsole llama
void bitmapMode();
void textMode();
void clearAll();
void clearTop();
void drawColorPalette();
void drawNesPalette();
void drawSurfaceMain(bool full);
void drawSurfaceBottom();
void setBackupVariables();

// Punto de entrada del modo texto
// Retorna true si debe saltar a frameEnd al volver a bitmap
bool runTextConsole();