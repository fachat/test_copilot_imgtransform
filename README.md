# test_copilot_imgtransform

Image transformation utility that converts PNG images to BMP format.

## Description

This program reads a PNG image file, resizes it to 720x576 resolution, reduces the color palette to 16 colors, and outputs the result as a BMP file to stdout.

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
./imgtransform <input.png> > output.bmp
```

Example:
```bash
./imgtransform photo.png > converted.bmp
```

## Features

- Reads PNG images of any size
- Resizes to 720x576 resolution using nearest-neighbor interpolation
- Reduces colors to 16-color VGA palette
- Outputs BMP format (4-bit color depth)
- Writes to stdout for easy piping

## Cleaning

```bash
make clean
```

