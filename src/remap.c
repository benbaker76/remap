#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <getopt.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#include "convert.h"
#include "diff.h"
#include "palette.h"
#include "lodepng.h"
#include "libimagequant.h"

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
    static struct options {
        const char* inputFilename;
        const char* paletteFilename;
        const char* outputFilename;
        int rangeMin;
        int rangeMax;
        int bitDepth;
    } options;

    options = (struct options) {
        .inputFilename = NULL,
        .paletteFilename = NULL,
        .outputFilename = NULL,
        .rangeMin = 0,
        .rangeMax = -1,
        .bitDepth = 8
    };

    static struct option long_options[] = {
        {"range", required_argument, 0, 'r'},
        {"bits", required_argument, 0, 'b'},
        {0, 0, 0, 0}
    };

   const char *usage_str = 
        "Usage: %s\n"
        "  --range min-max\n"
        "  --bits 4|8 (default 8)\n"
        "  <inputFilename> <paletteFilename> <outputFilename>\n";

    int option;
    while ((option = getopt_long(argc, argv, "r:b:", long_options, NULL)) != -1) {
        switch (option) {
            case 'r':
                sscanf(optarg, "%d-%d", &options.rangeMin, &options.rangeMax);
                break;
            case 'b':
                options.bitDepth = atoi(optarg);
                break;
            default:
                fprintf(stderr, usage_str, argv[0], argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (argc - optind < 3) {
        fprintf(stderr, usage_str, argv[0]);
        return EXIT_FAILURE;
    }

    options.inputFilename = argv[optind];
    if (access(options.inputFilename, F_OK) == -1) {
        fprintf(stderr, "%s cannot be found\n", options.inputFilename);
        return EXIT_FAILURE;
    }

    options.paletteFilename = argv[optind + 1];
    if (access(options.paletteFilename, F_OK) == -1) {
        fprintf(stderr, "%s cannot be found\n", options.paletteFilename);
        return EXIT_FAILURE;
    }

    options.outputFilename = argv[optind + 2];
    const char* paletteFileExtension = get_filename_ext(options.paletteFilename);
    const char* outputFileExtension = get_filename_ext(options.outputFilename);

    int inputWidth, inputHeight, inputFormat;
    unsigned char* inputImage = stbi_load(options.inputFilename, &inputWidth, &inputHeight, &inputFormat, STBI_rgb_alpha);

    unsigned char* paletteImage = NULL;
    int paletteWidth, paletteHeight, paletteFormat;
    Color* colorPalette = NULL;
    int paletteCount = 0, transparentIndex = -1;
	if (strcmp(paletteFileExtension, "act") == 0) {
		read_palette(options.paletteFilename, &colorPalette, &paletteCount, &transparentIndex);
	} else if (strcmp(paletteFileExtension, "pal") == 0) {
		read_palette(options.paletteFilename, &colorPalette, &paletteCount, &transparentIndex);
	} else if (strcmp(paletteFileExtension, "gpl") == 0) {
		read_palette(options.paletteFilename, &colorPalette, &paletteCount, &transparentIndex);
	} else if (strcmp(paletteFileExtension, "txt") == 0) {
		read_palette(options.paletteFilename, &colorPalette, &paletteCount, &transparentIndex);
	} else if (strcmp(paletteFileExtension, "png") == 0) {
		paletteImage = stbi_load(options.paletteFilename, &paletteWidth, &paletteHeight, &paletteFormat, 0);
	} else {
		fprintf(stderr, "The file extension \"%s\" is not supported. Please use one of the following instead: act, pal, gpl, txt, png\n", options.paletteFilename);
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

    if (options.rangeMax == -1) {
        options.rangeMax = countOfUniquePaletteColors - 1;
    }

    liq_attr *attr = liq_attr_create();
    liq_set_max_colors(attr, options.rangeMax-options.rangeMin+1);

    liq_image *inputLiqImage = liq_image_create_rgba(attr, inputImage, inputWidth, inputHeight, 0);

    for (int i = options.rangeMax; i >= options.rangeMin; i--) {
        liq_image_add_fixed_color(inputLiqImage, (liq_color){uniqueRgbPaletteColors[i].R, uniqueRgbPaletteColors[i].G, uniqueRgbPaletteColors[i].B, 255});
    }

    liq_result *quantizationResult;
    liq_image_quantize(inputLiqImage, attr, &quantizationResult);

    unsigned char* quantizedImage = (unsigned char*)malloc(inputWidth * inputHeight);

    liq_write_remapped_image(quantizationResult, inputLiqImage, quantizedImage, inputWidth * inputHeight);
    const liq_palette *palette = liq_get_palette(quantizationResult);
    
    LodePNGState state;
    lodepng_state_init(&state);

    if (options.bitDepth == 4)
    {
        for (int i = options.rangeMin; i <= options.rangeMax; i++) {
            lodepng_palette_add(&state.info_png.color, uniqueRgbPaletteColors[i].R, uniqueRgbPaletteColors[i].G, uniqueRgbPaletteColors[i].B, 255);
            lodepng_palette_add(&state.info_raw, uniqueRgbPaletteColors[i].R, uniqueRgbPaletteColors[i].G, uniqueRgbPaletteColors[i].B, 255);
        }
    }
    else
    {
        for (int i = 0; i < countOfUniquePaletteColors; i++) {
            lodepng_palette_add(&state.info_png.color, uniqueRgbPaletteColors[i].R, uniqueRgbPaletteColors[i].G, uniqueRgbPaletteColors[i].B, 255);
            lodepng_palette_add(&state.info_raw, uniqueRgbPaletteColors[i].R, uniqueRgbPaletteColors[i].G, uniqueRgbPaletteColors[i].B, 255);
        }
    }

    state.info_png.color.colortype = LCT_PALETTE;
    state.info_png.color.bitdepth = options.bitDepth;
    state.info_raw.colortype = LCT_PALETTE;
    state.info_raw.bitdepth = options.bitDepth;
    state.encoder.auto_convert = 0;

    int imageSize = (inputWidth * inputHeight * 4 + 7) / (options.bitDepth == 4 ? 8 : 1);
    unsigned char* outputImage = (unsigned char*)malloc(imageSize);
    memset(outputImage, 0, imageSize);
    if (!outputImage) {
        perror("Failed to allocate memory for image");
        return EXIT_FAILURE;
    }

    if (options.bitDepth == 4)
    {
        for (int i = 0; i < inputWidth * inputHeight; i++) {
            size_t byte_index = i / 2;
            int byte_half = i % 2 == 1;
            int colorIndex = quantizedImage[i];

            outputImage[byte_index] |= (unsigned char)(colorIndex << (byte_half ? 0 : 4));
        }
    }
    else if(options.bitDepth == 8)
    {
        for (int i = 0; i < inputWidth * inputHeight; i++) {
            outputImage[i] = quantizedImage[i] + options.rangeMin;
        }
    }

    liq_result_destroy(quantizationResult);
    liq_image_destroy(inputLiqImage);
    free(quantizedImage);

    unsigned char* buffer;
    size_t buffer_size;
    if (lodepng_encode(&buffer, &buffer_size, outputImage, inputWidth, inputHeight, &state)) {
        fprintf(stderr, "Encoder error: %s\n", lodepng_error_text(state.error));
        free(outputImage);
        lodepng_state_cleanup(&state);
        return EXIT_FAILURE;
    }

    if (lodepng_save_file(buffer, buffer_size, options.outputFilename)) {
        fprintf(stderr, "Error saving PNG file\n");
        free(outputImage);
        free(buffer);
        lodepng_state_cleanup(&state);
        return EXIT_FAILURE;
    }
    
    free(outputImage);
    free(buffer);
    lodepng_state_cleanup(&state);

    stbi_image_free(inputImage);
    stbi_image_free(paletteImage);

    return EXIT_SUCCESS;
}
