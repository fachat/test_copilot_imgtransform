#ifndef IMAGE_H
#define IMAGE_H

#include <stdint.h>

// Image structure
typedef struct {
    int width;
    int height;
    uint8_t *data; // RGB data
} Image;

// Image format types
typedef enum {
    FORMAT_UNKNOWN,
    FORMAT_PNG,
    FORMAT_JPEG
} ImageFormat;

#endif // IMAGE_H
