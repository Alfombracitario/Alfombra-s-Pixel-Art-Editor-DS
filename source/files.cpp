#include "files.h"

extern bool usesPages;
extern int paletteBpp;
extern bool nesMode;
extern int surfaceXres;
extern int surfaceYres;
extern bool preview;
extern u16* bgPreviewGfx;
extern int bgPreview;
extern void drawNesPalette();
extern u32 kDown;
extern u16 stack[16384];

char fname[MAX_TEXT_LENGTH];
DIR* currentDir = NULL;
struct dirent entryList[MAX_FILES];
int fileCount = 0;
u8 sortedIdx[MAX_FILES];
char path[257] = "/";
char format[6];
char currentFilePath[257];

int selector = 0;//selector para la consola
int selectorA = 0;//selector secundario

void saveFile(int format, char* path, u16* palette, u16* surface){
    switch(format){
        default:
            return;
        break;
        case formatDirectBMP:
            saveBMP(path, palette, surface);
        break;
        case format8bppBMP:
            saveBMP_indexed(path, palette, surface);
        break;
        case format4bppBMP:
            saveBMP_4bpp(path, palette, surface);
        break;
        case formatNES:
            exportNES(path, surface, 1<<surfaceYres);
        break;
        case formatGBC:
            exportGBC(path, surface, 1<<surfaceYres);
        break;
        case formatSNES4:
            exportSNES(path, surface, 1<<surfaceYres);
        break;
        case formatGBA4:
            exportGBA(path, surface, 1<<surfaceYres);
        break;
        case formatPCX:
            exportPCX(path, surface, palette, 1<<surfaceXres, 1<<surfaceYres);
        break;
        case formatPAL:
            exportPal(path, palette);
        break;
        case formatSNES8:
            exportSNES8bpp(path, surface, 1<<surfaceYres);
        break;
        case formatPal1555:
            exportPal1555(path, palette);
        break;
        case formatACS:
            exportACS(path, surface, palette);
        break;
        case formatPNG:
            png_export(path, surface, palette);
        break;
    }
}

void loadFile(int format, char* path, u16* palette, u16* surface){
    switch(format)
    {
        default:
            printf("\nNot supported!");
        break;
        case formatDirectBMP:
            loadBMP_direct(path, surface);
            paletteBpp = 16; usesPages = false;
        break;
        case format8bppBMP:
            loadBMP_indexed(path, palette, surface);
            paletteBpp = 8; usesPages = false;
        break;
        case format4bppBMP:
            loadBMP_4bpp(path, palette, surface);
            paletteBpp = 4; usesPages = false;
        break;
        case formatNES:
            paletteBpp = 2; nesMode = true; usesPages = true;
            importNES(path, surface);
            drawNesPalette();
        break;
        case formatGBC:
            paletteBpp = 2; usesPages = true;
            importGBC(path, surface);
        break;
        case formatSNES4:
            paletteBpp = 4; usesPages = true;
            importSNES(path, surface);
        break;
        case formatGBA4:
            paletteBpp = 4; usesPages = true;
            importGBA(path, surface);
        break;
        case formatPCX:
            paletteBpp = 8; usesPages = false;
            importPCX(path, surface, palette);
        break;
        case formatPAL:
            importPal(path, palette);
        break;
        case formatSNES8:
            usesPages = true;
            importSNES8bpp(path, surface);
        break;
        case formatPal1555:
            importPal1555(path, palette);
        break;
        case formatACS:
            usesPages = false;
            importACS(path, surface, palette);
        break;
        case formatPNG:
            usesPages = false;
            png_import(path, surface, palette);
        break;
    }
}

void previewFile(int format, const char* filename){
    int preBpp         = paletteBpp;
    int preSurfaceYres = surfaceYres;
    int preSurfaceXres = surfaceXres;

    // lanzar DMA fill en paralelo mientras la CPU prepara el path y carga el archivo
    dmaFillHalfWords(0, bgPreviewGfx, 128 * 128 * 2);

    char fullPath[257];
    u16 pal[256];
    snprintf(fullPath, sizeof(fullPath), "%s%s", path, filename);
    loadFile(format, fullPath, pal, stack);  // CPU ocupada aquí mientras DMA limpia

    //flushear para asegurarse de que la imagen se muestre bien.
    DC_FlushRange(stack, (1 << surfaceXres) * (1 << surfaceYres) * sizeof(u16));
    DC_FlushRange(pal, 256 * sizeof(u16));

    int maxExp = surfaceXres > surfaceYres ? surfaceXres : surfaceYres;
    bgSetScale(bgPreview, 296 >> (7 - maxExp), 296 >> (7 - maxExp));
    bgUpdate();

    int sw = 1 << surfaceXres;
    int sh = 1 << surfaceYres;

    if(paletteBpp == 16){
        for(int y = 0; y < sh; y++){
            u16* dst = bgPreviewGfx + y * 128;
            u16* src = stack       + y * sw;
            dmaCopy(src, dst, sw * 2);
        }
    } else {
        for(int y = 0; y < sh; y++){
            u16* dst = bgPreviewGfx + y * 128;
            u16* src = stack       + y * sw;
            for(int x = 0; x < sw; x++){
                dst[x] = pal[src[x]];
            }
        }
    }

    paletteBpp  = preBpp;
    surfaceYres = preSurfaceYres;
    surfaceXres = preSurfaceXres;
}

