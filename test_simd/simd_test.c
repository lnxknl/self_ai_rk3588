#define _GNU_SOURCE
#include "simd_test.h"

double get_time_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

int pin_thread_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

// Regular floating-point addition
void float_add_normal(float* a, float* b, float* c, int size) {
    for (int i = 0; i < size; i++) {
        c[i] = a[i] + b[i];
    }
}

// NEON SIMD floating-point addition
void float_add_simd(float* a, float* b, float* c, int size) {
    int i;
    for (i = 0; i <= size - 4; i += 4) {
        float32x4_t va = vld1q_f32(&a[i]);
        float32x4_t vb = vld1q_f32(&b[i]);
        float32x4_t vc = vaddq_f32(va, vb);
        vst1q_f32(&c[i], vc);
    }
    // Handle remaining elements
    for (; i < size; i++) {
        c[i] = a[i] + b[i];
    }
}

// Regular floating-point multiplication
void float_mul_normal(float* a, float* b, float* c, int size) {
    for (int i = 0; i < size; i++) {
        c[i] = a[i] * b[i];
    }
}

// NEON SIMD floating-point multiplication
void float_mul_simd(float* a, float* b, float* c, int size) {
    int i;
    for (i = 0; i <= size - 4; i += 4) {
        float32x4_t va = vld1q_f32(&a[i]);
        float32x4_t vb = vld1q_f32(&b[i]);
        float32x4_t vc = vmulq_f32(va, vb);
        vst1q_f32(&c[i], vc);
    }
    for (; i < size; i++) {
        c[i] = a[i] * b[i];
    }
}

// Regular FMA (Fused Multiply-Add)
void float_fma_normal(float* a, float* b, float* c, float* d, int size) {
    for (int i = 0; i < size; i++) {
        d[i] = a[i] * b[i] + c[i];
    }
}

// NEON SIMD FMA
void float_fma_simd(float* a, float* b, float* c, float* d, int size) {
    int i;
    for (i = 0; i <= size - 4; i += 4) {
        float32x4_t va = vld1q_f32(&a[i]);
        float32x4_t vb = vld1q_f32(&b[i]);
        float32x4_t vc = vld1q_f32(&c[i]);
        float32x4_t vd = vfmaq_f32(vc, va, vb);  // d = a * b + c
        vst1q_f32(&d[i], vd);
    }
    for (; i < size; i++) {
        d[i] = a[i] * b[i] + c[i];
    }
}

// Regular integer addition
void int_add_normal(int32_t* a, int32_t* b, int32_t* c, int size) {
    for (int i = 0; i < size; i++) {
        c[i] = a[i] + b[i];
    }
}

// NEON SIMD integer addition
void int_add_simd(int32_t* a, int32_t* b, int32_t* c, int size) {
    int i;
    for (i = 0; i <= size - 4; i += 4) {
        int32x4_t va = vld1q_s32(&a[i]);
        int32x4_t vb = vld1q_s32(&b[i]);
        int32x4_t vc = vaddq_s32(va, vb);
        vst1q_s32(&c[i], vc);
    }
    for (; i < size; i++) {
        c[i] = a[i] + b[i];
    }
}

// Regular integer multiplication
void int_mul_normal(int32_t* a, int32_t* b, int32_t* c, int size) {
    for (int i = 0; i < size; i++) {
        c[i] = a[i] * b[i];
    }
}

// NEON SIMD integer multiplication
void int_mul_simd(int32_t* a, int32_t* b, int32_t* c, int size) {
    int i;
    for (i = 0; i <= size - 4; i += 4) {
        int32x4_t va = vld1q_s32(&a[i]);
        int32x4_t vb = vld1q_s32(&b[i]);
        int32x4_t vc = vmulq_s32(va, vb);
        vst1q_s32(&c[i], vc);
    }
    for (; i < size; i++) {
        c[i] = a[i] * b[i];
    }
}

void run_float_benchmark(test_result_t* result, int core_id) {
    float *a, *b, *c, *d;
    double start_time, end_time;

    // Allocate aligned memory for better SIMD performance
    posix_memalign((void**)&a, 16, VECTOR_SIZE * sizeof(float));
    posix_memalign((void**)&b, 16, VECTOR_SIZE * sizeof(float));
    posix_memalign((void**)&c, 16, VECTOR_SIZE * sizeof(float));
    posix_memalign((void**)&d, 16, VECTOR_SIZE * sizeof(float));

    // Initialize arrays with random data
    for (int i = 0; i < VECTOR_SIZE; i++) {
        a[i] = (float)rand() / RAND_MAX;
        b[i] = (float)rand() / RAND_MAX;
        c[i] = (float)rand() / RAND_MAX;
    }

    // Test floating-point addition
    start_time = get_time_seconds();
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        float_add_normal(a, b, c, VECTOR_SIZE);
    }
    end_time = get_time_seconds();
    result[TEST_FLOAT_ADD].normal_time = end_time - start_time;

    start_time = get_time_seconds();
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        float_add_simd(a, b, c, VECTOR_SIZE);
    }
    end_time = get_time_seconds();
    result[TEST_FLOAT_ADD].simd_time = end_time - start_time;
    result[TEST_FLOAT_ADD].speedup = result[TEST_FLOAT_ADD].normal_time / result[TEST_FLOAT_ADD].simd_time;
    result[TEST_FLOAT_ADD].test_type = TEST_FLOAT_ADD;
    result[TEST_FLOAT_ADD].core_id = core_id;

    // Test floating-point multiplication
    start_time = get_time_seconds();
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        float_mul_normal(a, b, c, VECTOR_SIZE);
    }
    end_time = get_time_seconds();
    result[TEST_FLOAT_MUL].normal_time = end_time - start_time;

    start_time = get_time_seconds();
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        float_mul_simd(a, b, c, VECTOR_SIZE);
    }
    end_time = get_time_seconds();
    result[TEST_FLOAT_MUL].simd_time = end_time - start_time;
    result[TEST_FLOAT_MUL].speedup = result[TEST_FLOAT_MUL].normal_time / result[TEST_FLOAT_MUL].simd_time;
    result[TEST_FLOAT_MUL].test_type = TEST_FLOAT_MUL;
    result[TEST_FLOAT_MUL].core_id = core_id;

    // Test FMA
    start_time = get_time_seconds();
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        float_fma_normal(a, b, c, d, VECTOR_SIZE);
    }
    end_time = get_time_seconds();
    result[TEST_FLOAT_FMA].normal_time = end_time - start_time;

    start_time = get_time_seconds();
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        float_fma_simd(a, b, c, d, VECTOR_SIZE);
    }
    end_time = get_time_seconds();
    result[TEST_FLOAT_FMA].simd_time = end_time - start_time;
    result[TEST_FLOAT_FMA].speedup = result[TEST_FLOAT_FMA].normal_time / result[TEST_FLOAT_FMA].simd_time;
    result[TEST_FLOAT_FMA].test_type = TEST_FLOAT_FMA;
    result[TEST_FLOAT_FMA].core_id = core_id;

    free(a);
    free(b);
    free(c);
    free(d);
}

