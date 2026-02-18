#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <getopt.h>
#include "image.h"
#include "png_reader.h"
#include "jpeg_reader.h"

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

// Detect image format from magic bytes
ImageFormat detect_image_format(FILE *fp) {
    unsigned char header[8];
    
    // Read the first 8 bytes for format detection
    size_t bytes_read = fread(header, 1, 8, fp);
    if (bytes_read < 3 || ferror(fp)) {
        // Need at least 3 bytes for JPEG signature, 8 for PNG
        // If there's a read error, don't seek back
        return FORMAT_UNKNOWN;
    }
    
    // Seek back to the beginning
    fseek(fp, 0, SEEK_SET);
    
    // Check for PNG signature: 0x89 0x50 0x4E 0x47 0x0D 0x0A 0x1A 0x0A
    if (bytes_read >= 8 &&
        header[0] == 0x89 && header[1] == 0x50 && header[2] == 0x4E && header[3] == 0x47 &&
        header[4] == 0x0D && header[5] == 0x0A && header[6] == 0x1A && header[7] == 0x0A) {
        return FORMAT_PNG;
    }
    
    // Check for JPEG signature: 0xFF 0xD8 0xFF
    if (header[0] == 0xFF && header[1] == 0xD8 && header[2] == 0xFF) {
        return FORMAT_JPEG;
    }
    
    return FORMAT_UNKNOWN;
}

// Read image with automatic format detection
Image* read_image_auto(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return NULL;
    }
    
    ImageFormat format = detect_image_format(fp);
    Image *img = NULL;
    
    switch (format) {
        case FORMAT_PNG:
            img = read_png_from_fp(fp);
            break;
        case FORMAT_JPEG:
            img = read_jpeg_from_fp(fp);
            break;
        default:
            fprintf(stderr, "Error: Unknown or unsupported image format\n");
            break;
    }
    
    fclose(fp);
    return img;
}

// Read image from stdin with automatic format detection
Image* read_image_from_stdin(void) {
    // Read the header first to detect the format
    unsigned char header[8];
    if (fread(header, 1, 8, stdin) != 8) {
        fprintf(stderr, "Error: Failed to read image header from stdin\n");
        return NULL;
    }
    
    ImageFormat format = FORMAT_UNKNOWN;
    
    // Check for PNG signature
    if (header[0] == 0x89 && header[1] == 0x50 && header[2] == 0x4E && header[3] == 0x47 &&
        header[4] == 0x0D && header[5] == 0x0A && header[6] == 0x1A && header[7] == 0x0A) {
        format = FORMAT_PNG;
    }
    // Check for JPEG signature
    else if (header[0] == 0xFF && header[1] == 0xD8 && header[2] == 0xFF) {
        format = FORMAT_JPEG;
    }
    
    if (format == FORMAT_UNKNOWN) {
        fprintf(stderr, "Error: Unknown or unsupported image format from stdin\n");
        return NULL;
    }
    
    // We can't seek stdin, so we need to read the whole input into memory
    // Start with the header we already read
    size_t capacity = 10 * 1024 * 1024; // 10MB initial capacity
    size_t size = 8;
    unsigned char *buffer = (unsigned char*)malloc(capacity);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }
    memcpy(buffer, header, 8);
    
    // Read the rest from stdin with a read chunk size
    #define READ_CHUNK_SIZE 65536
    size_t bytes_read;
    while (1) {
        // Ensure we have space for at least one chunk
        if (capacity - size < READ_CHUNK_SIZE) {
            size_t new_capacity = capacity + (capacity / 2); // Grow by 50%
            unsigned char *new_buffer = (unsigned char*)realloc(buffer, new_capacity);
            if (!new_buffer) {
                free(buffer);
                fprintf(stderr, "Error: Memory reallocation failed\n");
                return NULL;
            }
            buffer = new_buffer;
            capacity = new_capacity;
        }
        
        bytes_read = fread(buffer + size, 1, READ_CHUNK_SIZE, stdin);
        if (bytes_read == 0) {
            break; // EOF or error
        }
        size += bytes_read;
    }
    
    // Create a temporary file to hold the data (needed for libjpeg/libpng)
    FILE *tmp = tmpfile();
    if (!tmp) {
        free(buffer);
        fprintf(stderr, "Error: Cannot create temporary file\n");
        return NULL;
    }
    
    fwrite(buffer, 1, size, tmp);
    free(buffer);
    fseek(tmp, 0, SEEK_SET);
    
    Image *img = NULL;
    switch (format) {
        case FORMAT_PNG:
            img = read_png_from_fp(tmp);
            break;
        case FORMAT_JPEG:
            img = read_jpeg_from_fp(tmp);
            break;
        default:
            break;
    }
    
    fclose(tmp);
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

