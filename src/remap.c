#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <getopt.h>
#include "convert.h"
#include "diff.h"
#include "palette.h"
#include "lodepng.h"
#include "libimagequant.h"

static struct options {
    const char* inputFilename;
    const char* paletteFilename;
    const char* outputFilename;
    int rangeMin;
    int rangeMax;
    int bitDepth;
    int slot;
    bool autoSlot;
} options;

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

void libimagequant_log(const liq_attr*, const char *message, void* user_info) {
    //fprintf(stderr, "%s\n", message);
}

int quantize_image(unsigned char* inputImage, int inputWidth, int inputHeight, rgbcolor* palette, liq_image **inputLiqImage, liq_result **quantizationResult)
{
    int result = EXIT_SUCCESS;

    liq_attr *attr = liq_attr_create();
    if (liq_set_max_colors(attr, options.rangeMax-options.rangeMin+1) != LIQ_OK)
    {
        fprintf(stderr, "Failed to set max colors\n");
        result = EXIT_FAILURE;
        goto quantize_image_exit;
    }

    if (liq_set_quality(attr, 0, 100) != LIQ_OK)
    {
        fprintf(stderr, "Failed to set quality\n");
        result = EXIT_FAILURE;
        goto quantize_image_exit;
    }

    liq_set_log_callback(attr, libimagequant_log, NULL);

    *inputLiqImage = liq_image_create_rgba(attr, inputImage, inputWidth, inputHeight, 0);
    for (int i = options.rangeMin; i <= options.rangeMax; i++) {
        if (liq_image_add_fixed_color(*inputLiqImage, (liq_color){palette[i].R, palette[i].G, palette[i].B, 255}) != LIQ_OK) {
            fprintf(stderr, "Failed to add color to image\n");
            result = EXIT_FAILURE;
            goto quantize_image_exit;
        }
    }

    if (liq_image_quantize(*inputLiqImage, attr, quantizationResult) != LIQ_OK) {
        fprintf(stderr, "Failed to quantize image\n");
        result = EXIT_FAILURE;
        goto quantize_image_exit;
    }

quantize_image_exit:

    return result;
}

