#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <png.h>
#include "png_reader.h"

// Read PNG from file pointer
Image* read_png_from_fp(FILE *fp) {
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        return NULL;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    int width = png_get_image_width(png, info);
    int height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    // Convert to RGB
    if (bit_depth == 16)
        png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    png_bytep *row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    if (!row_pointers) {
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }
    
    for (int y = 0; y < height; y++) {
        row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png, info));
        if (!row_pointers[y]) {
            // Free previously allocated rows
            for (int i = 0; i < y; i++) {
                free(row_pointers[i]);
            }
            free(row_pointers);
            png_destroy_read_struct(&png, &info, NULL);
            return NULL;
        }
    }

    png_read_image(png, row_pointers);

    // Create image structure
    Image *img = (Image*)malloc(sizeof(Image));
    if (!img) {
        for (int y = 0; y < height; y++) {
            free(row_pointers[y]);
        }
        free(row_pointers);
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }
    
    img->width = width;
    img->height = height;
    img->data = (uint8_t*)malloc(width * height * 3);
    if (!img->data) {
        free(img);
        for (int y = 0; y < height; y++) {
            free(row_pointers[y]);
        }
        free(row_pointers);
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }

    // Copy to RGB format
    for (int y = 0; y < height; y++) {
        png_bytep row = row_pointers[y];
        for (int x = 0; x < width; x++) {
            png_bytep px = &(row[x * 4]);
            int idx = (y * width + x) * 3;
            img->data[idx + 0] = px[0]; // R
            img->data[idx + 1] = px[1]; // G
            img->data[idx + 2] = px[2]; // B
        }
    }

    // Clean up
    for (int y = 0; y < height; y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);
    png_destroy_read_struct(&png, &info, NULL);

    return img;
}

// Read PNG file
Image* read_png(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return NULL;
    }
    Image *img = read_png_from_fp(fp);
    fclose(fp);
    return img;
}
