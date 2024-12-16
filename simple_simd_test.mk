CC = gcc
CFLAGS = -O3 -march=armv8-a -mtune=cortex-a55 -ftree-vectorize

simple_simd_test: simple_simd_test.c
	$(CC) $(CFLAGS) -o $@ $< -lm

clean:
	rm -f simple_simd_test