// Crop image to match target aspect ratio (crop on long side only)
Image* crop_to_aspect_ratio(Image *src, int target_width, int target_height) {
    float src_aspect = (float)src->width / src->height;
    float target_aspect = (float)target_width / target_height;
    
    int new_width = src->width;
    int new_height = src->height;
    int crop_x = 0;
    int crop_y = 0;
    
    if (src_aspect > target_aspect) {
        // Source is too wide, crop left and right
        new_width = (int)(src->height * target_aspect + 0.5);
        crop_x = (src->width - new_width) / 2;
    } else if (src_aspect < target_aspect) {
        // Source is too tall, crop top and bottom
        new_height = (int)(src->width / target_aspect + 0.5);
        crop_y = (src->height - new_height) / 2;
    } else {
        // Aspect ratios match, no cropping needed
        return NULL;
    }
    
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
    
    for (int y = 0; y < new_height; y++) {
        for (int x = 0; x < new_width; x++) {
            int src_idx = ((y + crop_y) * src->width + (x + crop_x)) * 3;
            int dst_idx = (y * new_width + x) * 3;
            
            dst->data[dst_idx + 0] = src->data[src_idx + 0];
            dst->data[dst_idx + 1] = src->data[src_idx + 1];
            dst->data[dst_idx + 2] = src->data[src_idx + 2];
        }
    }
    
    return dst;
}

// Structure for median-cut color box
typedef struct {
    int r_min, r_max;
    int g_min, g_max;
    int b_min, b_max;
    Color *colors;
    int count;
} ColorBox;

// Find the range of each color channel in a box
void find_box_range(ColorBox *box) {
    box->r_min = box->g_min = box->b_min = 255;
    box->r_max = box->g_max = box->b_max = 0;
    
    for (int i = 0; i < box->count; i++) {
        if (box->colors[i].r < box->r_min) box->r_min = box->colors[i].r;
        if (box->colors[i].r > box->r_max) box->r_max = box->colors[i].r;
        if (box->colors[i].g < box->g_min) box->g_min = box->colors[i].g;
        if (box->colors[i].g > box->g_max) box->g_max = box->colors[i].g;
        if (box->colors[i].b < box->b_min) box->b_min = box->colors[i].b;
        if (box->colors[i].b > box->b_max) box->b_max = box->colors[i].b;
    }
}

// Comparison functions for qsort
static int compare_r(const void *a, const void *b) {
    return ((Color*)a)->r - ((Color*)b)->r;
}

static int compare_g(const void *a, const void *b) {
    return ((Color*)a)->g - ((Color*)b)->g;
}

static int compare_b(const void *a, const void *b) {
    return ((Color*)a)->b - ((Color*)b)->b;
}

// Calculate average color of a box
Color box_average(ColorBox *box) {
    long r_sum = 0, g_sum = 0, b_sum = 0;
    for (int i = 0; i < box->count; i++) {
        r_sum += box->colors[i].r;
        g_sum += box->colors[i].g;
        b_sum += box->colors[i].b;
    }
    Color avg;
    avg.r = (uint8_t)(r_sum / box->count);
    avg.g = (uint8_t)(g_sum / box->count);
    avg.b = (uint8_t)(b_sum / box->count);
    return avg;
}

