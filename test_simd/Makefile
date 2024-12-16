CC = gcc
CFLAGS = -Wall -O3 -D_GNU_SOURCE -march=armv8-a+simd+fp -mtune=cortex-a76 -mfpu=neon-fp-armv8
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
