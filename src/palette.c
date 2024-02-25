/*
 * palette.c
 *
 *  Created on: Feb 24, 2024
 *      Author: bbaker
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "palette.h"

static unsigned char msPalHeader[] = { 'R', 'I', 'F', 'F' };
static unsigned char jascPalHeader[] = { 'J', 'A', 'S', 'C', '-', 'P', 'A', 'L' };
static unsigned char gimpPalHeader[] = { 'G', 'I', 'M', 'P', ' ', 'P', 'a', 'l', 'e', 't', 't', 'e' };
static unsigned char painNetPalHeader[] = { ';', ' ', 'P', 'a', 'i', 'n', 't', '.', 'N', 'E', 'T', ' ', 'P', 'a', 'l', 'e', 't', 't', 'e' };

#define SWAP_SHORT(x) ((unsigned short)(((x) << 8) | ((x) >> 8)))

int starts_with(const unsigned char* thisBytes, const unsigned char* thatBytes, int thisLength, int thatLength) {
    if (thatLength > thisLength) {
        return 0;
    }
    for (int i = 0; i < thatLength; i++) {
        if (thisBytes[i] != thatBytes[i]) {
            return 0;
        }
    }
    return 1;
}

void read_ms_pal(FILE* file, Color** colorPalette, int* paletteCount) {
    int fileLength;
    unsigned char riffType[4];
    unsigned char riffChunkSignature[4];
    unsigned char chunkSize[4];
    unsigned char paletteVersion[2];
    short palCount;

    fread(&fileLength, sizeof(int), 1, file);
    fileLength -= 16;
    fread(riffType, sizeof(unsigned char), 4, file);
    fread(riffChunkSignature, sizeof(unsigned char), 4, file);
    fread(chunkSize, sizeof(unsigned char), 4, file);
    fread(paletteVersion, sizeof(unsigned char), 2, file);
    fread(&palCount, sizeof(short), 1, file);

    *colorPalette = (Color*)malloc(palCount * sizeof(Color));
    *paletteCount = palCount;

    if (*colorPalette == NULL) {
        return;
    }

    for (int i = 0; i < palCount; i++) {
        unsigned char colorArray[4];
        fread(colorArray, sizeof(unsigned char), 4, file);
        (*colorPalette)[i].R = colorArray[0];
        (*colorPalette)[i].G = colorArray[1];
        (*colorPalette)[i].B = colorArray[2];
    }
}

void read_act_pal(FILE* file, Color **colorPalette, int* paletteCount, int *transparentIndex) {
    *colorPalette = (Color*)malloc(256 * sizeof(Color));

    if (*colorPalette == NULL) {
        return;
    }

    for (int i = 0; i < 256; i++) {
        if (fread(&(*colorPalette)[i].R, sizeof(unsigned char), 1, file) != 1 ||
            fread(&(*colorPalette)[i].G, sizeof(unsigned char), 1, file) != 1 ||
            fread(&(*colorPalette)[i].B, sizeof(unsigned char), 1, file) != 1) {
            free(*colorPalette);
            *colorPalette = NULL;
            return;
        }
    }

    long currentPosition = ftell(file);
    fseek(file, 0, SEEK_END);
    long endPosition = ftell(file);
    fseek(file, currentPosition, SEEK_SET);

    if (currentPosition == endPosition - 4) {
        short palCount, alphaIndex;
        if (fread(&palCount, sizeof(short), 1, file) != 1 ||
            fread(&alphaIndex, sizeof(short), 1, file) != 1) {
            free(*colorPalette);
            *colorPalette = NULL;
            return;
        }

        *colorPalette = (Color*)realloc(*colorPalette, SWAP_SHORT(palCount) * sizeof(Color));
        *paletteCount = SWAP_SHORT(palCount);
        *transparentIndex = SWAP_SHORT(alphaIndex);
    } else {
        *paletteCount = 256;
    }
}


void read_jasc_pal(FILE* file, Color** colorPalette, int* paletteCount) {
    char* tempString = NULL;
    char* versionString = NULL;

    tempString = (char*)malloc(256 * sizeof(char));
    versionString = (char*)malloc(256 * sizeof(char));

    fgets(tempString, 256, file);
    fgets(versionString, 256, file);
    fscanf(file, "%d", paletteCount);

    *colorPalette = (Color*)malloc(*paletteCount * sizeof(Color));

    if (*colorPalette == NULL) {
        return;
    }

    for (int i = 0; i < *paletteCount; i++) {
        char colorString[256];
        fgets(colorString, 256, file);
        if (colorString == NULL) {
            break;
        }
        int red, green, blue;
        sscanf(colorString, "%d %d %d", &red, &green, &blue);
        (*colorPalette)[i].R = red;
        (*colorPalette)[i].G = green;
        (*colorPalette)[i].B = blue;
    }

    free(tempString);
    free(versionString);
}

void read_gimp_pal(FILE* file, Color** colorPalette) {
    *colorPalette = (Color*)malloc(256 * sizeof(Color));

    if (*colorPalette == NULL) {
        return;
    }

    char lineString[256];

    while (1) {
        fgets(lineString, 256, file);
        if (lineString == NULL) {
            break;
        }
        if (strcmp(lineString, "") == 0 ||
            strncmp(lineString, "Name:", 5) == 0 ||
            strncmp(lineString, "Columns:", 8) == 0 ||
            strncmp(lineString, "#", 1) == 0) {
            continue;
        }
        char* colorArray[3];
        int palCount = 0;
        char* token = strtok(lineString, " \t");
        while (token != NULL) {
            colorArray[palCount++] = token;
            token = strtok(NULL, " \t");
        }
        if (palCount < 3) {
            continue;
        }
        int red = atoi(colorArray[0]);
        int green = atoi(colorArray[1]);
        int blue = atoi(colorArray[2]);
        *colorPalette = (Color*)realloc(*colorPalette, (palCount + 1) * sizeof(Color));
        (*colorPalette)[palCount].R = red;
        (*colorPalette)[palCount].G = green;
        (*colorPalette)[palCount].B = blue;
    }
}

void read_paintnet_pal(FILE* file, Color** colorPalette) {
    *colorPalette = (Color*)malloc(256 * sizeof(Color));

    if (*colorPalette == NULL) {
        return;
    }

    char lineString[256];
    int palCount = 0;

    while (1) {
        fgets(lineString, 256, file);
        if (lineString == NULL) {
            break;
        }
        if (strcmp(lineString, "") == 0 ||
            strncmp(lineString, ";", 1) == 0) {
            continue;
        }
        int result = 0;
        sscanf(lineString, "%x", &result);
        (*colorPalette)[palCount].R = (result >> 16) & 0xFF;
        (*colorPalette)[palCount].G = (result >> 8) & 0xFF;
        (*colorPalette)[palCount].B = result & 0xFF;

        palCount++;
    }

    *colorPalette = (Color*)realloc(*colorPalette, (palCount + 1) * sizeof(Color));
}

void read_palette(const char* fileName, Color** colorPalette, int* paletteCount, int* transparentIndex) {
    FILE* file = fopen(fileName, "rb");
    if (file == NULL) {
        return;
    }
    int maxMagicBytesLength = 0;
    unsigned char magicBytes[256];
    int bytesRead = fread(magicBytes, sizeof(unsigned char), 256, file);

    for (int i = 0; i < bytesRead; i++) {
        if (magicBytes[i] == '\0') {
            maxMagicBytesLength = i;
            break;
        }
    }

    for (int i = 0; i < bytesRead; i++) {
        if (starts_with(magicBytes, msPalHeader, bytesRead, 4)) {
            fseek(file, 4, SEEK_CUR);
            fseek(file, 4, SEEK_CUR);
            fseek(file, 4, SEEK_CUR);
            fseek(file, 2, SEEK_CUR);
            fread(paletteCount, sizeof(short), 1, file);
            read_ms_pal(file, colorPalette, paletteCount);
            break;
        } else if (starts_with(magicBytes, jascPalHeader, bytesRead, 8)) {
            fseek(file, 8, SEEK_CUR);
            fscanf(file, "%d", paletteCount);
            read_jasc_pal(file, colorPalette, paletteCount);
            break;
        } else if (starts_with(magicBytes, gimpPalHeader, bytesRead, 12)) {
            fseek(file, 12, SEEK_CUR);
            read_gimp_pal(file, colorPalette);
            break;
        } else if (starts_with(magicBytes, painNetPalHeader, bytesRead, 20)) {
            fseek(file, 20, SEEK_CUR);
            read_paintnet_pal(file, colorPalette);
            break;
        } else {
            fseek(file, 0, SEEK_SET);
            read_act_pal(file, colorPalette, paletteCount, transparentIndex);
            break;
        }
    }

    fclose(file);
}

void write_act_pal(const char *fileName, Color *colorPalette, int paletteCount, int transparentIndex) {
    FILE *file = fopen(fileName, "wb");
    if (file == NULL) {
        return;
    }

    for (int i = 0; i < 256; i++) {
        if (i < paletteCount) {
            fwrite(&colorPalette[i].R, sizeof(unsigned char), 1, file);
            fwrite(&colorPalette[i].G, sizeof(unsigned char), 1, file);
            fwrite(&colorPalette[i].B, sizeof(unsigned char), 1, file);
        } else {
            unsigned char zero = 0;
            fwrite(&zero, sizeof(unsigned char), 1, file);
            fwrite(&zero, sizeof(unsigned char), 1, file);
            fwrite(&zero, sizeof(unsigned char), 1, file);
        }
    }

    if (transparentIndex != -1 || paletteCount < 256) {
        unsigned char paletteCountHigh = (unsigned char)(paletteCount >> 8);
        unsigned char paletteCountLow = (unsigned char)paletteCount;
        fwrite(&paletteCountHigh, sizeof(unsigned char), 1, file);
        fwrite(&paletteCountLow, sizeof(unsigned char), 1, file);

        unsigned char transparentIndexHigh = (transparentIndex == -1) ? 0xFF : (unsigned char)(transparentIndex >> 8);
        unsigned char transparentIndexLow = (transparentIndex == -1) ? 0xFF : (unsigned char)transparentIndex;
        fwrite(&transparentIndexHigh, sizeof(unsigned char), 1, file);
        fwrite(&transparentIndexLow, sizeof(unsigned char), 1, file);
    }

    fclose(file);
}

void write_ms_pal(const char* fileName, Color* colorPalette, int paletteCount) {
    FILE* file = fopen(fileName, "wb");
    if (file == NULL) {
        return;
    }
    unsigned char riffSig[4] = { 'R', 'I', 'F', 'F' };
    unsigned char riffType[4] = { 'P', 'A', 'L', ' ' };
    unsigned char riffChunkSig[4] = { 'd', 'a', 't', 'a' };
    unsigned char chunkSize[4];
    unsigned char palVer[2] = { 0x00, 0x03 };
    short palCount = paletteCount;

    fwrite(riffSig, sizeof(unsigned char), 4, file);
    fwrite(chunkSize, sizeof(unsigned char), 4, file);
    fwrite(riffType, sizeof(unsigned char), 4, file);
    fwrite(riffChunkSig, sizeof(unsigned char), 4, file);
    fwrite(chunkSize, sizeof(unsigned char), 4, file);
    fwrite(palVer, sizeof(unsigned char), 2, file);
    fwrite(&palCount, sizeof(short), 1, file);

    for (int i = 0; i < palCount; i++) {
        fwrite(&colorPalette[i].R, sizeof(unsigned char), 1, file);
        fwrite(&colorPalette[i].G, sizeof(unsigned char), 1, file);
        fwrite(&colorPalette[i].B, sizeof(unsigned char), 1, file);
        fwrite("\0", sizeof(unsigned char), 1, file);
    }

    fclose(file);
}

void write_jasc_pal(const char* fileName, Color* colorPalette, int paletteCount) {
    FILE* file = fopen(fileName, "wb");
    if (file == NULL) {
        return;
    }
    fprintf(file, "JASC-PAL\n");
    fprintf(file, "0100\n");
    fprintf(file, "%d\n", paletteCount);
    for (int i = 0; i < paletteCount; i++) {
        fprintf(file, "%d %d %d\n", colorPalette[i].R, colorPalette[i].G, colorPalette[i].B);
    }

    fclose(file);
}

void write_gimp_pal(const char* fileName, Color* colorPalette, int paletteCount) {
    FILE* file = fopen(fileName, "wb");
    if (file == NULL) {
        return;
    }
    fprintf(file, "GIMP Palette\n");
    fprintf(file, "Name: %s\n", fileName);
    fprintf(file, "Columns: 0\n");
    fprintf(file, "#\n");
    for (int i = 0; i < paletteCount; i++) {
        fprintf(file, "%3d %3d %3d\tUntitled\n", colorPalette[i].R, colorPalette[i].G, colorPalette[i].B);
    }

    fclose(file);
}

void write_paintnet_pal(const char* fileName, Color* colorPalette, int paletteCount) {
    FILE* file = fopen(fileName, "wb");
    if (file == NULL) {
        return;
    }
    fprintf(file, "; Paint.NET Palette\n");
    fprintf(file, "; %s\n", fileName);
    for (int i = 0; i < paletteCount; i++) {
        fprintf(file, "%08X\n", (colorPalette[i].R << 16) | (colorPalette[i].G << 8) | colorPalette[i].B);
    }

    fclose(file);
}

void write_palette(const char* fileName, Color* colorPalette, int paletteCount, int transparentIndex, PaletteFormat paletteFormat) {
    switch (paletteFormat) {
        case Act:
            write_act_pal(fileName, colorPalette, paletteCount, transparentIndex);
            break;
        case MSPal:
            write_ms_pal(fileName, colorPalette, paletteCount);
            break;
        case JASC:
            write_jasc_pal(fileName, colorPalette, paletteCount);
            break;
        case GIMP:
            write_gimp_pal(fileName, colorPalette, paletteCount);
            break;
        case PaintNET:
            write_paintnet_pal(fileName, colorPalette, paletteCount);
            break;
    }
}