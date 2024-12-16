#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <pthread.h>
#include <unistd.h>

// Cache sizes for RK3588
#define A55_L1I_SIZE (32 * 1024)      // 32KB L1 I-cache
#define A55_L1D_SIZE (32 * 1024)      // 32KB L1 D-cache
#define A55_L2_SIZE  (256 * 1024)     // 256KB L2 cache
#define A76_L1I_SIZE (64 * 1024)      // 64KB L1 I-cache
#define A76_L1D_SIZE (64 * 1024)      // 64KB L1 D-cache
#define A76_L2_SIZE  (512 * 1024)     // 512KB L2 cache
#define L3_SIZE      (4 * 1024 * 1024) // 4MB L3 cache

// Test parameters
#define NUM_ITERATIONS 1000000
#define CACHE_LINE_SIZE 64
#define MAX_ARRAY_SIZE (8 * 1024 * 1024) // 8MB (larger than L3)

static inline double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// Pin thread to specific CPU core
int pin_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_t current_thread = pthread_self();
    return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

// Sequential access test
double test_sequential_access(char* array, size_t size) {
    double start = get_time();
    volatile char sum = 0;  // Prevent optimization
    
    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        for (size_t i = 0; i < size; i += CACHE_LINE_SIZE) {
            sum += array[i];
        }
    }
    
    return (get_time() - start) / NUM_ITERATIONS;
}

// Random access test
double test_random_access(char* array, size_t size, size_t* indices, size_t num_indices) {
    double start = get_time();
    volatile char sum = 0;  // Prevent optimization
    
    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        for (size_t i = 0; i < num_indices; i++) {
            sum += array[indices[i]];
        }
    }
    
    return (get_time() - start) / NUM_ITERATIONS;
}

void test_cache_level(const char* desc, size_t size, int core) {
    printf("\nTesting %s (Size: %zu KB) on Core %d\n", desc, size/1024, core);
    printf("----------------------------------------\n");
    
    // Pin to specified core
    if (pin_to_core(core) != 0) {
        printf("Failed to pin to core %d\n", core);
        return;
    }
    
    // Allocate and initialize test array
    char* array = aligned_alloc(CACHE_LINE_SIZE, size);
    if (!array) {
        printf("Memory allocation failed!\n");
        return;
    }
    
    // Initialize with random data
    for (size_t i = 0; i < size; i++) {
        array[i] = (char)rand();
    }
    
    // Create random access pattern
    size_t num_indices = size / CACHE_LINE_SIZE;
    size_t* indices = malloc(num_indices * sizeof(size_t));
    for (size_t i = 0; i < num_indices; i++) {
        indices[i] = (i * CACHE_LINE_SIZE) % size;
    }
    // Shuffle indices for random access
    for (size_t i = num_indices - 1; i > 0; i--) {
        size_t j = rand() % (i + 1);
        size_t temp = indices[i];
        indices[i] = indices[j];
        indices[j] = temp;
    }
    
    // Run tests
    double seq_time = test_sequential_access(array, size);
    double rand_time = test_random_access(array, size, indices, num_indices);
    
    // Calculate bandwidth (MB/s)
    double seq_bandwidth = (size / (1024.0 * 1024.0)) / seq_time;
    double rand_bandwidth = (num_indices * CACHE_LINE_SIZE / (1024.0 * 1024.0)) / rand_time;
    
    printf("Sequential Access: %.2f ns/access (%.2f MB/s)\n", 
           seq_time * 1e9 / (size / CACHE_LINE_SIZE), seq_bandwidth);
    printf("Random Access:    %.2f ns/access (%.2f MB/s)\n", 
           rand_time * 1e9 / num_indices, rand_bandwidth);
    
    free(array);
    free(indices);
}

int main() {
    srand(time(NULL));
    
    printf("RK3588 Cache Performance Test\n");
    printf("============================\n");
    
    // Test on A55 core (core 0)
    printf("\nTesting on A55 Core (Core 0):\n");
    test_cache_level("A55 L1D Cache", A55_L1D_SIZE, 0);
    test_cache_level("A55 L2 Cache", A55_L2_SIZE, 0);
    test_cache_level("L3 Cache", L3_SIZE, 0);
    test_cache_level("Main Memory", MAX_ARRAY_SIZE, 0);
    
    // Test on A76 core (core 4)
    printf("\nTesting on A76 Core (Core 4):\n");
    test_cache_level("A76 L1D Cache", A76_L1D_SIZE, 4);
    test_cache_level("A76 L2 Cache", A76_L2_SIZE, 4);
    test_cache_level("L3 Cache", L3_SIZE, 4);
    test_cache_level("Main Memory", MAX_ARRAY_SIZE, 4);
    
    return 0;
}
