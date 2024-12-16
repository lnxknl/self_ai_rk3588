#ifndef CPU_BENCH_H
#define CPU_BENCH_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <errno.h>
#include <math.h>

// RK3588 CPU configuration
#define NUM_A76_CORES     4
#define NUM_A55_CORES     4
#define TOTAL_CORES       (NUM_A76_CORES + NUM_A55_CORES)

// A76 cores are 4-7, A55 cores are 0-3 on RK3588
#define A76_CORE_START    4
#define A55_CORE_START    0

// Test configurations
#define TEST_DURATION_SEC 10
#define MATRIX_SIZE       1024
#define BUFFER_SIZE       (64 * 1024 * 1024)  // 64MB for memory test

typedef struct {
    int core_id;
    long long operations;
    double execution_time;
    double gflops;
    int test_type;
} ThreadData;

// Test types
enum {
    TEST_CPU_COMPUTE = 0,
    TEST_MEMORY_BANDWIDTH,
    TEST_CACHE_LATENCY
};

// Function declarations
void* cpu_compute_test(void* arg);
void* memory_bandwidth_test(void* arg);
void* cache_latency_test(void* arg);
int pin_thread_to_core(int core_id);
double get_time_in_seconds(void);
void run_benchmark(int test_type, const char* test_name);
void print_cpu_info(void);

#endif // CPU_BENCH_H
