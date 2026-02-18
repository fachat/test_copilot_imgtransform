#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <jpeglib.h>
#include "jpeg_reader.h"

// Read JPEG from file pointer
Image* read_jpeg_from_fp(FILE *fp) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fp);
    
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        return NULL;
    }
    
    // Force RGB output
    cinfo.out_color_space = JCS_RGB;
    
    jpeg_start_decompress(&cinfo);
    
    int width = cinfo.output_width;
    int height = cinfo.output_height;
    int row_stride = width * cinfo.output_components;
    
    // Create image structure
    Image *img = (Image*)malloc(sizeof(Image));
    if (!img) {
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        return NULL;
    }
    
    img->width = width;
    img->height = height;
    img->data = (uint8_t*)malloc(width * height * 3);
    if (!img->data) {
        free(img);
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        return NULL;
    }
    
    // Allocate row buffer using standard allocation
    JSAMPROW row_buffer = (JSAMPROW)malloc(row_stride);
    if (!row_buffer) {
        free(img->data);
        free(img);
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        return NULL;
    }
    
    // Read scanlines
    int y = 0;
    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, &row_buffer, 1);
        memcpy(&img->data[y * width * 3], row_buffer, width * 3);
        y++;
    }
    
    free(row_buffer);
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    
    return img;
}

// Read JPEG file
Image* read_jpeg(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return NULL;
    }
    Image *img = read_jpeg_from_fp(fp);
    fclose(fp);
    return img;
}
