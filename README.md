# test_copilot_imgtransform

Image transformation utility that converts PNG and JPEG images to BMP format.

## Description

This program reads an image file (PNG or JPEG/JPG), resizes it to 720x576 resolution, reduces the color palette to 16 colors, and outputs the result as a BMP file. The input image format is automatically detected from the file content (magic bytes), not from the file extension.

## Requirements

- gcc compiler
- libpng development library
- libjpeg development library

On Ubuntu/Debian:
```bash
sudo apt-get install build-essential libpng-dev libjpeg-dev
```

## Building

```bash
make
```

## Usage

```bash
./imgtransform [OPTIONS] [input_image]
```

### Options

- `-h` - Show help message and exit
- `-o <file>` - Save output to `<file>` instead of stdout
- `-c` - Crop the source image to match target aspect ratio (720x576). If the source is too wide, crop left and right sides equally. If the source is too tall, crop top and bottom equally.

If no input file is specified, image data is read from stdin.

### Supported Input Formats

The program automatically detects the input image format based on the file's magic bytes:

- **PNG** (Portable Network Graphics) - Files starting with the PNG signature (0x89 0x50 0x4E 0x47)
- **JPEG/JPG** (Joint Photographic Experts Group) - Files starting with the JPEG signature (0xFF 0xD8 0xFF)

### Examples

```bash
# Convert a PNG file and output to stdout
./imgtransform photo.png > converted.bmp

# Convert a JPEG file and save to a specific output file
./imgtransform -o converted.bmp photo.jpg

# Convert with cropping to preserve aspect ratio
./imgtransform -c -o converted.bmp photo.png

# Read from stdin and output to file (works with both PNG and JPEG)
cat photo.jpg | ./imgtransform -o converted.bmp

# Show help
./imgtransform -h
```

## Features

- Reads PNG and JPEG/JPG images of any size
- Automatic input format detection based on file magic bytes
- Supports reading from stdin
- Resizes to 720x576 resolution using nearest-neighbor interpolation
- Optional cropping to match target aspect ratio (preserves image proportions)
- Reduces colors to 16-color VGA palette
- Outputs BMP format (4-bit color depth)
- Writes to stdout or a specified output file

## Cleaning

```bash
make clean
```