void run_int_benchmark(test_result_t* result, int core_id) {
    int32_t *a, *b, *c;
    double start_time, end_time;

    posix_memalign((void**)&a, 16, VECTOR_SIZE * sizeof(int32_t));
    posix_memalign((void**)&b, 16, VECTOR_SIZE * sizeof(int32_t));
    posix_memalign((void**)&c, 16, VECTOR_SIZE * sizeof(int32_t));

    // Initialize arrays with random data
    for (int i = 0; i < VECTOR_SIZE; i++) {
        a[i] = rand() % 1000;
        b[i] = rand() % 1000;
    }

    // Test integer addition
    start_time = get_time_seconds();
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        int_add_normal(a, b, c, VECTOR_SIZE);
    }
    end_time = get_time_seconds();
    result[TEST_INT_ADD].normal_time = end_time - start_time;

    start_time = get_time_seconds();
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        int_add_simd(a, b, c, VECTOR_SIZE);
    }
    end_time = get_time_seconds();
    result[TEST_INT_ADD].simd_time = end_time - start_time;
    result[TEST_INT_ADD].speedup = result[TEST_INT_ADD].normal_time / result[TEST_INT_ADD].simd_time;
    result[TEST_INT_ADD].test_type = TEST_INT_ADD;
    result[TEST_INT_ADD].core_id = core_id;

    // Test integer multiplication
    start_time = get_time_seconds();
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        int_mul_normal(a, b, c, VECTOR_SIZE);
    }
    end_time = get_time_seconds();
    result[TEST_INT_MUL].normal_time = end_time - start_time;

    start_time = get_time_seconds();
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        int_mul_simd(a, b, c, VECTOR_SIZE);
    }
    end_time = get_time_seconds();
    result[TEST_INT_MUL].simd_time = end_time - start_time;
    result[TEST_INT_MUL].speedup = result[TEST_INT_MUL].normal_time / result[TEST_INT_MUL].simd_time;
    result[TEST_INT_MUL].test_type = TEST_INT_MUL;
    result[TEST_INT_MUL].core_id = core_id;

    free(a);
    free(b);
    free(c);
}

int main() {
    const int num_cores = 8;  // RK3588 has 8 cores total
    test_result_t results[num_cores][5];  // 5 test types per core
    pthread_t threads[num_cores];
    
    printf("Starting SIMD and FPU benchmark on RK3588...\n");
    printf("Testing both Cortex-A76 and Cortex-A55 cores\n\n");

    // Run tests on each core
    for (int core = 0; core < num_cores; core++) {
        if (pin_thread_to_core(core) != 0) {
            printf("Failed to pin thread to core %d\n", core);
            continue;
        }

        run_float_benchmark(&results[core][0], core);
        run_int_benchmark(&results[core][3], core);  // Offset for integer tests

        printf("\nCore %d Results (Cortex-%s):\n", core, core >= 4 ? "A76" : "A55");
        printf("----------------------------------------\n");
        
        const char* test_names[] = {
            "Float Add", "Float Multiply", "Float FMA",
            "Integer Add", "Integer Multiply"
        };

        for (int test = 0; test < 5; test++) {
            printf("%s:\n", test_names[test]);
            printf("  Normal: %.3f ms\n", results[core][test].normal_time * 1000.0);
            printf("  SIMD:   %.3f ms\n", results[core][test].simd_time * 1000.0);
            printf("  Speedup: %.2fx\n", results[core][test].speedup);
        }
        printf("\n");
    }

    // Print summary
    printf("\nSummary of SIMD Speedups:\n");
    printf("----------------------------------------\n");
    printf("Operation    | A76 Avg | A55 Avg\n");
    printf("----------------------------------------\n");
    
    for (int test = 0; test < 5; test++) {
        double a76_speedup = 0, a55_speedup = 0;
        
        // Calculate averages
        for (int core = 0; core < 4; core++) {
            a55_speedup += results[core][test].speedup;
        }
        for (int core = 4; core < 8; core++) {
            a76_speedup += results[core][test].speedup;
        }
        
        a55_speedup /= 4;
        a76_speedup /= 4;
        
        const char* test_names[] = {
            "Float Add   ", "Float Mul  ", "Float FMA  ",
            "Int Add    ", "Int Mul    "
        };
        
        printf("%s | %.2fx   | %.2fx\n", 
               test_names[test], a76_speedup, a55_speedup);
    }
    
    return 0;
}
