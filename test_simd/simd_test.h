#ifndef SIMD_TEST_H
#define SIMD_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arm_neon.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

// Test vector sizes (reduced for more accurate timing)
#define VECTOR_SIZE (4096)  // 4K elements
#define TEST_ITERATIONS 1000000  // More iterations for better timing

// Test types
typedef enum {
    TEST_FLOAT_ADD = 0,
    TEST_FLOAT_MUL,
    TEST_FLOAT_FMA,
    TEST_INT_ADD,
    TEST_INT_MUL
} test_type_t;

typedef struct {
    double normal_time;
    double simd_time;
    double speedup;
    test_type_t test_type;
    int core_id;
} test_result_t;

// Function declarations
void* run_float_tests(void* arg);
void* run_int_tests(void* arg);
double get_time_seconds(void);
int pin_thread_to_core(int core_id);

// SIMD operation functions
void float_add_normal(float* a, float* b, float* c, int size);
void float_add_simd(float* a, float* b, float* c, int size);
void float_mul_normal(float* a, float* b, float* c, int size);
void float_mul_simd(float* a, float* b, float* c, int size);
void float_fma_normal(float* a, float* b, float* c, float* d, int size);
void float_fma_simd(float* a, float* b, float* c, float* d, int size);
void int_add_normal(int32_t* a, int32_t* b, int32_t* c, int size);
void int_add_simd(int32_t* a, int32_t* b, int32_t* c, int size);
void int_mul_normal(int32_t* a, int32_t* b, int32_t* c, int size);
void int_mul_simd(int32_t* a, int32_t* b, int32_t* c, int size);

#endif // SIMD_TEST_H
