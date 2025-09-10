//Este código, es como una "librería" ya que la utlizaré para todos mis proyectos de DS en C/C++
/*
    Alfombra's visual DS library.
*/
#include <nds.h>


// Cosas comunes
u16 AVARGB(int r, int g, int b, int a = 1) {
    return ((a & 1) << 15)       // Alpha en bit 15
         | ((b & 31) << 10)      // Azul
         | ((g & 31) << 5)       // Verde
         |  (r & 31);            // Rojo
}

u16 AVinvertColor(u16 color)
{
    return (~color) | 0x8000; //recordemos que alpha debe ser 1
}

void AVfillDMA(u16 *arr, int start, int end, u16 value) {
    int count = end - start;
    dmaFillHalfWords(value, &arr[start], count << 1);  
}



//específico para dibujado
void AVsetPixel(u16* arr, int x, int y, u16 color) {
    arr[(y<<8) + x] = color;
}

u16 AVreadPixel(u16* arr, int x, int y)
{
    return arr[(y<<8) + x];
}

void AVdrawRectangle(u16* arr, int x,int width, int y,int height, u16 color){
    int xlimit = x+width;
    int ylimit = y+height;
    for(int i = y; i < ylimit; i++)//eje vertical
    {
        int _y = i<<8;
        for(int j = x; j < xlimit; j++)//eje horizontal
        {
            arr[_y + j] = color;
        }
    }
}

void AVdrawRectangleDMA(u16* arr, int x, int width, int y, int height, u16 color) {
    int xto = x + width;
    height += y;
    for (int i = y; i < height; i++) {
        int _i = i << 8;
        dmaFillHalfWords(color, &arr[_i + x], (xto - x) << 1);
    }
}


void AVdrawRectangleHollow(u16* arr, int x, int width, int y, int height, u16 color) {
    int xlimit = x + width;
    int ylimit = y + height;

    // línea superior e inferior
    for (int i = x; i < xlimit; i++) {
        arr[(y << 8) + i]         = color;       // top
        arr[((ylimit - 1) << 8) + i] = color;    // bottom
    }

    // líneas laterales
    for (int j = y + 1; j < (ylimit - 1); j++) {
        arr[(j << 8) + x]         = color;       // left
        arr[(j << 8) + (xlimit - 1)] = color;    // right
    }
}

void AVdrawVline(u16* arr,int y0, int y1, int x, u16 color)
{
    y0 = (y0<<8)+x;
    y1 = (y1<<8)+x;
    for(int i = y0; i < y1; i+=256)
    {
        arr[i] = color;
    }
}

void AVdrawHlineDMA(u16* arr, int x0, int x1, int y, u16 color) {
    dmaFillHalfWords(color, &arr[(y << 8) + x0], (x1 - x0) << 1);
}
