#include "textconsole.h"
#include "files.h"

#include "GFXnewImageInput.h"
#include "GFXconsoleInput.h"

#include <stdio.h>
#include <string.h>

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

// Variables internas del selector de archivos (definidas en files.cpp)
extern int selector;
extern int selectorA;
extern int fileCount;
extern char fname[];
extern char format[];
extern char currentFilePath[];
extern bool mayus;

//input
const char *keyboardLower = "1234567890()#@qwertyuiop[]$^asdfghjkl/{}%!|zxcvbnm>>-_~=<<      ,.'`;+";
const char *keyboardUpper = "1234567890()#@QWERTYUIOP[]$^ASDFGHJKL/{}%!|ZXCVBNM>>-_~=<<      ,.'`;+";
char getKeyboardKey(int x, int y){
    //primero arreglamos las teclas a un margen
    x=(x>>4)-1;//dividir en 16

    y = (y>>4)-7;//pasar a un rango de 0 a 5

    //combinar a una sola variable
    x = x+(y*14);

    //convertir el valor a un dato 
    const char *output = mayus ? keyboardUpper : keyboardLower;
    return output[x];
}

void handleKey(char key) {
    int len = strlen(fname);

    switch (key) {
        case '/': // borrar
            if (len > 0) fname[len - 1] = '\0';
            break;

        case '|': // alternar mayúsculas
            mayus = !mayus;
            break;

        case '>': // enter
            fname[0] = '\0';
            kDown = KEY_START;
            break;

        case '<': // salir
            kDown = KEY_SELECT;
            break;

        default:
            // agregar carácter normal
            if (len < 15 && key != '\0') {
                // si mayúsculas activadas y es letra
                if (mayus && key >= 'a' && key <= 'z') {
                    key -= 32; // convierte a mayúscula ('a' → 'A')
                }

                fname[len] = key;
                fname[len + 1] = '\0';
            }
            break;
    }
}


// ======================== MODO NUEVA IMAGEN ========================

static bool handleNewImage()
{
    const int bpps[5] = {2, 4, 8, 2, 16};

    consoleClear();
    printf("Create new file:\n");
    printf("Resolution: %d", 1 << resX);
    printf("x%d", 1 << resY);
    printf("\nColors:%d", 1 << selectorA);

    if (selector == 3) printf("\nNes mode");
    if (selector == 4) printf("\nDirect color mode");

    if (kDown & KEY_RIGHT)
    {
        selector++;
        if (selector > 3) selector = 0;
        selectorA = bpps[selector];
    }
    else if (kDown & KEY_LEFT)
    {
        selector--;
        if (selector < 0) selector = 3;
        selectorA = bpps[selector];
    }

    if (kDown & KEY_START || kDown & KEY_A)
    {
        nesMode      = false;
        surfaceXres  = resX;
        surfaceYres  = resY;
        paletteBpp   = selectorA;
        subSurfaceZoom = 7 - MAX(surfaceXres, surfaceYres);

        clearAll();
        bitmapMode();
        drawColorPalette();

        if (selector == 3)
        {
            nesMode = true;
            drawNesPalette();
        }
        selector = 0;
        return true; // señal: saltar a frameEnd en bitmap
    }

    if (kDown & KEY_TOUCH)
    {
        int x = touch.px;
        int y = touch.py - 48;
        int option = y >> 5;
        switch (option)
        {
            case 0: // Colores
                selector  = (x - 8) / 48;
                selector  = MIN(selector, 4);
                selectorA = bpps[selector];
                break;
            case 1: // Xres
                if (x > 64) resX = x >> 5;
                break;
            case 2: // Yres
                if (x > 64) resY = x >> 5;
                break;
        }
    }

    return false;
}

// ======================== MODO ARCHIVO (LOAD/SAVE) ========================

