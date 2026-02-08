#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <png.h>

#define TARGET_WIDTH 720
#define TARGET_HEIGHT 576
#define NUM_COLORS 16

// BMP file structures
#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BMPFileHeader;

typedef struct {
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BMPInfoHeader;

typedef struct {
    uint8_t rgbBlue;
    uint8_t rgbGreen;
    uint8_t rgbRed;
    uint8_t rgbReserved;
} RGBQuad;
#pragma pack(pop)

// Color structure for quantization
typedef struct {
    uint8_t r, g, b;
} Color;

// Image structure
typedef struct {
    int width;
    int height;
    uint8_t *data; // RGB data
} Image;

// Read PNG file
Image* read_png(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return NULL;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        return NULL;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
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
        fclose(fp);
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
            fclose(fp);
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
        fclose(fp);
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
        fclose(fp);
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
    fclose(fp);

    return img;
}

// Resize image using simple nearest neighbor interpolation
Image* resize_image(Image *src, int new_width, int new_height) {
    Image *dst = (Image*)malloc(sizeof(Image));
    if (!dst) {
        return NULL;
    }
    
    dst->width = new_width;
    dst->height = new_height;
    dst->data = (uint8_t*)malloc(new_width * new_height * 3);
    if (!dst->data) {
        free(dst);
        return NULL;
    }

    float x_ratio = (float)src->width / new_width;
    float y_ratio = (float)src->height / new_height;

    for (int y = 0; y < new_height; y++) {
        for (int x = 0; x < new_width; x++) {
            int src_x = (int)(x * x_ratio);
            int src_y = (int)(y * y_ratio);
            
            int src_idx = (src_y * src->width + src_x) * 3;
            int dst_idx = (y * new_width + x) * 3;
            
            dst->data[dst_idx + 0] = src->data[src_idx + 0];
            dst->data[dst_idx + 1] = src->data[src_idx + 1];
            dst->data[dst_idx + 2] = src->data[src_idx + 2];
        }
    }

    return dst;
}

// Color quantization to 16-color VGA palette
void quantize_colors(Image *img, Color *palette, int num_colors) {
    // Use a standard 16-color palette (similar to VGA palette)
    Color vga_palette[16] = {
        {0, 0, 0},       // Black
        {0, 0, 170},     // Blue
        {0, 170, 0},     // Green
        {0, 170, 170},   // Cyan
        {170, 0, 0},     // Red
        {170, 0, 170},   // Magenta
        {170, 85, 0},    // Brown
        {170, 170, 170}, // Light Gray
        {85, 85, 85},    // Dark Gray
        {85, 85, 255},   // Light Blue
        {85, 255, 85},   // Light Green
        {85, 255, 255},  // Light Cyan
        {255, 85, 85},   // Light Red
        {255, 85, 255},  // Light Magenta
        {255, 255, 85},  // Yellow
        {255, 255, 255}  // White
    };
    
    memcpy(palette, vga_palette, sizeof(Color) * 16);
    
    // Map each pixel to nearest color in palette
    for (int i = 0; i < img->width * img->height; i++) {
        int idx = i * 3;
        uint8_t r = img->data[idx + 0];
        uint8_t g = img->data[idx + 1];
        uint8_t b = img->data[idx + 2];
        
        int min_dist = INT_MAX;
        int best_color = 0;
        
        for (int c = 0; c < num_colors; c++) {
            int dr = (int)r - palette[c].r;
            int dg = (int)g - palette[c].g;
            int db = (int)b - palette[c].b;
            int dist = dr*dr + dg*dg + db*db;
            
            if (dist < min_dist) {
                min_dist = dist;
                best_color = c;
            }
        }
        
        img->data[idx + 0] = palette[best_color].r;
        img->data[idx + 1] = palette[best_color].g;
        img->data[idx + 2] = palette[best_color].b;
    }
}

// Write BMP to stdout
void write_bmp(Image *img, Color *palette, int num_colors) {
    // BMP rows must be padded to 4-byte boundary
    int row_size = ((img->width * 4 + 31) / 32) * 4; // 4 bits per pixel
    int pixel_data_size = row_size * img->height;
    
    BMPFileHeader file_header;
    file_header.bfType = 0x4D42; // "BM"
    file_header.bfSize = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + 
                         sizeof(RGBQuad) * num_colors + pixel_data_size;
    file_header.bfReserved1 = 0;
    file_header.bfReserved2 = 0;
    file_header.bfOffBits = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + 
                            sizeof(RGBQuad) * num_colors;
    
    BMPInfoHeader info_header;
    info_header.biSize = sizeof(BMPInfoHeader);
    info_header.biWidth = img->width;
    info_header.biHeight = img->height;
    info_header.biPlanes = 1;
    info_header.biBitCount = 4; // 4 bits per pixel for 16 colors
    info_header.biCompression = 0; // BI_RGB
    info_header.biSizeImage = pixel_data_size;
    info_header.biXPelsPerMeter = 0;
    info_header.biYPelsPerMeter = 0;
    info_header.biClrUsed = num_colors;
    info_header.biClrImportant = num_colors;
    
    // Write headers
    fwrite(&file_header, sizeof(BMPFileHeader), 1, stdout);
    fwrite(&info_header, sizeof(BMPInfoHeader), 1, stdout);
    
    // Write palette
    for (int i = 0; i < num_colors; i++) {
        RGBQuad quad;
        quad.rgbBlue = palette[i].b;
        quad.rgbGreen = palette[i].g;
        quad.rgbRed = palette[i].r;
        quad.rgbReserved = 0;
        fwrite(&quad, sizeof(RGBQuad), 1, stdout);
    }
    
    // Create index map for quick color lookup
    uint8_t *indices = (uint8_t*)malloc(img->width * img->height);
    if (!indices) {
        fprintf(stderr, "Error: Memory allocation failed for color indices\n");
        return;
    }
    for (int i = 0; i < img->width * img->height; i++) {
        int idx = i * 3;
        uint8_t r = img->data[idx + 0];
        uint8_t g = img->data[idx + 1];
        uint8_t b = img->data[idx + 2];
        
        for (int c = 0; c < num_colors; c++) {
            if (r == palette[c].r && g == palette[c].g && b == palette[c].b) {
                indices[i] = c;
                break;
            }
        }
    }
    
    // Write pixel data (bottom to top, 4 bits per pixel, padded)
    uint8_t *row_buffer = (uint8_t*)calloc(row_size, 1);
    if (!row_buffer) {
        fprintf(stderr, "Error: Memory allocation failed for row buffer\n");
        free(indices);
        return;
    }
    for (int y = img->height - 1; y >= 0; y--) {
        memset(row_buffer, 0, row_size);
        for (int x = 0; x < img->width; x++) {
            int pixel_idx = y * img->width + x;
            uint8_t color_idx = indices[pixel_idx];
            
            int byte_idx = x / 2;
            if (x % 2 == 0) {
                row_buffer[byte_idx] |= (color_idx << 4);
            } else {
                row_buffer[byte_idx] |= color_idx;
            }
        }
        fwrite(row_buffer, row_size, 1, stdout);
    }
    
    free(row_buffer);
    free(indices);
}

void free_image(Image *img) {
    if (img) {
        if (img->data) {
            free(img->data);
        }
        free(img);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <png_file>\n", argv[0]);
        return 1;
    }
    
    const char *input_file = argv[1];
    
    // Read PNG
    Image *img = read_png(input_file);
    if (!img) {
        fprintf(stderr, "Error: Failed to read PNG file\n");
        return 1;
    }
    
    // Resize to 720x576
    Image *resized = resize_image(img, TARGET_WIDTH, TARGET_HEIGHT);
    free_image(img);
    
    if (!resized) {
        fprintf(stderr, "Error: Failed to resize image\n");
        return 1;
    }
    
    // Quantize to 16 colors
    Color palette[NUM_COLORS];
    quantize_colors(resized, palette, NUM_COLORS);
    
    // Write BMP to stdout
    write_bmp(resized, palette, NUM_COLORS);
    
    free_image(resized);
    
    return 0;
}
