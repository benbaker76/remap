/*
 * palette.h
 *
 *  Created on: Feb 24, 2024
 *      Author: bbaker
 */

#ifndef PALETTE_H
#define PALETTE_H

typedef struct {
    unsigned char R;
    unsigned char G;
    unsigned char B;
} Color;

typedef enum {
    Act,
    MSPal,
    JASC,
    GIMP,
    PaintNET
} PaletteFormat;

void read_palette(const char* fileName, Color** colorPalette, int* paletteCount, int* transparentIndex);
void write_palette(const char* fileName, Color* colorPalette, int paletteCount, int transparentIndex, PaletteFormat paletteFormat);

#endif /* PALETTE_H */
