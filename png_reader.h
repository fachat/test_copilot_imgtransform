#ifndef PNG_READER_H
#define PNG_READER_H

#include <stdio.h>
#include "image.h"

// Read PNG from file pointer
Image* read_png_from_fp(FILE *fp);

// Read PNG file
Image* read_png(const char *filename);

#endif // PNG_READER_H
