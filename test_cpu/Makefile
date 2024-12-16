CC = gcc
CFLAGS = -Wall -O3 -D_GNU_SOURCE
LDFLAGS = -pthread -lm
DEPS = cpu_bench.h
OBJ = cpu_bench.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

cpu_bench: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

.PHONY: clean

clean:
	rm -f *.o cpu_bench
