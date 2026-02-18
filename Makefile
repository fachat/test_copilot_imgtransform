CC = gcc
CFLAGS = -Wall -Wextra -O2
LIBS = -lpng -ljpeg -lm

TARGET = imgtransform
SOURCES = imgtransform.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: all clean
