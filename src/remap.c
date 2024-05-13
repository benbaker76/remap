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
    int paletteSlot;
    bool autoPaletteSlot;
    bool mask;
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

int write_image(unsigned char* quantizedImage, int inputWidth, int inputHeight, rgbcolor* palette, int paletteCount)
{
    int result = EXIT_SUCCESS;
    LodePNGState state;
    lodepng_state_init(&state);
    unsigned char* outputImage = NULL, *pngOutput = NULL;

    if (options.bitDepth == 4)
    {
        for (int i = options.rangeMin; i <= options.rangeMax; i++) {
            lodepng_palette_add(&state.info_png.color, palette[i].R, palette[i].G, palette[i].B, 255);
            lodepng_palette_add(&state.info_raw, palette[i].R, palette[i].G, palette[i].B, 255);
        }
    }
    else if (options.bitDepth == 8)
    {
        for (int i = 0; i < paletteCount; i++) {
            lodepng_palette_add(&state.info_png.color, palette[i].R, palette[i].G, palette[i].B, 255);
            lodepng_palette_add(&state.info_raw, palette[i].R, palette[i].G, palette[i].B, 255);
        }
    }

    state.info_png.color.colortype = LCT_PALETTE;
    state.info_png.color.bitdepth = options.bitDepth;
    state.info_raw.colortype = LCT_PALETTE;
    state.info_raw.bitdepth = options.bitDepth;
    state.encoder.auto_convert = 0;

    int bpp = lodepng_get_bpp(&state.info_raw);
    int imageSize = (inputWidth * inputHeight * bpp + 7) / 8;
    outputImage = (unsigned char*)calloc(imageSize, sizeof(unsigned char));
    if (!outputImage) {
        perror("Failed to allocate memory for image");
        result = EXIT_FAILURE;
        goto png_exit;
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

    size_t pngOutputSize;
    if (lodepng_encode(&pngOutput, &pngOutputSize, outputImage, inputWidth, inputHeight, &state)) {
        fprintf(stderr, "Encoder error: %s\n", lodepng_error_text(state.error));
        result = EXIT_FAILURE;
        goto png_exit;
    }

    if (lodepng_save_file(pngOutput, pngOutputSize, options.outputFilename)) {
        fprintf(stderr, "Error saving PNG file\n");
        result = EXIT_FAILURE;
        goto png_exit;
    }

png_exit:
    if (pngOutput != NULL)
    {
        free(pngOutput);

        if (outputImage != NULL)
        {
            lodepng_state_cleanup(&state);

            free(outputImage);
        }
    }

    return result;
}

int write_image_mask(unsigned char *inputImage, int inputWidth, int inputHeight)
{
    int result = EXIT_SUCCESS;
    LodePNGState state;
    lodepng_state_init(&state);
    unsigned char* outputImage = NULL, *pngOutput = NULL;
    unsigned char* maskFilename = NULL;

    state.info_png.color.colortype = LCT_RGBA;
    state.info_png.color.bitdepth = 8;
    state.info_raw.colortype = LCT_RGBA;
    state.info_raw.bitdepth = 8;
    state.encoder.auto_convert = 0;

    int bpp = lodepng_get_bpp(&state.info_raw);
    int imageSize = (inputWidth * inputHeight * bpp + 7) / 8;
    outputImage = (unsigned char*)calloc(imageSize, sizeof(unsigned char));
    if (!outputImage) {
        perror("Failed to allocate memory for image");
        result = EXIT_FAILURE;
        goto mask_exit;
    }

    // Iterate over the pixels in the input image
    for (int i = 0; i < inputWidth * inputHeight * 4; i += 4) {
        // If the pixel is not alpha (i.e., it has some color), make it white
        if (inputImage[i + 3] != 0) {
            outputImage[i] = 255;     // Red
            outputImage[i + 1] = 255; // Green
            outputImage[i + 2] = 255; // Blue
            outputImage[i + 3] = inputImage[i + 3]; // Alpha
        } else {
            // If the pixel is alpha, keep it as is
            outputImage[i] = inputImage[i];     // Red
            outputImage[i + 1] = inputImage[i + 1]; // Green
            outputImage[i + 2] = inputImage[i + 2]; // Blue
            outputImage[i + 3] = inputImage[i + 3]; // Alpha
        }
    }

    size_t outputSize;
    if (lodepng_encode(&pngOutput, &outputSize, outputImage, inputWidth, inputHeight, &state)) {
        fprintf(stderr, "Encoder error: %s\n", lodepng_error_text(state.error));
        result = EXIT_FAILURE;
        goto mask_exit;
    }

    maskFilename = strdup((const char *)options.outputFilename);
    char* maskFilenameExt = strrchr(maskFilename, '.');
    if (maskFilenameExt != NULL) {
        strcpy(maskFilenameExt, "_mask.png");
    }

    if (lodepng_save_file(pngOutput, outputSize, maskFilename)) {
        fprintf(stderr, "Error saving PNG file\n");
        result = EXIT_FAILURE;
        goto mask_exit;
    }

mask_exit:
    if (pngOutput != NULL)
    {
        free(pngOutput);

        if (outputImage != NULL)
        {
            lodepng_state_cleanup(&state);

            free(outputImage);
        }
    }
    
    if (maskFilename != NULL)
    {
        free(maskFilename);
    }

    return result;
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

int get_unique_color_palette(unsigned char* image, int width, int height, int format, rgbcolor **uniquePalette)
{
    rgbcolor *palette = (rgbcolor*) malloc(width * height * sizeof(rgbcolor));

    for (int i = 0; i < width * height; i++) {
        float R = (float) image[i * format + 0];
        float G = (float) image[i * format + 1];
        float B = (float) image[i * format + 2];

        if (format == 4) {
            float A = (float) image[i * format + 3] / 255.0f;
            rgbacolor rgba;
            rgbacolor_init(&rgba, R, G, B, A);
            rgbcolor rgb;
            rgba_to_rgb(&rgba, &rgb, NULL);
            R = rgb.R;
            G = rgb.G;
            B = rgb.B;
        }

        rgbcolor_init(palette + i, R, G, B);
    }

    qsort(palette, width * height, sizeof(rgbcolor), compareRgbcolor);

    int paletteCount = 0;

    *uniquePalette = (rgbcolor*) malloc(width * height * sizeof(rgbcolor));

    for (int i = 0; i < width * height; i++) {
        rgbcolor* color = palette + i;
        float R = color->R;
        float G = color->G;
        float B = color->B;
        if (i == 0) {
            rgbcolor_init(*uniquePalette + paletteCount++, R, G, B);
        } else {
            rgbcolor* lastColor = *uniquePalette + paletteCount - 1;
            if (R != lastColor->R || G != lastColor->G || B != lastColor->B) {
                rgbcolor_init(*uniquePalette + paletteCount++, R, G, B);
            }
        }
    }

    free(palette);

    return paletteCount;
}

int get_unique_color_palette_count(unsigned char* image, int width, int height, int format)
{
    int paletteCount = 0;
    rgbcolor *uniquePalette;

    paletteCount = get_unique_color_palette(image, width, height, format, &uniquePalette);

    free(uniquePalette);

    return paletteCount;
}

const char *get_color_type(LodePNGColorType colorType)
{
    switch(colorType)
    {
    case LCT_GREY:
        return "GREY";
    case LCT_RGB:
        return "RGB";
    case LCT_PALETTE:
        return "PALETTE";
    case LCT_GREY_ALPHA:
        return "GREY_ALPHA";
    case LCT_RGBA:
        return "RGBA";
    }

    return "UNKNOWN";
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
        .paletteSlot = -1,
        .autoPaletteSlot = false,
        .mask = false
    };

    static struct option long_options[] = {
        {"range", required_argument, 0, 'r'},
        {"bits", required_argument, 0, 'b'},
        {"slot", required_argument, 0, 's'},
        {"mask", no_argument, 0, 'm'},
        {0, 0, 0, 0}
    };

    const char *usage_str = 
        "Usage: %s [options] <inputFilename> <paletteFilename> <outputFilename>\n"
        "  -r --range min-max  Use a range of colors from the palette\n"
        "  -b --bits 4|8       Bit depth of png output (default 8)\n"
        "  -s --slot n|auto    16 color palette slot\n"
        "  -m --mask           Generate a mask file\n";

    int option;
    while ((option = getopt_long(argc, argv, "r:b:s:m", long_options, NULL)) != -1) {
        switch (option) {
            case 'r':
                sscanf(optarg, "%d-%d", &options.rangeMin, &options.rangeMax);
                break;
            case 'b':
                options.bitDepth = atoi(optarg);
                break;
            case 's':
                if (strcmp(optarg, "auto") == 0) {
                    options.autoPaletteSlot = true;
                } else
                    options.paletteSlot = atoi(optarg);
                break;
            case 'm':
                options.mask = true;
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

    unsigned char* inputImage = NULL, *paletteImage = NULL, *outputImage = NULL, *quantizedImage = NULL;
    unsigned char* pngInput = NULL, *pngPalette = NULL, *pngOutput = NULL;
    int inputWidth, inputHeight;
    size_t pngInputSize = 0;
    LodePNGState inputState, paletteState;
    Color* colorPalette = NULL;
    int paletteCount = 0, transparentIndex = -1;
    rgbcolor *outputColorPalette = NULL;
    liq_image *inputLiqImage = NULL;
    liq_result *quantizationResult = NULL;

    lodepng_state_init(&inputState);

    inputState.info_raw.colortype = LCT_RGBA;
    inputState.info_raw.bitdepth = 8;

    inputState.decoder.color_convert = 1;

    lodepng_load_file(&pngInput, &pngInputSize, options.inputFilename);
    result = lodepng_decode(&inputImage, &inputWidth, &inputHeight, &inputState, pngInput, pngInputSize);

    if(result)
    {
        printf("Decoder error %u: %s\n", result, lodepng_error_text(result));
        result = EXIT_FAILURE;
        goto main_exit;
    }

    LodePNGColorMode* color = &inputState.info_png.color;
    const char *colorType = get_color_type(color->colortype);
    int inputPaletteCount = (color->colortype == LCT_PALETTE ? color->palettesize : get_unique_color_palette_count(inputImage, inputWidth, inputHeight, 4));

    printf("input: %s %dx%d (%s format, %d bits)\n", options.inputFilename, inputWidth, inputHeight, colorType, color->bitdepth);

	if (strcmp(paletteFileExtension, "act") == 0) {
		read_palette(options.paletteFilename, &colorPalette, &paletteCount, &transparentIndex);
	} else if (strcmp(paletteFileExtension, "pal") == 0) {
		read_palette(options.paletteFilename, &colorPalette, &paletteCount, &transparentIndex);
	} else if (strcmp(paletteFileExtension, "gpl") == 0) {
		read_palette(options.paletteFilename, &colorPalette, &paletteCount, &transparentIndex);
	} else if (strcmp(paletteFileExtension, "txt") == 0) {
		read_palette(options.paletteFilename, &colorPalette, &paletteCount, &transparentIndex);
	} else if (strcmp(paletteFileExtension, "png") == 0) {
        read_palette(options.paletteFilename, &colorPalette, &paletteCount, &transparentIndex);
	} else {
		fprintf(stderr, "The file extension \"%s\" is not supported. Please use one of the following instead: act, pal, gpl, txt, png\n", options.paletteFilename);
        result = EXIT_FAILURE;
        goto main_exit;
	}

    int outputColorPaletteCount = paletteCount;

    outputColorPalette = (rgbcolor*)malloc(paletteCount * sizeof(rgbcolor));
    for (int i = 0; i < paletteCount; i++) {
        float R = (float) colorPalette[i].R;
        float G = (float) colorPalette[i].G;
        float B = (float) colorPalette[i].B;
        rgbcolor_init(outputColorPalette + i, R, G, B);
    }

    double min_error = DBL_MAX;

    if (options.autoPaletteSlot) {
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
                options.paletteSlot = i;
            }

            liq_result_destroy(quantizationResult);
            liq_image_destroy(inputLiqImage);

            quantizationResult = NULL;
            inputLiqImage = NULL;
        }
    }

    if (options.paletteSlot != -1)
    {
        options.bitDepth = 4;
        options.rangeMin = options.paletteSlot * 16;
        options.rangeMax = options.rangeMin + 15;

        printf("paletteSlot: %d\n", options.paletteSlot);
    }

    if (options.rangeMax == -1) {
        options.rangeMax = outputColorPaletteCount - 1;
    }

    if (quantize_image(inputImage, inputWidth, inputHeight, outputColorPalette, &inputLiqImage, &quantizationResult) == EXIT_FAILURE) {
        result = EXIT_FAILURE;
        goto main_exit;
    }

    quantizedImage = (unsigned char*)malloc(inputWidth * inputHeight);

    if (liq_write_remapped_image(quantizationResult, inputLiqImage, quantizedImage, inputWidth * inputHeight) != LIQ_OK) {
        fprintf(stderr, "Failed to write remapped image\n");
        result = EXIT_FAILURE;
        goto main_exit;
    }

    const liq_palette *palette = liq_get_palette(quantizationResult);
    const liq_remapping_result *mappingResult = quantizationResult->remapping;
    int quality_percent = liq_get_quantization_quality(quantizationResult);

    printf("remapped image from %d to %d colors...MSE=%.3f (Q=%d)\n", inputPaletteCount, palette->count, mappingResult->palette_error, quality_percent);

    if (write_image(quantizedImage, inputWidth, inputHeight, outputColorPalette, outputColorPaletteCount) == EXIT_FAILURE) {
        result = EXIT_FAILURE;
        goto main_exit;
    }

    if (options.mask) {
        if (write_image_mask(inputImage, inputWidth, inputHeight) == EXIT_FAILURE) {
            result = EXIT_FAILURE;
            goto main_exit;
        }
    }

main_exit:

    free(quantizedImage);
    free(colorPalette);
    free(outputColorPalette);

    liq_result_destroy(quantizationResult);
    liq_image_destroy(inputLiqImage);

    if (pngInput != NULL)
    {
        free(pngInput);

        if (inputImage != NULL)
        {
            lodepng_state_cleanup(&inputState);

            free(inputImage);
        }
    }

    if (pngPalette != NULL)
    {
        free(pngPalette);

        if (paletteImage != NULL)
        {
            lodepng_state_cleanup(&paletteState);

            free(paletteImage);
        }
    }


    return EXIT_SUCCESS;
}
