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

# Test target: verify output matches reference files
# Generic - just add new test input/output files without changing Makefile
test: $(TARGET)
	@echo "Running verification tests..."
	@passed=0; failed=0; \
	for ref in testoutput-C/*.bmp; do \
		basename=$$(basename "$$ref" .bmp); \
		input=""; \
		for ext in png jpg jpeg; do \
			if [ -f "testinput/$$basename.$$ext" ]; then \
				input="testinput/$$basename.$$ext"; \
				break; \
			fi; \
		done; \
		if [ -z "$$input" ]; then \
			echo "SKIP: No input file found for $$basename"; \
			continue; \
		fi; \
		tmpout=$$(mktemp --suffix=.bmp); \
		if ./$(TARGET) -C "$$input" -o "$$tmpout" 2>/dev/null; then \
			if cmp -s "$$ref" "$$tmpout"; then \
				echo "PASS: $$basename"; \
				passed=$$((passed + 1)); \
			else \
				echo "FAIL: $$basename (output mismatch)"; \
				failed=$$((failed + 1)); \
			fi; \
		else \
			echo "FAIL: $$basename (conversion error)"; \
			failed=$$((failed + 1)); \
		fi; \
		rm -f "$$tmpout"; \
	done; \
	echo ""; \
	echo "Results: $$passed passed, $$failed failed"; \
	if [ $$failed -gt 0 ]; then exit 1; fi

.PHONY: all clean test
