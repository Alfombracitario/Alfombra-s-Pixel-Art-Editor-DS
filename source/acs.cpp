#include "acs.h"

static const u8 log2Lut[129] = {
//  0     1     2     3     4     5     6     7     8     9
    0xFF, 0,    1,    1,    2,    2,    2,    2,    3,    3,  // 0-9
    3,    3,    3,    3,    3,    3,    4,    4,    4,    4,  // 10-19
    4,    4,    4,    4,    4,    4,    4,    4,    4,    4,  // 20-29
    4,    4,    5,    5,    5,    5,    5,    5,    5,    5,  // 30-39
    5,    5,    5,    5,    5,    5,    5,    5,    5,    5,  // 40-49
    5,    5,    5,    5,    5,    5,    5,    5,    5,    5,  // 50-59
    5,    5,    5,    5,    6,    6,    6,    6,    6,    6,  // 60-69
    6,    6,    6,    6,    6,    6,    6,    6,    6,    6,  // 70-79
    6,    6,    6,    6,    6,    6,    6,    6,    6,    6,  // 80-89
    6,    6,    6,    6,    6,    6,    6,    6,    6,    6,  // 90-99
    6,    6,    6,    6,    6,    6,    6,    6,    6,    6,  // 100-109
    6,    6,    6,    6,    6,    6,    6,    6,    6,    6,  // 110-119
    6,    6,    6,    6,    6,    6,    6,    6,    7,        // 120-128
};
//advertencia, estos importadores y exportadores de ACS están optimizados específicamente para este editor de pixel art,
//ACS es un poco más complejo,
//pero simplemente ignoré ciertos aspectos para que encaje en el hardware de la DS y la estructura de este editor de pixelart
#define ACScolModeARGB1555   0
#define ACScolModeARGB8888   1
#define ACScolModeRGB888     2
#define ACScolModeGrayScale8 3
#define ACScolModeGrayScale4 4
#define ACStotalModes 5

#define ACSmirror 01
#define ACSpattern 00
#define ACSrepeat 1


// ============================================================================
//                      Funciones auxiliares
// ============================================================================