void buildCurrentFilePath(void) {
    currentFilePath[0] = '\0';
    strcat(currentFilePath, path);
    strcat(currentFilePath, fname);
    strcat(currentFilePath, format);
}

int enterFolder(int index) {
    if(index < 0 || index >= fileCount) return 0;
    if(entryList[sortedIdx[index]].d_type == DT_DIR) {
        char newPath[256];
        snprintf(newPath, sizeof(newPath), "%s%s/", path, entryList[sortedIdx[index]].d_name);
        strncpy(path, newPath, sizeof(path));

        // abrir nuevo directorio
        if(currentDir) closedir(currentDir);
        currentDir = opendir(path);
        selector = 0;
        return 1; // carpeta abierta
    }
    return 0; // no es carpeta
}
int goBack() {
    if(strcmp(path, "/") == 0) return 0; // ya en raíz

    // quitar última carpeta
    char* lastSlash = strrchr(path, '/');
    if(lastSlash != path) {
        *lastSlash = '\0';
        lastSlash = strrchr(path, '/');
        *(lastSlash+1) = '\0';
    } else {
        path[1] = '\0';
    }

    // abrir carpeta padre
    if(currentDir) closedir(currentDir);
    currentDir = opendir(path);
    selector = 0;
    return 1;
}
int compare_dirent(const void* a, const void* b) {
    u8 idxA = *(const u8*)a;
    u8 idxB = *(const u8*)b;
    return strcasecmp(entryList[idxA].d_name, entryList[idxB].d_name);
}
void listFiles() {
    if(!currentDir) return;

    fileCount = 0;
    struct dirent* ent;
    rewinddir(currentDir);

    while((ent = readdir(currentDir)) != NULL && fileCount < MAX_FILES) {
        entryList[fileCount] = *ent;
        sortedIdx[fileCount] = (u8)fileCount;
        fileCount++;
    }

    qsort(sortedIdx, fileCount, sizeof(u8), compare_dirent);

    int start = selector;
    if(start + SCREEN_LINES > fileCount) start = fileCount - SCREEN_LINES;
    if(start < 0) start = 0;

    for(int i = 0; i < SCREEN_LINES && (start + i) < fileCount; i++) {
        const char* name = entryList[sortedIdx[start + i]].d_name;
        bool isDir = (entryList[sortedIdx[start + i]].d_type == DT_DIR);
        char displayName[32];

        if(isDir) {
            int len = strlen(name);
            if(len > 27) {
                snprintf(displayName, sizeof(displayName), "%.27s.../", name);
            } else {
                snprintf(displayName, sizeof(displayName), "%s/", name);
            }
        } else {
            const char* dot = strrchr(name, '.');
            if(dot && strlen(name) > 27) {
                int extLen = strlen(dot);
                int keep = 27 - extLen - 3;
                if(keep < 1) keep = 1;
                snprintf(displayName, sizeof(displayName), "%.*s...%s", keep, name, dot);
            } else if(strlen(name) > 27) {
                snprintf(displayName, sizeof(displayName), "%.23s...", name);
            } else {
                strcpy(displayName, name);
            }
        }

        if(start + i == selector)
            printf("> %s\n", displayName);
        else
            printf("  %s\n", displayName);
    }

    if(preview && selector >= 0 && selector < fileCount) {
        const char* selName = entryList[sortedIdx[selector]].d_name;
        bool selIsDir = (entryList[sortedIdx[selector]].d_type == DT_DIR);

        if(!selIsDir) {
            const char* dot = strrchr(selName, '.');
            if(dot && strncasecmp(dot, format, strlen(format)) == 0) {
                previewFile(selectorA,selName);
            }
        }
    }
}

void OnKeyPressed(int key) {
if ( key < 0 ) return;
    switch(key) {
    case '\b':
        if (selector > 0 ) {
            selector--;
            fname[selector] = '\0';
        }
    break;
    case '\n':
    case '\r':
        if (selector < MAX_TEXT_LENGTH)
        {
            fname[selector++] = '\n';
            fname[selector] = '\0';           
        }
    default:
        if (selector < MAX_TEXT_LENGTH)
        {
            fname[selector++] = (char)key;
            fname[selector] = '\0';
        }
    }
}