// Generate optimized palette using median-cut algorithm
void generate_optimized_palette(Image *img, Color *palette, int num_colors) {
    int pixel_count = img->width * img->height;
    
    // Initialize palette to black as fallback in case of early return
    for (int i = 0; i < num_colors; i++) {
        palette[i].r = palette[i].g = palette[i].b = 0;
    }
    
    // Create array of all colors in image
    Color *all_colors = (Color*)malloc(sizeof(Color) * pixel_count);
    if (!all_colors) {
        fprintf(stderr, "Error: Memory allocation failed for color array\n");
        return;
    }
    
    for (int i = 0; i < pixel_count; i++) {
        int idx = i * 3;
        all_colors[i].r = img->data[idx + 0];
        all_colors[i].g = img->data[idx + 1];
        all_colors[i].b = img->data[idx + 2];
    }
    
    // Create initial box containing all colors
    ColorBox *boxes = (ColorBox*)malloc(sizeof(ColorBox) * num_colors);
    if (!boxes) {
        fprintf(stderr, "Error: Memory allocation failed for color boxes\n");
        free(all_colors);
        return;
    }
    
    boxes[0].colors = all_colors;
    boxes[0].count = pixel_count;
    find_box_range(&boxes[0]);
    int num_boxes = 1;
    
    // Split boxes until we have enough
    while (num_boxes < num_colors) {
        // Find box with largest range to split
        int best_box = -1;
        int best_range = 0;
        
        for (int i = 0; i < num_boxes; i++) {
            if (boxes[i].count < 2) continue; // Can't split a box with fewer than 2 colors
            
            int r_range = boxes[i].r_max - boxes[i].r_min;
            int g_range = boxes[i].g_max - boxes[i].g_min;
            int b_range = boxes[i].b_max - boxes[i].b_min;
            int max_range = r_range > g_range ? r_range : g_range;
            max_range = max_range > b_range ? max_range : b_range;
            
            if (max_range > best_range) {
                best_range = max_range;
                best_box = i;
            }
        }
        
        if (best_box == -1) break; // No more boxes can be split
        
        // Determine which channel to split on
        int r_range = boxes[best_box].r_max - boxes[best_box].r_min;
        int g_range = boxes[best_box].g_max - boxes[best_box].g_min;
        int b_range = boxes[best_box].b_max - boxes[best_box].b_min;
        
        if (r_range >= g_range && r_range >= b_range) {
            qsort(boxes[best_box].colors, boxes[best_box].count, sizeof(Color), compare_r);
        } else if (g_range >= r_range && g_range >= b_range) {
            qsort(boxes[best_box].colors, boxes[best_box].count, sizeof(Color), compare_g);
        } else {
            qsort(boxes[best_box].colors, boxes[best_box].count, sizeof(Color), compare_b);
        }
        
        // Split at median
        int median = boxes[best_box].count / 2;
        
        // Create new box from second half
        boxes[num_boxes].colors = boxes[best_box].colors + median;
        boxes[num_boxes].count = boxes[best_box].count - median;
        find_box_range(&boxes[num_boxes]);
        
        // Shrink original box to first half
        boxes[best_box].count = median;
        find_box_range(&boxes[best_box]);
        
        num_boxes++;
    }
    
    // Calculate average color for each box
    for (int i = 0; i < num_boxes; i++) {
        palette[i] = box_average(&boxes[i]);
    }
    
    // If we have fewer boxes than colors needed, fill remaining with black
    for (int i = num_boxes; i < num_colors; i++) {
        palette[i].r = palette[i].g = palette[i].b = 0;
    }
    
    free(boxes);
    free(all_colors);
}