// Retorna true si debe saltar a frameEnd
static bool handleFileConsole()
{
    if (currentConsoleMode == SAVE_file)
    {
        if (kDown & KEY_START)
        {
            buildCurrentFilePath();
            nesMode = false;
            if (selectorA == formatPAL)
                saveFile(formatPAL, currentFilePath, palette, surface);
            else
            {
                imgFormat = selectorA;
                saveFile(imgFormat, currentFilePath, palette, surface);
            }
            bitmapMode();
            return true;
        }
    }
    else if (currentConsoleMode == LOAD_file)// LOAD_file
    {
        if (kDown & KEY_START)
        {
            hasClipboard = false;
            buildCurrentFilePath();
            clearTop();
            nesMode = false;
            if (selectorA == formatPAL)
                loadFile(formatPAL, currentFilePath, palette, surface);
            else
            {
                imgFormat = selectorA;
                loadFile(imgFormat, currentFilePath, palette, surface);
            }
            drawSurfaceMain();
            drawSurfaceBottom();
            drawColorPalette();
            setBackupVariables();
            bitmapMode();
            return true;
        }
    }

    // Navegación con hold
    if (holdTimer > 10)
    {
        if (kHeld & KEY_RIGHT) { selectorA++; redraw = true; consoleClear(); }
        if (kHeld & KEY_LEFT)  { selectorA--; redraw = true; consoleClear(); }
        if (kHeld & KEY_UP   && selector > 0)             { selector--; redraw = true; consoleClear(); }
        if (kHeld & KEY_DOWN && selector < fileCount - 1) { selector++; redraw = true; consoleClear(); }
    }
    else
    {
        if (kDown & KEY_RIGHT) { selectorA++; redraw = true; consoleClear(); }
        if (kDown & KEY_LEFT)  { selectorA--; redraw = true; consoleClear(); }
        if (kDown & KEY_UP   && selector > 0)             { selector--; redraw = true; consoleClear(); }
        if (kDown & KEY_DOWN && selector < fileCount - 1) { selector++; redraw = true; consoleClear(); }
    }

    if (selectorA >= MaxFormats)    selectorA = 0;
    else if (selectorA < 0)         selectorA = MaxFormats - 1;

    strcpy(format, formats[selectorA]);

    // Teclado táctil
    if (kDown & KEY_TOUCH && touch.py > 112)
    {
        redraw = true;
        consoleClear();
        char key = getKeyboardKey(touch.px, touch.py);
        if (key != '\0') handleKey(key);
    }

    // Abrir archivo o entrar en directorio
    if (kDown & KEY_A)
    {
        redraw = true;
        consoleClear();
        if (selector < 0 || selector >= fileCount)
        {
            printf("Error");
        }
        else if (entryList[sortedIdx[selector]].d_type == DT_DIR)
        {
            enterFolder(selector);
        }
        else
        {
            strncpy(fname, entryList[sortedIdx[selector]].d_name, sizeof(fname));
            format[0] = '\0';
            // Simular KEY_START para abrir — volvemos a evaluar desde el top
            kDown = KEY_START;
            return handleFileConsole(); // recursión de un nivel, resuelve el open
        }
    }

    if (kDown & KEY_B) goBack();

    if (redraw)
    {
        printf(fname);
        printf(texts[selectorA]);
        printf("\n????????????????????????????????\n");
        listFiles();
        redraw = false;
    }

    return false;
}

// ======================== ENTRY POINT ========================

bool runTextConsole()
{
    // limpiar input residual del frame que nos llamó
    scanKeys();
    kDown = 0;
    kHeld = 0;
    kUp   = 0;

    while (true)
    {
        swiWaitForVBlank();
        scanKeys();
        kDown = keysDown();
        kHeld = keysHeld();
        kUp   = keysUp();
        touchRead(&touch);
        holdTimer = kDown ? holdTimer + 1 : 0;

        if (kDown & KEY_SELECT)
        {
            bitmapMode();
            currentConsoleMode = MODE_NO; // o el valor neutro que tengas
            currentSubMode = SUB_BITMAP;
            return false;
        }

        if (currentConsoleMode == MODE_NEWIMAGE)
        {
            if (handleNewImage()) { currentConsoleMode = MODE_NO; return true; }
        }
        else if (currentConsoleMode == SAVE_file)
        {
            if (handleFileConsole()) { currentConsoleMode = MODE_NO; return true; }
        }
        else if (currentConsoleMode == LOAD_file)
        {
            if (handleFileConsole()) { currentConsoleMode = MODE_NO; return true; }
        }
    }
}