static inline uint32_t fastHash(const uint16_t* p, int len){
    uint32_t h = 2166136261u;
    for(int i=0;i<len;i++){
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

// Hash invertido
static inline uint32_t fastHashRev(const uint16_t* p, int len){
    uint32_t h = 2166136261u;
    for(int i=len-1; i>=0; i--){
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

// Confirmar mirror
static inline int isMirror(const uint16_t* a, const uint16_t* b, int len){
    for(int i=0;i<len;i++){
        if(a[i] != b[len-1-i]) return 0;
    }
    return 1;
}

// Confirmar igual
static inline int isEqual(const uint16_t* a, const uint16_t* b, int len){
    for(int i=0;i<len;i++){
        if(a[i] != b[i]) return 0;
    }
    return 1;
}

//función auxiliar
inline void readCommand7(u8 byte, int* pInd, u16* surface){
    //determinar cual es el tipo de comando contra el que estamos tratando
    switch((byte>>5) & 0b011){
        case ACSpattern:{
            //000P PPRR
            u8 repeat = (byte & 0b11)+1;
            u8 pixels = ((byte>>2) & 0b111)+2;

            int rInd = *pInd-pixels;
            u8 iterations = repeat*pixels;
            for(int i = 0; i < iterations; i++){
                surface[(*pInd)++] = surface[rInd++];
            }
        break;}
        case ACSmirror:{
            //001x xxxx
            //debemos leer para atrás y escribirlo hacia adelante
            int mInd = *pInd-1;
            u8 repeat = (byte & 0b11111)+2;
            for(int i = 0; i < repeat; i++){
                surface[(*pInd)++] = surface[mInd--];
            }
        break;}

        default:{//repetición simple (01xxxxxx)
            //obtener último color
            u16 col = surface[*pInd-1];
            u8 repeat = (byte & 0b111111)+1;
            for(int i = 0; i<repeat;i++){
                surface[(*pInd)++] = col;
            }
        break;}
    }
}
inline void readCommand8(u8 byte, int* pInd, u16* surface){
    switch(byte>>6){
        case ACSpattern:{//repeat pattern
            //CCP PPRRR
            u8 repeat = (byte & 0b111)+1;
            u8 pixels = ((byte>>3) & 0b111)+2;

            int rInd = *pInd-pixels;
            u8 iterations = repeat*pixels;
            for(int i = 0; i < iterations; i++){
                surface[(*pInd)++] = surface[rInd++];
            }
        break;}

        case ACSmirror:{//Mirror
            //debemos leer para atrás y escribirlo hacia adelante
            int mInd = *pInd-1;
            u8 repeat = (byte & 0b111111)+2;
            for(int i = 0; i < repeat; i++){
                surface[(*pInd)++] = surface[mInd--];
            }
        break;}

        default:{//repetición simple, caso 10 u 11
            //obtener último indice
            u16 pixel = surface[*pInd-1];
            u8 repeat = (byte & 0b1111111)+1;
            for(int i = 0; i<repeat;i++){
                surface[(*pInd)++] = pixel;
            }
        break;}
    }
}
void importACS(const char* path, u16* surface, u16* pal){

    //usamos backup para leer el archivo en RAM
    u8* data = (u8*)backup;

    FILE* f = fopen(path, "rb");
    if(!f) return;

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if(size > sizeof(backup)){ fclose(f); return; }

    fread(data, 1, size, f);
    fclose(f);

    // ---------- byte 0: version y otros datos ----------
    if(data[0] != 1) return;//hardcodeado por ahora

    memset(surface, 0, 32768);

    int ind = 1;

    // ---------- byte 1: resolución ----------
    static const int resTable[16] = {
        0,4,8,16,24,32,48,64,96,128,192,256,320,512,1024,-1
    };

    u8 val = data[ind++];
    int resX = resTable[val >> 4];
    int resY = resTable[val & 0xF];

    if(resX == -1){ resX = (data[ind]<<8) | data[ind+1]; ind+=2; }
    if(resY == -1){ resY = (data[ind]<<8) | data[ind+1]; ind+=2; }

    if(resX > 128 || resY > 128) return;

    surfaceXres = log2Lut[resX];
    surfaceYres = log2Lut[resY];

    u32 imgRes = (u32)resX * (u32)resY;

    // ---------- byte 2: bpp + colorMode ----------
    val = data[ind++];
    u8 bpp       = val >> 6;
    paletteBpp   = 1 << (bpp & 0b11);
    u8 colorMode = (val >> 3) & 0b111;
    if(colorMode > ACStotalModes) return;

    // ---------- colorCount ----------
    int colorCount = data[ind++];

    //  MODO INDEXADO
    if(colorCount > 0){

        // --- leer paleta ---
        switch(colorMode){

            case ACScolModeARGB1555:
                for(int i = 0; i <= colorCount; i++){
                    pal[i] = ((u16)data[ind] << 8) | data[ind+1];
                    ind += 2;
                }
            break;

            case ACScolModeARGB8888:
                for(int i = 0; i <= colorCount; i++){
                    u8 a = data[ind++] >> 7;
                    u8 r = data[ind++] >> 3;
                    u8 g = data[ind++] >> 3;
                    u8 b = data[ind++] >> 3;
                    pal[i] = (u16)((a<<15)|(r<<10)|(g<<5)|b);
                }
            break;

            case ACScolModeGrayScale4:
                for(int i = 0; i <= colorCount; i++){
                    u8 v  = data[ind++];
                    u8 hi = (v & 0xF0) >> 3;
                    u8 lo = (v & 0x0F) << 1;
                    pal[i++] = 0x8000 | (hi<<10) | (hi<<5) | hi;
                    if(i >= colorCount) break;
                    pal[i]   = 0x8000 | (lo<<10) | (lo<<5) | lo;
                }
            break;

            case ACScolModeGrayScale8:
                for(int i = 0; i <= colorCount; i++){
                    u8 v = data[ind++] >> 3;
                    pal[i] = 0x8000 | (u16)((v<<10)|(v<<5)|v);
                }
            break;

            case ACScolModeRGB888:
                for(int i = 0; i <= colorCount; i++){
                    u8 r = data[ind++] >> 3;
                    u8 g = data[ind++] >> 3;
                    u8 b = data[ind++] >> 3;
                    pal[i] = 0x8000 | (u16)((r<<10)|(g<<5)|b);
                }
            break;
        }

        // modo oculto solo-paleta
        if(resX == 0 || resY == 0) return;

        // --- cabecera de control ---
        int ByteCtrlCount  = ((int)data[ind]<<8) | data[ind+1]; ind+=2;
        int CommandsCount  = ((int)data[ind]<<8) | data[ind+1]; ind+=2;

        const u8* ctrlBase = data + ind;           // puntero fijo al bloque de control
        const u8* cmdBase  = data + ind + ByteCtrlCount; // puntero fijo al bloque de comandos
        ind += ByteCtrlCount + CommandsCount;       // ind apunta ahora a los datos de píxel

        // punteros de lectura independientes — evita recalcular offsets cada vez
        const u8* pixPtr  = data + ind;
        const u8* cmdPtr  = cmdBase;
        int ctrlPos = 0;
        int pInd    = 0;
        u8  part    = 0;

        //  HOT LOOP — bit de control desempaquetado de a 8 bits
        //  para reducir las divisiones/módulos por iteración.
        #define CTRL_BIT() \
            (( ctrlBase[ctrlPos >> 3] >> (7 - (ctrlPos & 7)) ) & 1)

        switch(bpp){

            // -------- 8 BPP --------
            case 3:
                while(pInd < (int)imgRes){
                    // desempaquetar el byte de control actual completo
                    u8 ctrl = ctrlBase[ctrlPos >> 3];
                    int startBit = ctrlPos & 7;
                    int bitsLeft = 8 - startBit;
                    int pixLeft  = (int)imgRes - pInd;
                    int n = bitsLeft < pixLeft ? bitsLeft : pixLeft;

                    u8 mask = 0x80 >> startBit;
                    for(int b = 0; b < n; b++, mask >>= 1){
                        if(ctrl & mask){
                            readCommand8(*cmdPtr++, &pInd, surface);
                        } else {
                            surface[pInd++] = *pixPtr++;
                        }
                    }
                    ctrlPos += n;
                }
            break;

            // -------- 4 BPP --------
            case 2:
                while(pInd < (int)imgRes){
                    u8 ctrl = ctrlBase[ctrlPos >> 3];
                    int startBit = ctrlPos & 7;
                    int bitsLeft = 8 - startBit;
                    int pixLeft  = (int)imgRes - pInd;
                    int n = bitsLeft < pixLeft ? bitsLeft : pixLeft;

                    u8 mask = 0x80 >> startBit;
                    for(int b = 0; b < n; b++, mask >>= 1){
                        if(ctrl & mask){
                            readCommand8(*cmdPtr++, &pInd, surface);
                        } else {
                            u8 raw = *pixPtr;
                            u8 index;
                            if(part == 0){ index = raw >> 4;   part = 1; }
                            else         { index = raw & 0x0F; pixPtr++; part = 0; }
                            surface[pInd++] = index;
                        }
                    }
                    ctrlPos += n;
                }
            break;

            // -------- 2 BPP --------
            case 1:
                while(pInd < (int)imgRes){
                    u8 ctrl = ctrlBase[ctrlPos >> 3];
                    int startBit = ctrlPos & 7;
                    int bitsLeft = 8 - startBit;
                    int pixLeft  = (int)imgRes - pInd;
                    int n = bitsLeft < pixLeft ? bitsLeft : pixLeft;

                    u8 mask = 0x80 >> startBit;
                    for(int b = 0; b < n; b++, mask >>= 1){
                        if(ctrl & mask){
                            readCommand8(*cmdPtr++, &pInd, surface);
                        } else {
                            int shift = 6 - (part << 1);
                            u8 index = (*pixPtr >> shift) & 0b11;
                            surface[pInd++] = index;
                            part++;
                            if(shift == 0){ pixPtr++; part = 0; }
                        }
                    }
                    ctrlPos += n;
                }
            break;

            // -------- 1 BPP --------
            case 0:
                while(pInd < (int)imgRes){
                    u8 ctrl = ctrlBase[ctrlPos >> 3];
                    int startBit = ctrlPos & 7;
                    int bitsLeft = 8 - startBit;
                    int pixLeft  = (int)imgRes - pInd;
                    int n = bitsLeft < pixLeft ? bitsLeft : pixLeft;

                    u8 mask = 0x80 >> startBit;
                    for(int b = 0; b < n; b++, mask >>= 1){
                        if(ctrl & mask){
                            readCommand8(*cmdPtr++, &pInd, surface);
                        } else {
                            int shift = 7 - part;
                            u8 index = (*pixPtr >> shift) & 1;
                            surface[pInd++] = index;
                            part++;
                            if(shift == 0){ pixPtr++; part = 0; }
                        }
                    }
                    ctrlPos += n;
                }
            break;
        }

        #undef CTRL_BIT

    //  MODO DIRECTO
    } else {
        paletteBpp = 16;
        int pInd = 0;

        switch(colorMode){

            case ACScolModeARGB1555:
                while(pInd < (int)imgRes){
                    u8 hi = data[ind++];
                    u8 lo = data[ind++];
                    if(hi < 0x80){
                        if((hi | lo) != 0){
                            readCommand7(hi, &pInd, surface);
                            ind--;
                            continue;
                        } else {
                            surface[pInd++] = 0;
                            continue;
                        }
                    }
                    surface[pInd++] = ((u16)hi << 8) | lo;
                }
            break;

            case ACScolModeARGB8888:
                for(int i = 0; i < (int)imgRes; i++){
                    u8 a = data[ind++] >> 7;
                    u8 r = data[ind++] >> 3;
                    u8 g = data[ind++] >> 3;
                    u8 b = data[ind++] >> 3;
                    if(a == 0){
                        if((r | g | b) != 0){
                            readCommand7(r, &i, surface);
                            ind -= 2;
                            continue;
                        } else {
                            surface[i] = 0;
                            continue;
                        }
                    }
                    surface[i] = (u16)((a<<15)|(r<<10)|(g<<5)|b);
                }
            break;

            case ACScolModeGrayScale4:
                for(int i = 0; i < 16; i++){
                    pal[i] = 0x8000 | (u16)((i<<11)|(1<<6)|(1<<1));
                }
            break;

            case ACScolModeGrayScale8:{
                for(int i = 0; i < 32; i++){
                    pal[i] = 0x8000 | (u16)((i<<10)|(i<<5)|i);
                }

                int ByteCtrlCount = ((int)data[ind]<<8) | data[ind+1]; ind+=2;
                int CommandsCount = ((int)data[ind]<<8) | data[ind+1]; ind+=2;

                const u8* ctrlBase = data + ind;
                const u8* cmdPtr   = data + ind + ByteCtrlCount;
                const u8* pixPtr   = data + ind + ByteCtrlCount + CommandsCount;
                int ctrlPos = 0;

                while(pInd < (int)imgRes){
                    u8 ctrl = ctrlBase[ctrlPos >> 3];
                    int startBit = ctrlPos & 7;
                    int bitsLeft = 8 - startBit;
                    int pixLeft  = (int)imgRes - pInd;
                    int n = bitsLeft < pixLeft ? bitsLeft : pixLeft;

                    u8 mask = 0x80 >> startBit;
                    for(int b = 0; b < n; b++, mask >>= 1){
                        if(ctrl & mask){
                            readCommand8(*cmdPtr++, &pInd, surface);
                        } else {
                            surface[pInd++] = *pixPtr++ >> 3;
                        }
                    }
                    ctrlPos += n;
                }
            break;}

            case ACScolModeRGB888:
            break;
        }
    }
}


void exportACS(const char* path, u16* surface, u16* pal){
    //agregar los otros comandos de exportación

    // revisar si la imagen es válida (solo aplica a esta app)
    if(surfaceXres < 2 || surfaceXres >= 8){ return; }
    if(surfaceYres < 2 || surfaceYres >= 8){ return; }
    u8* data = (u8*)backup;

    // -------------------------------------------------------------------
    // ENCABEZADO (3 bytes + 1 de config de paleta)
    // -------------------------------------------------------------------
    u8 ver         = 0;//versión 0
    u8 compression = 1;//usa compresión
    u8 fType       = 0;//imagen

    // Byte 0
    data[0] = (ver << 4) | (fType << 2) | (compression);

    // resoluciones ACS (reducido específicamente para esta app)
    u8 resTable[8] = {15,15,1,2,3,5,7,9};
    data[1] = (resTable[surfaceXres] << 4) | (resTable[surfaceYres]);
    data[2] = 0;// byte configuraciones color/bpp
    data[3] = 0;// byte 3 = cantidad de colores de paleta

    // índice del primer byte libre después del header
    int ind = 4;

    // -------------------------------------------------------------------
    // CONTADOR DE COLORES
    // usamos data[4 ... 32771] como tabla de colores (32768 bytes)
    // -------------------------------------------------------------------
    u8* table = &data[4];

    int totalPixels = (1 << surfaceXres) * (1 << surfaceYres);
    int unique = 0;
    int gray   = 1;
    int maxCol = 0;

    // limpiar la tabla de colores sin desbordar
    memset(table, 0, 32768);
    for(int i = 0; i < totalPixels; i++){
        u16 px = surface[i] & 0x7FFF; // ignorar bit alpha ARGB1555

        if(!table[px]){
            table[px] = 1;
            unique++;

            if(px > maxCol) maxCol = px;

            if(gray){
                int r = (px >> 10) & 0x1F;
                int g = (px >>  5) & 0x1F;
                int b =  px        & 0x1F;

                if(!(r == g && g == b)){
                    gray = 0;
                }
            }
        }
    }

    // -------------------------------------------------------------------
    // CONFIG. COLOR Y BPP
    // -------------------------------------------------------------------
    u8 colorConfig = gray ? ACScolModeGrayScale8 : ACScolModeARGB1555;
    u8 bpp = 0; // 1bpp, 2bpp, 4bpp, 8bpp
    if(paletteBpp == 16){
        maxCol = unique;
    }

    if(maxCol < 4){
        bpp = 1;   // 2bpp
    }else if(maxCol < 16){
        bpp = 2;   // 4bpp
    }else if(maxCol < 256){
        bpp = 3;   // 8bpp
    }

    data[2] = (bpp << 6) | (colorConfig << 3);
    //los otros tres bits están reservados

    // BYTE 3: cantidad de colores en paleta
    u8 colorCount = 0;
    if(maxCol > 255){
        // modo directo → no usa paleta
        colorCount = 0;
    }else{
        colorCount = maxCol;
    }
    data[3] = colorCount;

    // PALETA (este editor de pixel art solo exporta a dos formatos de manera nativa)
    if(colorCount > 0){
        if(colorConfig == ACScolModeARGB1555){
            for(int i = 0; i <= colorCount; i++){
                u16 col = pal[i];
                data[ind++] = col >> 8;
                data[ind++] = col & 0xFF;
            }
        }else{
            // Grayscale8
            for(int i = 0; i <= colorCount; i++){
                u8 g = (pal[i] & 0b11111) << 3;
                data[ind++] = g;
            }
        }

    }

    // -------------------------------------------------------------------
    // ESCRITURA DE PÍXELES
    // -------------------------------------------------------------------
    if(colorCount == 0){
        printf("\nUsing direct mode");
        // ====================== MODO DIRECTO ============================
        int iPix = 0;
        u16 lastColor = 0x81;//color inválido en ARGB1555
        const int MAXPAT = 9;
        const int MINPAT = 2; 
        printf("\nLooking for patterns, \nthis will take a time");
        while(iPix < totalPixels){
            u16 curr = surface[iPix];

            // 2) repetición del color (implementado)
            if(curr == lastColor){//comprueba hacia atrás porque el comando copia color
                int run = 1;//un pixel repetido
                while(iPix + run < totalPixels && run < 64){
                    if(surface[iPix+run] == lastColor) run++;
                    else break;
                }
                u8 cmd = (0b01000000 | ((run-1) & 0b111111));//nunca debería ser mayor a 63
                data[ind++] = cmd;

                iPix += run;//sumarle al indice de pixeles
                continue;
            }
            // 3) pixel directo ARGB1555
            data[ind++] = curr >> 8;
            data[ind++] = curr & 0xFF;

            lastColor = curr;
            iPix++;
        }
    }else{
        // ====================== MODO INDEXADO ===========================
        u8* cmdBuf = (u8*)stack;
        u8* pixBuf = (u8*)stack+8192;
        u8* ctrlBuf= (u8*)stack+24576;

        int cmdInd  = 0;
        int ctrlInd = 0;
        int pixInd  = 0;

        int  ctrlBit     = 0;
        u8   currentCtrl = 0;

        int  iPix        = 0;
        const int MAXPAT = 9;
        const int MINPAT = 2;
        const int MINMIRROR = 2;
        const int MAXMIRROR = 65;
        u16 lastRaw   = 0;
        u8  lastIndex = 0;

        while(iPix < totalPixels){

            // cerrar byte-control si está lleno
            if(ctrlBit == 8){
                ctrlBuf[ctrlInd++] = currentCtrl;
                currentCtrl = 0;
                ctrlBit = 0;
            }

            u8 index = surface[iPix]; // surface ya indexada

            int bestSize   = 0;   // tamaño del patrón normal
            int bestRepeat = 0;   // repeticiones encontradas
            int bestMirror = 0;   // tamaño del mirror (independiente)
            int mode       = 0;   // 1 = pattern, 2 = mirror, 0 = nada

            int maxHist = iPix;

            //-----------------------------------------------------
            // 1) DETECCIÓN DE PATRONES NORMALES + REPEATS
            //-----------------------------------------------------
            for(int size = MAXPAT; size >= MINPAT; size--)
            {
                if(size > maxHist) continue;
                if(iPix + size > totalPixels) continue;

                if(!isEqual(&surface[iPix - size], &surface[iPix], size)) continue;

                int rep = 1;
                while(rep < 8){
                    int startB = iPix + rep * size;
                    if(startB + size > totalPixels) break;
                    if(!isEqual(&surface[iPix], &surface[startB], size)) break;
                    rep++;
                }

                bestSize   = size;
                bestRepeat = rep;
                break;
            }

            //-----------------------------------------------------
            // 2) DETECCIÓN DE MIRROR
            //-----------------------------------------------------
            for(int size = MAXMIRROR; size >= MINMIRROR; size--)
            {
                if(size > maxHist) continue;
                if(iPix + size > totalPixels) continue;

                if(isMirror(&surface[iPix], &surface[iPix - size], size)){
                    bestMirror = size;
                    break;
                }
            }

            //-----------------------------------------------------
            // 3) DECISIÓN
            //-----------------------------------------------------
            int savingPattern = (bestSize > 0) ? (bestSize * bestRepeat) - 1 : 0;
            int savingMirror  = (bestMirror > 0) ? bestMirror - 1 : 0;

            if(savingPattern > 1 && savingPattern >= savingMirror) mode = 1;
            else if(savingMirror > 1) mode = 2;

            // --- después de calcular savingPattern, savingMirror ---

            // calcular también ahorro de repetición simple
            int run = 0;
            if(iPix > 0 && index == lastIndex){
                run = 1;
                while(iPix + run < totalPixels && run < 128){
                    if((u8)surface[iPix+run] == lastIndex) run++;
                    else break;
                }
            }
            int savingRun = (run > 0) ? run - 1 : 0;

            // decidir modo final incluyendo run
            if(savingPattern > 1 && savingPattern >= savingMirror && savingPattern >= savingRun) mode = 1;
            else if(savingMirror > 1 && savingMirror >= savingRun) mode = 2;
            else if(run > 0) mode = 3; // repetición simple
            // else mode = 0 → pixel crudo

            if(mode == 1){
                currentCtrl |= (1 << (7 - ctrlBit));
                u8 cmd = (ACSpattern << 6) | ((bestSize - 2) << 3) | (bestRepeat - 1);
                cmdBuf[cmdInd++] = cmd;
                iPix += bestSize * bestRepeat;
                lastIndex = (u8)surface[iPix - 1];
                ctrlBit++; continue;
            }
            else if(mode == 2){
                currentCtrl |= (1 << (7 - ctrlBit));
                u8 cmd = (ACSmirror << 6) | ((bestMirror - 2) & 0b111111);
                cmdBuf[cmdInd++] = cmd;
                iPix += bestMirror;
                lastIndex = (u8)surface[iPix - 1];
                ctrlBit++; continue;
            }
            else if(mode == 3){
                currentCtrl |= (1 << (7 - ctrlBit));
                u8 cmd = 0b10000000 | (run - 1);
                cmdBuf[cmdInd++] = cmd;
                iPix += run;
                ctrlBit++; continue;
            }
            // 3) pixel crudo
            // bit de control = 0
            pixBuf[pixInd++] = index;

            lastRaw   = index;
            lastIndex = index;
            iPix++;
            ctrlBit++;
        }

        // guardar último byte-control si quedó incompleto
        if(ctrlBit > 0){
            ctrlBuf[ctrlInd++] = currentCtrl;
        }

        // header de conteos
        data[ind++] = (ctrlInd >> 8)  & 0xFF;
        data[ind++] = (ctrlInd      ) & 0xFF;

        data[ind++] = (cmdInd  >> 8)  & 0xFF;
        data[ind++] = (cmdInd       ) & 0xFF;
        //el lector no pide cantidad de pixeles

        // 1) byte-controls
        for(int k = 0; k < ctrlInd; k++){
            data[ind++] = ctrlBuf[k];
        }
        printf("\n%d byte controls",ctrlInd);

        // 2) comandos
        for(int k = 0; k < cmdInd; k++){
            data[ind++] = cmdBuf[k];
        }
        printf("\n%d Commands",cmdInd);

        // 3) pixeles en index
        switch(bpp){

            case 0: {//1bpp
                int bitPos = 7;
                u8 byte = 0;

                for(int k = 0; k < pixInd; k++){
                    byte |= (pixBuf[k] & 1) << bitPos;
                    if(--bitPos < 0){
                        data[ind++] = byte;
                        bitPos = 7;
                        byte = 0;
                    }
                }
                if(bitPos != 7)
                    data[ind++] = byte;
            } break;

            case 1: {//2bpp
                int bitPos = 6; // 2 bits por pixel
                u8 byte = 0;

                for(int k = 0; k < pixInd; k++){
                    byte |= (pixBuf[k] & 3) << bitPos;
                    bitPos -= 2;

                    if(bitPos < 0){
                        data[ind++] = byte;
                        bitPos = 6;
                        byte = 0;
                    }
                }
                if(bitPos != 6)
                    data[ind++] = byte;
            } break;

            case 2: {//4bpp
                int high = 1;
                u8 byte = 0;

                for(int k = 0; k < pixInd; k++){
                    if(high){
                        byte = (pixBuf[k] & 0xF) << 4;
                        high = 0;
                    } else {
                        byte |= (pixBuf[k] & 0xF);
                        data[ind++] = byte;//escribir byte
                        high = 1;
                        byte = 0;
                    }
                }
                if(!high)
                    data[ind++] = byte;//terminar de escribir byte
            } break;

            case 3: // 8bpp
                for(int k = 0; k < pixInd; k++){
                    data[ind++] = pixBuf[k];
                }
            break;
        }
        printf("\n%d indexed bytes",pixInd);
    }
    
    int finalSize = ind;
    printf("\nProcess finished\n%d bytes.",finalSize);
    FILE* f = fopen(path, "wb");
    if(!f){
        return;
    }

    fwrite(data, 1, finalSize, f);
    fclose(f);
}