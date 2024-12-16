#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arm_neon.h>

#define TEST_SIZE 1024
#define ITERATIONS 1000

static inline double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void test_float_add() {
    printf("\nTesting Float Addition:\n");
    
    // Allocate and initialize arrays
    float* a = aligned_alloc(16, TEST_SIZE * sizeof(float));
    float* b = aligned_alloc(16, TEST_SIZE * sizeof(float));
    float* c_normal = aligned_alloc(16, TEST_SIZE * sizeof(float));
    float* c_simd = aligned_alloc(16, TEST_SIZE * sizeof(float));
    
    // Initialize data
    for (int i = 0; i < TEST_SIZE; i++) {
        a[i] = (float)i;
        b[i] = (float)(i * 2);
    }
    
    // Normal addition
    double start = get_time();
    for (int iter = 0; iter < ITERATIONS; iter++) {
        for (int i = 0; i < TEST_SIZE; i++) {
            c_normal[i] = a[i] + b[i];
        }
    }
    double normal_time = get_time() - start;
    
    // NEON SIMD addition
    start = get_time();
    for (int iter = 0; iter < ITERATIONS; iter++) {
        for (int i = 0; i < TEST_SIZE; i += 4) {
            float32x4_t va = vld1q_f32(&a[i]);
            float32x4_t vb = vld1q_f32(&b[i]);
            float32x4_t vc = vaddq_f32(va, vb);
            vst1q_f32(&c_simd[i], vc);
        }
    }
    double simd_time = get_time() - start;
    
    // Verify results
    int errors = 0;
    for (int i = 0; i < TEST_SIZE; i++) {
        if (fabsf(c_normal[i] - c_simd[i]) > 1e-5) {
            errors++;
        }
    }
    
    printf("Normal time: %.3f ms\n", normal_time * 1000.0);
    printf("SIMD time:   %.3f ms\n", simd_time * 1000.0);
    printf("Speedup:     %.2fx\n", normal_time / simd_time);
    printf("Errors:      %d\n", errors);
    
    free(a);
    free(b);
    free(c_normal);
    free(c_simd);
}

void test_int_add() {
    printf("\nTesting Integer Addition:\n");
    
    int32_t* a = aligned_alloc(16, TEST_SIZE * sizeof(int32_t));
    int32_t* b = aligned_alloc(16, TEST_SIZE * sizeof(int32_t));
    int32_t* c_normal = aligned_alloc(16, TEST_SIZE * sizeof(int32_t));
    int32_t* c_simd = aligned_alloc(16, TEST_SIZE * sizeof(int32_t));
    
    // Initialize data
    for (int i = 0; i < TEST_SIZE; i++) {
        a[i] = i;
        b[i] = i * 2;
    }
    
    // Normal addition
    double start = get_time();
    for (int iter = 0; iter < ITERATIONS; iter++) {
        for (int i = 0; i < TEST_SIZE; i++) {
            c_normal[i] = a[i] + b[i];
        }
    }
    double normal_time = get_time() - start;
    
    // NEON SIMD addition
    start = get_time();
    for (int iter = 0; iter < ITERATIONS; iter++) {
        for (int i = 0; i < TEST_SIZE; i += 4) {
            int32x4_t va = vld1q_s32(&a[i]);
            int32x4_t vb = vld1q_s32(&b[i]);
            int32x4_t vc = vaddq_s32(va, vb);
            vst1q_s32(&c_simd[i], vc);
        }
    }
    double simd_time = get_time() - start;
    
    // Verify results
    int errors = 0;
    for (int i = 0; i < TEST_SIZE; i++) {
        if (c_normal[i] != c_simd[i]) {
            errors++;
        }
    }
    
    printf("Normal time: %.3f ms\n", normal_time * 1000.0);
    printf("SIMD time:   %.3f ms\n", simd_time * 1000.0);
    printf("Speedup:     %.2fx\n", normal_time / simd_time);
    printf("Errors:      %d\n", errors);
    
    free(a);
    free(b);
    free(c_normal);
    free(c_simd);
}

int main() {
    printf("Simple NEON SIMD Test\n");
    printf("====================\n");
    
    // Print CPU info
    FILE* fp = fopen("/proc/cpuinfo", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "model name", 10) == 0) {
                printf("CPU: %s", line + 12);
                break;
            }
        }
        fclose(fp);
    }
    
    test_float_add();
    test_int_add();
    
    return 0;
}
