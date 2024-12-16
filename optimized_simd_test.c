#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arm_neon.h>

#define TEST_SIZE 4096
#define ITERATIONS 10000
#define VECTOR_SIZE 4  // Process 4 elements per SIMD operation

static inline double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// Optimized SIMD implementation based on compiler hints
void float_add_simd(float* restrict a, float* restrict b, float* restrict c, int size) {
    // Process 4 elements at a time using NEON
    for (int i = 0; i < size; i += VECTOR_SIZE) {
        float32x4_t va = vld1q_f32(&a[i]);
        float32x4_t vb = vld1q_f32(&b[i]);
        float32x4_t vc = vaddq_f32(va, vb);
        vst1q_f32(&c[i], vc);
    }
}

// Auto-vectorized implementation
__attribute__((optimize("tree-vectorize")))
void float_add_auto(float* restrict a, float* restrict b, float* restrict c, int size) {
    #pragma GCC ivdep
    for (int i = 0; i < size; i++) {
        c[i] = a[i] + b[i];
    }
}

// Non-vectorized implementation
__attribute__((optimize("no-tree-vectorize")))
void float_add_scalar(float* restrict a, float* restrict b, float* restrict c, int size) {
    for (int i = 0; i < size; i++) {
        c[i] = a[i] + b[i];
    }
}

void benchmark_float_add() {
    printf("\nBenchmarking Float Addition:\n");
    printf("---------------------------\n");
    
    // Allocate aligned memory
    float* a = aligned_alloc(32, TEST_SIZE * sizeof(float));
    float* b = aligned_alloc(32, TEST_SIZE * sizeof(float));
    float* c_scalar = aligned_alloc(32, TEST_SIZE * sizeof(float));
    float* c_simd = aligned_alloc(32, TEST_SIZE * sizeof(float));
    float* c_auto = aligned_alloc(32, TEST_SIZE * sizeof(float));
    
    // Initialize data
    for (int i = 0; i < TEST_SIZE; i++) {
        a[i] = (float)i;
        b[i] = (float)(i * 2);
    }
    
    // Warm up the cache
    float_add_scalar(a, b, c_scalar, TEST_SIZE);
    float_add_simd(a, b, c_simd, TEST_SIZE);
    float_add_auto(a, b, c_auto, TEST_SIZE);
    
    // Benchmark scalar version
    double start = get_time();
    for (int i = 0; i < ITERATIONS; i++) {
        float_add_scalar(a, b, c_scalar, TEST_SIZE);
    }
    double scalar_time = get_time() - start;
    
    // Benchmark SIMD version
    start = get_time();
    for (int i = 0; i < ITERATIONS; i++) {
        float_add_simd(a, b, c_simd, TEST_SIZE);
    }
    double simd_time = get_time() - start;
    
    // Benchmark auto-vectorized version
    start = get_time();
    for (int i = 0; i < ITERATIONS; i++) {
        float_add_auto(a, b, c_auto, TEST_SIZE);
    }
    double auto_time = get_time() - start;
    
    // Verify results
    int simd_errors = 0, auto_errors = 0;
    for (int i = 0; i < TEST_SIZE; i++) {
        if (c_scalar[i] != c_simd[i]) simd_errors++;
        if (c_scalar[i] != c_auto[i]) auto_errors++;
    }
    
    printf("Scalar time:      %.3f ms\n", scalar_time * 1000.0);
    printf("Manual SIMD time: %.3f ms (Speedup: %.2fx, Errors: %d)\n", 
           simd_time * 1000.0, scalar_time/simd_time, simd_errors);
    printf("Auto-vec time:    %.3f ms (Speedup: %.2fx, Errors: %d)\n", 
           auto_time * 1000.0, scalar_time/auto_time, auto_errors);
    
    free(a);
    free(b);
    free(c_scalar);
    free(c_simd);
    free(c_auto);
}

int main() {
    printf("Optimized SIMD Test\n");
    printf("==================\n");
    printf("Compiler: GCC %d.%d.%d\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
    printf("Test size: %d elements\n", TEST_SIZE);
    printf("Iterations: %d\n", ITERATIONS);
    
    benchmark_float_add();
    return 0;
}
