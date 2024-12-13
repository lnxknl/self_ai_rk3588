CC = gcc
CFLAGS = -Wall -Wextra -I.
DEPS = vpu_test.h
OBJ = vpu_test.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

vpu_test: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o vpu_test
