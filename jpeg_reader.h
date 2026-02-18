#ifndef JPEG_READER_H
#define JPEG_READER_H

#include <stdio.h>
#include "image.h"

// Read JPEG from file pointer
Image* read_jpeg_from_fp(FILE *fp);

// Read JPEG file
Image* read_jpeg(const char *filename);

#endif // JPEG_READER_H
