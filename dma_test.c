#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/dma-buf.h>

#define DMA_BUF_SIZE (1024 * 1024)  // 1MB buffer
#define PAGE_SIZE 4096

int main() {
    int fd, ret;
    void *src_buf, *dst_buf;
    
    printf("RK3588 DMA Test\n");
    printf("===============\n");

    // Allocate source and destination buffers
    src_buf = mmap(NULL, DMA_BUF_SIZE, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (src_buf == MAP_FAILED) {
        perror("Failed to allocate source buffer");
        return -1;
    }

    dst_buf = mmap(NULL, DMA_BUF_SIZE, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (dst_buf == MAP_FAILED) {
        perror("Failed to allocate destination buffer");
        munmap(src_buf, DMA_BUF_SIZE);
        return -1;
    }

    // Initialize source buffer with pattern
    printf("Initializing source buffer with pattern...\n");
    for (int i = 0; i < DMA_BUF_SIZE; i++) {
        ((char*)src_buf)[i] = i & 0xFF;
    }

    // Clear destination buffer
    memset(dst_buf, 0, DMA_BUF_SIZE);

    // Try to open DMA device
    printf("\nChecking DMA devices:\n");
    const char *dma_paths[] = {
        "/dev/dma_heap/system",
        "/dev/dma_heap/system-uncached",
        "/dev/dma_heap/cma",
        NULL
    };

    for (const char **path = dma_paths; *path; path++) {
        printf("Trying %s: ", *path);
        fd = open(*path, O_RDWR);
        if (fd >= 0) {
            printf("SUCCESS\n");
            close(fd);
        } else {
            printf("FAILED (%s)\n", strerror(errno));
        }
    }

    // Check memory alignment
    printf("\nMemory Information:\n");
    printf("Source buffer address: %p\n", src_buf);
    printf("Destination buffer address: %p\n", dst_buf);
    printf("Page size: %d bytes\n", PAGE_SIZE);
    printf("Source buffer alignment: %s\n", 
           ((unsigned long)src_buf % PAGE_SIZE) == 0 ? "aligned" : "not aligned");
    printf("Destination buffer alignment: %s\n",
           ((unsigned long)dst_buf % PAGE_SIZE) == 0 ? "aligned" : "not aligned");

    // Check system DMA capabilities
    printf("\nSystem DMA Information:\n");
    system("ls -l /sys/class/dma");
    printf("\nDMA Channels:\n");
    system("ls -l /sys/class/dma/dma*");

    // Cleanup
    munmap(src_buf, DMA_BUF_SIZE);
    munmap(dst_buf, DMA_BUF_SIZE);

    return 0;
}
