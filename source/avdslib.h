#ifndef AVDSLIB_H
#define AVDSLIB_H

#include <nds.h>   // para u16, dmaFillHalfWords y cosas de NDS
// --------------------------------------------------
// Funciones de colores
// --------------------------------------------------
u16 AVARGB(int r, int g, int b, int a = 1);
u16 AVinvertColor(u16 color);

// --------------------------------------------------
// Funciones de manipulación de arrays
// --------------------------------------------------
void AVfillDMA(u16 *arr, int start, int end, u16 value);

// --------------------------------------------------
// Funciones de dibujo
// --------------------------------------------------
void AVsetPixel(u16* arr, int x, int y, u16 color);
u16 AVreadPixel(u16* arr, int x, int y);

void AVdrawRectangle(u16* arr, int x, int width, int y, int height, u16 color);
void AVdrawRectangleDMA(u16* arr, int x, int width, int y, int height, u16 color);
void AVdrawRectangleHollow(u16* arr, int x, int width, int y, int height, u16 color);

void AVdrawVline(u16* arr, int y0, int y1, int x, u16 color);
void AVdrawHlineDMA(u16* arr, int x0, int x1, int y, u16 color);

//planeo crear la función para cargar .acs y guardarlo
#endif // AVDSLIB_H