int main(int argc, char** argv) {
    int result = EXIT_SUCCESS;

    options = (struct options) {
        .inputFilename = NULL,
        .paletteFilename = NULL,
        .outputFilename = NULL,
        .rangeMin = 0,
        .rangeMax = -1,
        .bitDepth = 8,
        .slot = -1,
        .autoSlot = false
    };

    static struct option long_options[] = {
        {"range", required_argument, 0, 'r'},
        {"bits", required_argument, 0, 'b'},
        {"slot", required_argument, 0, 's'},
        {0, 0, 0, 0}
    };

   const char *usage_str = 
        "Usage: %s\n"
        "  --range min-max\n"
        "  --bits 4|8 (default 8)\n"
        "  --slot auto|n (16 color palette slot)\n"
        "  <inputFilename> <paletteFilename> <outputFilename>\n";

    int option;
    while ((option = getopt_long(argc, argv, "r:b:s:", long_options, NULL)) != -1) {
        switch (option) {
            case 'r':
                sscanf(optarg, "%d-%d", &options.rangeMin, &options.rangeMax);
                break;
            case 'b':
                options.bitDepth = atoi(optarg);
                break;
            case 's':
                if (strcmp(optarg, "auto") == 0) {
                    options.autoSlot = true;
                } else
                    options.slot = atoi(optarg);
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

    unsigned char* inputImage = NULL;
    int inputWidth, inputHeight, inputFormat = 4;
    unsigned char* png = NULL;
    size_t pngsize;
    LodePNGState state;

    lodepng_state_init(&state);
    lodepng_load_file(&png, &pngsize, options.inputFilename);
    result = lodepng_decode(&inputImage, &inputWidth, &inputHeight, &state, png, pngsize);

    if(result)
    {
        printf("decoder error! %u: %s\n", result, lodepng_error_text(result));
        lodepng_state_cleanup(&state);
        free(png);
        result = EXIT_FAILURE;
        goto main_exit;
    }

    LodePNGColorMode* color = &state.info_png.color;
    int inputPaletteCount = color->palettesize;

    lodepng_state_cleanup(&state);
    free(png);

    printf("input: %s %dx%d\n", options.inputFilename, inputWidth, inputHeight);

    unsigned char* paletteImage = NULL;
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
        size_t pngsize;
        unsigned width, height;

        lodepng_state_init(&state);

        lodepng_load_file(&png, &pngsize, options.paletteFilename);
        result = lodepng_decode(&paletteImage, &width, &height, &state, png, pngsize);

        if(result)
        {
            printf("decoder error %u: %s\n", result, lodepng_error_text(result));
            lodepng_state_cleanup(&state);
            free(png);
            result = EXIT_FAILURE;
            goto main_exit;
        }

        LodePNGColorMode* color = &state.info_png.color;
        paletteCount = color->palettesize;
        colorPalette = (Color*)malloc(paletteCount * sizeof(Color));
        
        if(color->colortype == LCT_PALETTE) {
            for(size_t i = 0; i < paletteCount; i++) {
                colorPalette[i].R = color->palette[i * 4 + 0];
                colorPalette[i].G = color->palette[i * 4 + 1];
                colorPalette[i].B = color->palette[i * 4 + 2];
            }
        }

        lodepng_state_cleanup(&state);
        free(png);
	} else {
		fprintf(stderr, "The file extension \"%s\" is not supported. Please use one of the following instead: act, pal, gpl, txt, png\n", options.paletteFilename);
        result = EXIT_FAILURE;
        goto main_exit;
	}

    int outputColorPaletteCount = paletteCount;

    rgbcolor *outputColorPalette = (rgbcolor*)malloc(paletteCount * sizeof(rgbcolor));
    for (int i = 0; i < paletteCount; i++) {
        float R = (float) colorPalette[i].R;
        float G = (float) colorPalette[i].G;
        float B = (float) colorPalette[i].B;
        rgbcolor_init(outputColorPalette + i, R, G, B);
    }

    liq_image *inputLiqImage;
    liq_result *quantizationResult;
    double min_error = DBL_MAX;

    if (options.autoSlot) {
        options.bitDepth = 4;

        for (int i = 0; i < 16; i++) {
            options.rangeMin = i * 16;
            options.rangeMax = options.rangeMin + 15;
            
            if (quantize_image(inputImage, inputWidth, inputHeight, outputColorPalette, &inputLiqImage, &quantizationResult) == EXIT_FAILURE) {
                result = EXIT_FAILURE;
                goto main_exit;
            }

            if (quantizationResult->palette_error < min_error) {
                min_error = quantizationResult->palette_error;
                options.slot = i;
            }

            liq_result_destroy(quantizationResult);
            liq_image_destroy(inputLiqImage);
        }
    }

    if (options.slot != -1)
    {
        options.bitDepth = 4;
        options.rangeMin = options.slot * 16;
        options.rangeMax = options.rangeMin + 15;

        printf("slot: %d\n", options.slot);
    }

    if (options.rangeMax == -1) {
        options.rangeMax = outputColorPaletteCount - 1;
    }

    if (quantize_image(inputImage, inputWidth, inputHeight, outputColorPalette, &inputLiqImage, &quantizationResult) == EXIT_FAILURE) {
        result = EXIT_FAILURE;
        goto main_exit;
    }

    unsigned char* quantizedImage = (unsigned char*)malloc(inputWidth * inputHeight);

    if (liq_write_remapped_image(quantizationResult, inputLiqImage, quantizedImage, inputWidth * inputHeight) != LIQ_OK) {
        fprintf(stderr, "Failed to write remapped image\n");
        result = EXIT_FAILURE;
        goto main_exit;
    }

    const liq_palette *palette = liq_get_palette(quantizationResult);
    const liq_remapping_result *mappingResult = quantizationResult->remapping;
    int quality_percent = liq_get_quantization_quality(quantizationResult);

    printf("remapped image from %d to %d colors...MSE=%.3f (Q=%d)\n", inputPaletteCount, palette->count, mappingResult->palette_error, quality_percent);

    lodepng_state_init(&state);

    if (options.bitDepth == 4)
    {
        for (int i = options.rangeMin; i <= options.rangeMax; i++) {
            lodepng_palette_add(&state.info_png.color, outputColorPalette[i].R, outputColorPalette[i].G, outputColorPalette[i].B, 255);
            lodepng_palette_add(&state.info_raw, outputColorPalette[i].R, outputColorPalette[i].G, outputColorPalette[i].B, 255);
        }
    }
    else if (options.bitDepth == 8)
    {
        for (int i = 0; i < outputColorPaletteCount; i++) {
            lodepng_palette_add(&state.info_png.color, outputColorPalette[i].R, outputColorPalette[i].G, outputColorPalette[i].B, 255);
            lodepng_palette_add(&state.info_raw, outputColorPalette[i].R, outputColorPalette[i].G, outputColorPalette[i].B, 255);
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
        result = EXIT_FAILURE;
        goto main_exit;
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

    unsigned char* buffer;
    size_t buffer_size;
    if (lodepng_encode(&buffer, &buffer_size, outputImage, inputWidth, inputHeight, &state)) {
        fprintf(stderr, "Encoder error: %s\n", lodepng_error_text(state.error));
        result = EXIT_FAILURE;
        goto main_exit;
    }

    if (lodepng_save_file(buffer, buffer_size, options.outputFilename)) {
        fprintf(stderr, "Error saving PNG file\n");
        result = EXIT_FAILURE;
        goto main_exit;
    }

main_exit:

    free(outputColorPalette);
    free(quantizedImage);
    liq_result_destroy(quantizationResult);
    liq_image_destroy(inputLiqImage);
    
    free(outputImage);
    free(buffer);
    lodepng_state_cleanup(&state);

    free(inputImage);
    free(paletteImage);

    return EXIT_SUCCESS;
}
