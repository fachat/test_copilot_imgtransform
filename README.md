# test_copilot_imgtransform

Image transformation utility that converts PNG images to BMP format.

## Description

This program reads a PNG image file, resizes it to 720x576 resolution, reduces the color palette to 16 colors, and outputs the result as a BMP file.

## Requirements

- gcc compiler
- libpng development library

On Ubuntu/Debian:
```bash
sudo apt-get install build-essential libpng-dev
```

## Building

```bash
make
```

## Usage

```bash
./imgtransform [OPTIONS] [input.png]
```

### Options

- `-h` - Show help message and exit
- `-o <file>` - Save output to `<file>` instead of stdout
- `-c` - Crop the source image to match target aspect ratio (720x576). If the source is too wide, crop left and right sides equally. If the source is too tall, crop top and bottom equally.

If no input file is specified, PNG data is read from stdin.

### Examples

```bash
# Convert a PNG file and output to stdout
./imgtransform photo.png > converted.bmp

# Convert a PNG file and save to a specific output file
./imgtransform -o converted.bmp photo.png

# Convert with cropping to preserve aspect ratio
./imgtransform -c -o converted.bmp photo.png

# Read from stdin and output to file
cat photo.png | ./imgtransform -o converted.bmp

# Show help
./imgtransform -h
```

## Features

- Reads PNG images of any size
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

