CC = gcc
CFLAGS = -Wall -Wextra -O2
LIBS = -lpng -ljpeg -lm

TARGET = imgtransform
SOURCES = imgtransform.c png_reader.c jpeg_reader.c
HEADERS = image.h png_reader.h jpeg_reader.h
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LIBS)

# Dependencies
imgtransform.o: imgtransform.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

png_reader.o: png_reader.c png_reader.h image.h
	$(CC) $(CFLAGS) -c $< -o $@

jpeg_reader.o: jpeg_reader.c jpeg_reader.h image.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: all clean
