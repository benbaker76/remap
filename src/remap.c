#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#include "convert.h"
#include "diff.h"
#include "palette.h"
#include "lodepng.h"

const char* get_filename_ext(const char* filename) {
    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename) {
        return "";
    }
    return dot + 1;
}

int perfectHashRgbcolor(const void* key) {
    rgbcolor* rgb = (rgbcolor*) key;
	unsigned char R = (unsigned char) rgb->R;
    unsigned char G = (unsigned char) rgb->G;
    unsigned char B = (unsigned char) rgb->B;
	return ((R & 0xff) << 16) + ((G & 0xff) << 8) + (B & 0xff);
}

int compareRgbcolor(const void* a, const void* b) {
    rgbcolor* rgb1 = (rgbcolor*) a;
    rgbcolor* rgb2 = (rgbcolor*) b;
    return perfectHashRgbcolor(rgb1) - perfectHashRgbcolor(rgb2);
}

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s inputFilename paletteFilename outputFilename\n", argv[0]);
        return EXIT_FAILURE;
    }
    char* inputFilename = argv[1];
	if (access(inputFilename, F_OK) == -1) {
		fprintf(stderr, "%s cannot be found\n", inputFilename);
		return EXIT_FAILURE;
	}
    char* paletteFilename = argv[2];
	if (access(paletteFilename, F_OK) == -1) {
		fprintf(stderr, "%s cannot be found\n", paletteFilename);
		return EXIT_FAILURE;
	}
    char* outputFilename = argv[3];
    const char* paletteFileExtension = get_filename_ext(paletteFilename);
	const char* outputFileExtension = get_filename_ext(outputFilename);

    int inputWidth, inputHeight, inputFormat;
    unsigned char* inputImage = stbi_load(inputFilename, &inputWidth, &inputHeight, &inputFormat, 0);

    unsigned char* paletteImage = NULL;
    int paletteWidth, paletteHeight, paletteFormat;
    Color* colorPalette = NULL;
    int paletteCount = 0, transparentIndex = -1;
	if (strcmp(paletteFileExtension, "act") == 0) {
		read_palette(paletteFilename, &colorPalette, &paletteCount, &transparentIndex);
	} else if (strcmp(paletteFileExtension, "pal") == 0) {
		read_palette(paletteFilename, &colorPalette, &paletteCount, &transparentIndex);
	} else if (strcmp(paletteFileExtension, "gpl") == 0) {
		read_palette(paletteFilename, &colorPalette, &paletteCount, &transparentIndex);
	} else if (strcmp(paletteFileExtension, "txt") == 0) {
		read_palette(paletteFilename, &colorPalette, &paletteCount, &transparentIndex);
	} else if (strcmp(paletteFileExtension, "png") == 0) {
		paletteImage = stbi_load(paletteFilename, &paletteWidth, &paletteHeight, &paletteFormat, 0);
	} else {
		fprintf(stderr, "The file extension \"%s\" is not supported. Please use one of the following instead: act, pal, gpl, txt, png\n", paletteFilename);
	}

    rgbcolor* uniqueRgbPaletteColors = NULL;
    int countOfUniquePaletteColors = 0;

    if (colorPalette != NULL)
    {
        paletteWidth = paletteCount;
        paletteHeight = 1;
        paletteFormat = 3;
        countOfUniquePaletteColors = paletteCount;

        uniqueRgbPaletteColors = (rgbcolor*) malloc(paletteCount * sizeof(rgbcolor));
        for (int i = 0; i < paletteCount; i++) {
            float R = (float) colorPalette[i].R;
            float G = (float) colorPalette[i].G;
            float B = (float) colorPalette[i].B;
            rgbcolor_init(uniqueRgbPaletteColors + i, R, G, B);
        }
    }
    else
    {
        rgbcolor* rgbPaletteColors = (rgbcolor*) malloc(paletteWidth * paletteHeight * sizeof(rgbcolor));
        uniqueRgbPaletteColors = (rgbcolor*) malloc(paletteWidth * paletteHeight * sizeof(rgbcolor));
        for (int i = 0; i < paletteWidth * paletteHeight; i++) {
            float R = (float) paletteImage[i * paletteFormat + 0];
            float G = (float) paletteImage[i * paletteFormat + 1];
            float B = (float) paletteImage[i * paletteFormat + 2];

            if (paletteFormat == 4) {
                float A = (float) paletteImage[i * paletteFormat + 3] / 255.0f;
                rgbacolor rgba;
                rgbacolor_init(&rgba, R, G, B, A);
                rgbcolor rgb;
                rgba_to_rgb(&rgba, &rgb, NULL);
                R = rgb.R;
                G = rgb.G;
                B = rgb.B;
            }
            
            rgbcolor_init(rgbPaletteColors + i, R, G, B);
        }

        qsort(rgbPaletteColors, paletteWidth * paletteHeight, sizeof(rgbcolor), compareRgbcolor);

        rgbcolor uniqueRgbPaletteColors[paletteWidth * paletteHeight];
        int countOfUniquePaletteColors = 0;
        for (int i = 0; i < paletteWidth * paletteHeight; i++) {
            rgbcolor* rgbPaletteColor = rgbPaletteColors + i;
            float R = rgbPaletteColor->R;
            float G = rgbPaletteColor->G;
            float B = rgbPaletteColor->B;
            if (i == 0) {
                rgbcolor_init(uniqueRgbPaletteColors + countOfUniquePaletteColors++, R, G, B);
            } else {
                rgbcolor* lastUniqueRgbPaletteColor = uniqueRgbPaletteColors + countOfUniquePaletteColors - 1;
                if (R != lastUniqueRgbPaletteColor->R || G != lastUniqueRgbPaletteColor->G || B != lastUniqueRgbPaletteColor->B) {
                    rgbcolor_init(uniqueRgbPaletteColors + countOfUniquePaletteColors++, R, G, B);
                }
            }
        }

        free(rgbPaletteColors);
    }

    labcolor uniqueLabPaletteColors[countOfUniquePaletteColors];
    for (int i = 0; i < countOfUniquePaletteColors; i++) {
        rgb_to_lab(uniqueRgbPaletteColors + i, uniqueLabPaletteColors + i);
    }
    
    LodePNGState state;
    lodepng_state_init(&state);

    for (int i = 0; i < countOfUniquePaletteColors; i++) {
        lodepng_palette_add(&state.info_png.color, uniqueRgbPaletteColors[i].R, uniqueRgbPaletteColors[i].G, uniqueRgbPaletteColors[i].B, 255);
        lodepng_palette_add(&state.info_raw, uniqueRgbPaletteColors[i].R, uniqueRgbPaletteColors[i].G, uniqueRgbPaletteColors[i].B, 255);
    }

    state.info_png.color.colortype = LCT_PALETTE;
    state.info_png.color.bitdepth = 4;
    state.info_raw.colortype = LCT_PALETTE;
    state.info_raw.bitdepth = 4;
    state.encoder.auto_convert = 0;

    int imageSize = (inputWidth * inputHeight * 4 + 7) / 8;
    unsigned char* image = (unsigned char*)malloc(imageSize);
    memset(image, 0, imageSize);
    if (!image) {
        perror("Failed to allocate memory for image");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < inputWidth * inputHeight; i++) {
        size_t byte_index = i / 2;
        int byte_half = i % 2 == 1;
        float R = (float) inputImage[i * inputFormat + 0];
        float G = (float) inputImage[i * inputFormat + 1];
        float B = (float) inputImage[i * inputFormat + 2];

        rgbcolor rgb;
        rgbcolor_init(&rgb, R, G, B);

        int colorIndex = 0;
        labcolor lab;        
        rgb_to_lab(&rgb, &lab);
        float minDiff = 100.0f;

        for (int i = 0; i < countOfUniquePaletteColors; i++) {
            float diff = ciede2000(&lab, uniqueLabPaletteColors + i);
            if (diff < minDiff) {
                minDiff = diff;
                colorIndex = i;
            }
        }

        image[byte_index] |= (unsigned char)(colorIndex << (byte_half ? 0 : 4));
    }

    unsigned char* buffer;
    size_t buffer_size;
    if (lodepng_encode(&buffer, &buffer_size, image, inputWidth, inputHeight, &state)) {
        fprintf(stderr, "Encoder error: %s\n", lodepng_error_text(state.error));
        free(image);
        lodepng_state_cleanup(&state);
        return EXIT_FAILURE;
    }

    if (lodepng_save_file(buffer, buffer_size, outputFilename)) {
        fprintf(stderr, "Error saving PNG file\n");
        free(image);
        free(buffer);
        lodepng_state_cleanup(&state);
        return EXIT_FAILURE;
    }
    
    free(image);
    free(buffer);
    lodepng_state_cleanup(&state);

    stbi_image_free(inputImage);
    stbi_image_free(paletteImage);

    return EXIT_SUCCESS;
}