// Color quantization to 16-color VGA palette or optimized palette
void quantize_colors(Image *img, Color *palette, int num_colors, int optimize_palette) {
    if (optimize_palette) {
        // Generate optimized palette from image colors
        generate_optimized_palette(img, palette, num_colors);
    } else {
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
    }
    
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

// Write BMP to file pointer
void write_bmp(Image *img, Color *palette, int num_colors, FILE *out) {
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
    fwrite(&file_header, sizeof(BMPFileHeader), 1, out);
    fwrite(&info_header, sizeof(BMPInfoHeader), 1, out);
    
    // Write palette
    for (int i = 0; i < num_colors; i++) {
        RGBQuad quad;
        quad.rgbBlue = palette[i].b;
        quad.rgbGreen = palette[i].g;
        quad.rgbRed = palette[i].r;
        quad.rgbReserved = 0;
        fwrite(&quad, sizeof(RGBQuad), 1, out);
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
        fwrite(row_buffer, row_size, 1, out);
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

void print_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s [OPTIONS] [input_image]\n\n", program_name);
    fprintf(stderr, "Image transformation utility that converts PNG or JPEG images to BMP format.\n");
    fprintf(stderr, "Reads an image file (PNG or JPEG/JPG), resizes it to 720x576 resolution,\n");
    fprintf(stderr, "reduces the color palette to 16 colors (VGA palette), and outputs the result\n");
    fprintf(stderr, "as a BMP file. The input image format is automatically detected from the\n");
    fprintf(stderr, "file content (magic bytes), not from the file extension.\n\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h           Show this help message and exit\n");
    fprintf(stderr, "  -o <file>    Save output to <file> instead of stdout\n");
    fprintf(stderr, "  -c           Crop the source image to match target aspect ratio.\n");
    fprintf(stderr, "               If the source is too wide, crop left and right sides equally.\n");
    fprintf(stderr, "               If the source is too tall, crop top and bottom equally.\n");
    fprintf(stderr, "  -C           Optimize the colour palette so that output colours best\n");
    fprintf(stderr, "               match the input colours, instead of using the VGA palette.\n\n");
    fprintf(stderr, "Supported input formats:\n");
    fprintf(stderr, "  - PNG (Portable Network Graphics)\n");
    fprintf(stderr, "  - JPEG/JPG (Joint Photographic Experts Group)\n\n");
    fprintf(stderr, "If no input file is specified, image data is read from stdin.\n");
}

int main(int argc, char *argv[]) {
    const char *input_file = NULL;
    const char *output_file = NULL;
    int crop_mode = 0;
    int optimize_palette = 0;
    int opt;
    
    while ((opt = getopt(argc, argv, "hcCo:")) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'o':
                output_file = optarg;
                break;
            case 'c':
                crop_mode = 1;
                break;
            case 'C':
                optimize_palette = 1;
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Get optional input filename from remaining arguments
    if (optind < argc) {
        input_file = argv[optind];
    }
    
    // Read image (auto-detects format)
    Image *img;
    if (input_file) {
        img = read_image_auto(input_file);
    } else {
        img = read_image_from_stdin();
    }
    
    if (!img) {
        fprintf(stderr, "Error: Failed to read image file\n");
        return 1;
    }
    
    // Optionally crop to target aspect ratio
    Image *source = img;
    if (crop_mode) {
        Image *cropped = crop_to_aspect_ratio(img, TARGET_WIDTH, TARGET_HEIGHT);
        if (cropped) {
            free_image(img);
            source = cropped;
        }
        // If cropped is NULL, aspect ratios already match, use original
    }
    
    // Resize to 720x576
    Image *resized = resize_image(source, TARGET_WIDTH, TARGET_HEIGHT);
    free_image(source);
    
    if (!resized) {
        fprintf(stderr, "Error: Failed to resize image\n");
        return 1;
    }
    
    // Quantize to 16 colors
    Color palette[NUM_COLORS];
    quantize_colors(resized, palette, NUM_COLORS, optimize_palette);
    
    // Determine output destination
    FILE *out = stdout;
    if (output_file) {
        out = fopen(output_file, "wb");
        if (!out) {
            fprintf(stderr, "Error: Cannot open output file %s\n", output_file);
            free_image(resized);
            return 1;
        }
    }
    
    // Write BMP
    write_bmp(resized, palette, NUM_COLORS, out);
    
    if (output_file) {
        fclose(out);
    }
    
    free_image(resized);
    
    return 0;
}
