CC = gcc
CFLAGS = -O2 -Wall
LDFLAGS = -lOpenCL
INCLUDES = -I/usr/include/CL

TARGET = opencl_test

$(TARGET): opencl_test.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)
