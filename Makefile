CC = gcc
CFLAGS = -Wall -O3 -D_GNU_SOURCE -march=armv8-a+simd -mtune=cortex-a76
LDFLAGS = -pthread -lm
DEPS = simd_test.h
OBJ = simd_test.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

simd_test: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

.PHONY: clean

clean:
	rm -f *.o simd_test
