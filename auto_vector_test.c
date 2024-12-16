#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TEST_SIZE 1024
#define ITERATIONS 1000

static inline double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// Force compiler to not vectorize this version
__attribute__((optimize("no-tree-vectorize")))
void float_add_scalar(float* restrict a, float* restrict b, float* restrict c, int size) {
    for (int i = 0; i < size; i++) {
        c[i] = a[i] + b[i];
    }
}

// Allow compiler to auto-vectorize this version
__attribute__((optimize("tree-vectorize")))
void float_add_vector(float* restrict a, float* restrict b, float* restrict c, int size) {
    #pragma GCC ivdep
    for (int i = 0; i < size; i++) {
        c[i] = a[i] + b[i];
    }
}

void test_float_add() {
    printf("\nTesting Float Addition:\n");
    
    // Allocate aligned memory
    float* a = aligned_alloc(32, TEST_SIZE * sizeof(float));
    float* b = aligned_alloc(32, TEST_SIZE * sizeof(float));
    float* c_scalar = aligned_alloc(32, TEST_SIZE * sizeof(float));
    float* c_vector = aligned_alloc(32, TEST_SIZE * sizeof(float));
    
    // Initialize data
    for (int i = 0; i < TEST_SIZE; i++) {
        a[i] = (float)i;
        b[i] = (float)(i * 2);
    }
    
    // Scalar addition
    double start = get_time();
    for (int iter = 0; iter < ITERATIONS; iter++) {
        float_add_scalar(a, b, c_scalar, TEST_SIZE);
    }
    double scalar_time = get_time() - start;
    
    // Vector addition
    start = get_time();
    for (int iter = 0; iter < ITERATIONS; iter++) {
        float_add_vector(a, b, c_vector, TEST_SIZE);
    }
    double vector_time = get_time() - start;
    
    // Verify results
    int errors = 0;
    for (int i = 0; i < TEST_SIZE; i++) {
        if (c_scalar[i] != c_vector[i]) {
            errors++;
            if (errors < 5) {
                printf("Error at index %d: scalar=%.6f vector=%.6f\n", 
                       i, c_scalar[i], c_vector[i]);
            }
        }
    }
    
    printf("Scalar time: %.3f ms\n", scalar_time * 1000.0);
    printf("Vector time: %.3f ms\n", vector_time * 1000.0);
    printf("Speedup:     %.2fx\n", scalar_time / vector_time);
    printf("Errors:      %d\n", errors);
    
    free(a);
    free(b);
    free(c_scalar);
    free(c_vector);
}

int main() {
    printf("Auto-vectorization Test\n");
    printf("======================\n");
    
    // Print compiler version and flags
    printf("Compiler: GCC %d.%d.%d\n", 
           __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
    
    test_float_add();
    return 0;
}
