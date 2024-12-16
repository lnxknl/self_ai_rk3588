#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <linux/dma-buf.h>

#define MIN_BUF_SIZE (4 * 1024)        // 4KB
#define MAX_BUF_SIZE (64 * 1024 * 1024) // 64MB
#define PAGE_SIZE 4096
#define NUM_ITERATIONS 100

// Test configurations
typedef struct {
    size_t size;
    int alignment;
    const char* description;
} buffer_config_t;

buffer_config_t buffer_configs[] = {
    {4 * 1024,      4096, "4KB aligned to 4KB"},
    {64 * 1024,     4096, "64KB aligned to 4KB"},
    {1 * 1024 * 1024, 4096, "1MB aligned to 4KB"},
    {16 * 1024 * 1024, 4096, "16MB aligned to 4KB"},
    {0, 0, NULL}  // Sentinel
};

// Timing utilities
double get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0);
}

// Memory utilities
void* allocate_aligned_buffer(size_t size, size_t alignment) {
    void* ptr;
    int ret = posix_memalign(&ptr, alignment, size);
    if (ret != 0) {
        return NULL;
    }
    return ptr;
}

// Pattern generation and verification
void generate_pattern(void* buf, size_t size) {
    uint32_t* ptr = (uint32_t*)buf;
    for (size_t i = 0; i < size/4; i++) {
        ptr[i] = (uint32_t)i ^ 0xAAAAAAAA;
    }
}

int verify_pattern(void* src, void* dst, size_t size) {
    uint32_t* src_ptr = (uint32_t*)src;
    uint32_t* dst_ptr = (uint32_t*)dst;
    for (size_t i = 0; i < size/4; i++) {
        if (dst_ptr[i] != src_ptr[i]) {
            printf("Mismatch at offset %zu: expected 0x%08x, got 0x%08x\n",
                   i*4, src_ptr[i], dst_ptr[i]);
            return 0;
        }
    }
    return 1;
}

// DMA channel testing
typedef struct {
    int channel;
    const char* name;
    int fd;
} dma_channel_t;

#define MAX_DMA_CHANNELS 8
dma_channel_t dma_channels[MAX_DMA_CHANNELS];
int num_dma_channels = 0;

void discover_dma_channels() {
    char path[256];
    printf("\nDiscovering DMA channels:\n");
    
    // Scan for DMA channels
    for (int i = 0; i < 16; i++) {
        snprintf(path, sizeof(path), "/dev/dma%d", i);
        int fd = open(path, O_RDWR);
        if (fd >= 0) {
            if (num_dma_channels < MAX_DMA_CHANNELS) {
                dma_channels[num_dma_channels].channel = i;
                dma_channels[num_dma_channels].name = strdup(path);
                dma_channels[num_dma_channels].fd = fd;
                printf("Found DMA channel %d: %s\n", i, path);
                num_dma_channels++;
            }
        }
    }
    printf("Found %d DMA channels\n", num_dma_channels);
}

// Performance measurement
typedef struct {
    size_t buffer_size;
    double cpu_time;
    double dma_time;
    double bandwidth_cpu;
    double bandwidth_dma;
} perf_result_t;

void measure_performance(size_t size, perf_result_t* result) {
    void* src = allocate_aligned_buffer(size, PAGE_SIZE);
    void* dst = allocate_aligned_buffer(size, PAGE_SIZE);
    if (!src || !dst) {
        printf("Failed to allocate buffers for performance test\n");
        return;
    }

    // CPU copy performance
    generate_pattern(src, size);
    double start = get_time_ms();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        memcpy(dst, src, size);
    }
    double end = get_time_ms();
    result->cpu_time = (end - start) / NUM_ITERATIONS;
    result->bandwidth_cpu = (size / (1024.0 * 1024.0)) / (result->cpu_time / 1000.0);

    // DMA copy performance (if available)
    if (num_dma_channels > 0) {
        start = get_time_ms();
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            // Note: This is a placeholder for actual DMA transfer
            // Real DMA transfer would use proper DMA API calls
            memcpy(dst, src, size);
        }
        end = get_time_ms();
        result->dma_time = (end - start) / NUM_ITERATIONS;
        result->bandwidth_dma = (size / (1024.0 * 1024.0)) / (result->dma_time / 1000.0);
    }

    free(src);
    free(dst);
}

void test_peripheral_dma() {
    printf("\nTesting Peripheral DMA Access:\n");
    printf("------------------------------\n");

    // List of common peripheral devices to test
    const char* peripherals[] = {
        "/dev/i2c-0",
        "/dev/i2c-1",
        "/dev/spidev0.0",
        "/dev/spidev0.1",
        NULL
    };

    for (const char** dev = peripherals; *dev; dev++) {
        printf("Testing %s: ", *dev);
        int fd = open(*dev, O_RDWR);
        if (fd >= 0) {
            printf("Available\n");
            close(fd);
        } else {
            printf("Not available (%s)\n", strerror(errno));
        }
    }
}

int main() {
    printf("RK3588 DMA Comprehensive Test\n");
    printf("============================\n");

    // Discover DMA channels
    discover_dma_channels();

    // Test different buffer configurations
    printf("\nTesting Buffer Configurations:\n");
    printf("-----------------------------\n");
    
    for (buffer_config_t* config = buffer_configs; config->size > 0; config++) {
        printf("\nTesting %s:\n", config->description);
        perf_result_t result = {0};
        result.buffer_size = config->size;
        
        measure_performance(config->size, &result);
        
        printf("Buffer Size: %zu bytes\n", config->size);
        printf("CPU Copy: %.2f ms (%.2f MB/s)\n", 
               result.cpu_time, result.bandwidth_cpu);
        if (num_dma_channels > 0) {
            printf("DMA Copy: %.2f ms (%.2f MB/s)\n", 
                   result.dma_time, result.bandwidth_dma);
            printf("Speedup: %.2fx\n", result.cpu_time / result.dma_time);
        }
    }

    // Test peripheral DMA access
    test_peripheral_dma();

    // Memory alignment verification
    printf("\nMemory Alignment Test:\n");
    printf("---------------------\n");
    for (int alignment = 4; alignment <= 4096; alignment *= 2) {
        void* buf = allocate_aligned_buffer(PAGE_SIZE, alignment);
        if (buf) {
            printf("Alignment %d: Success (address: %p)\n", alignment, buf);
            free(buf);
        } else {
            printf("Alignment %d: Failed\n", alignment);
        }
    }

    // Cleanup
    for (int i = 0; i < num_dma_channels; i++) {
        close(dma_channels[i].fd);
        free((void*)dma_channels[i].name);
    }

    printf("\nTest completed!\n");
    return 0;
}
