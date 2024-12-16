#include "cpu_bench.h"
#include <math.h>

// Get CPU frequency from sysfs
static unsigned long get_cpu_freq_khz(int cpu) {
    char path[256];
    FILE *f;
    unsigned long freq = 0;
    
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", cpu);
    f = fopen(path, "r");
    if (f) {
        fscanf(f, "%lu", &freq);
        fclose(f);
    }
    return freq;
}

void print_cpu_info(void) {
    printf("\nCPU Information:\n");
    printf("---------------\n");
    
    // Print A76 cores info
    for (int i = A76_CORE_START; i < A76_CORE_START + NUM_A76_CORES; i++) {
        unsigned long freq = get_cpu_freq_khz(i);
        printf("Cortex-A76 Core %d: %.2f GHz\n", i, freq / 1000000.0);
    }
    
    // Print A55 cores info
    for (int i = A55_CORE_START; i < A55_CORE_START + NUM_A55_CORES; i++) {
        unsigned long freq = get_cpu_freq_khz(i);
        printf("Cortex-A55 Core %d: %.2f GHz\n", i, freq / 1000000.0);
    }
    printf("\n");
}

int pin_thread_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    
    pthread_t current_thread = pthread_self();
    return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

double get_time_in_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

// CPU Compute Test: Matrix multiplication
void* cpu_compute_test(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    if (pin_thread_to_core(data->core_id) != 0) {
        printf("Failed to pin thread to core %d\n", data->core_id);
        return NULL;
    }
    
    float* matrix_a = (float*)malloc(MATRIX_SIZE * MATRIX_SIZE * sizeof(float));
    float* matrix_b = (float*)malloc(MATRIX_SIZE * MATRIX_SIZE * sizeof(float));
    float* matrix_c = (float*)malloc(MATRIX_SIZE * MATRIX_SIZE * sizeof(float));
    
    // Initialize matrices
    for (int i = 0; i < MATRIX_SIZE * MATRIX_SIZE; i++) {
        matrix_a[i] = (float)rand() / RAND_MAX;
        matrix_b[i] = (float)rand() / RAND_MAX;
        matrix_c[i] = 0.0f;
    }
    
    double start_time = get_time_in_seconds();
    data->operations = 0;
    
    while (get_time_in_seconds() - start_time < TEST_DURATION_SEC) {
        // Perform matrix multiplication
        for (int i = 0; i < MATRIX_SIZE; i++) {
            for (int j = 0; j < MATRIX_SIZE; j++) {
                float sum = 0.0f;
                for (int k = 0; k < MATRIX_SIZE; k++) {
                    sum += matrix_a[i * MATRIX_SIZE + k] * matrix_b[k * MATRIX_SIZE + j];
                }
                matrix_c[i * MATRIX_SIZE + j] = sum;
            }
        }
        data->operations++;
    }
    
    data->execution_time = get_time_in_seconds() - start_time;
    // Calculate GFLOPS: 2 * N^3 operations per matrix multiplication
    data->gflops = (2.0 * MATRIX_SIZE * MATRIX_SIZE * MATRIX_SIZE * data->operations) / 
                   (data->execution_time * 1000000000.0);
    
    free(matrix_a);
    free(matrix_b);
    free(matrix_c);
    return NULL;
}

// Memory Bandwidth Test
void* memory_bandwidth_test(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    if (pin_thread_to_core(data->core_id) != 0) {
        printf("Failed to pin thread to core %d\n", data->core_id);
        return NULL;
    }
    
    char* buffer = (char*)malloc(BUFFER_SIZE);
    char* dest = (char*)malloc(BUFFER_SIZE);
    
    double start_time = get_time_in_seconds();
    data->operations = 0;
    
    while (get_time_in_seconds() - start_time < TEST_DURATION_SEC) {
        memcpy(dest, buffer, BUFFER_SIZE);
        data->operations += BUFFER_SIZE;
    }
    
    data->execution_time = get_time_in_seconds() - start_time;
    // Calculate bandwidth in GB/s
    data->gflops = (data->operations) / (data->execution_time * 1024 * 1024 * 1024);
    
    free(buffer);
    free(dest);
    return NULL;
}

// Cache Latency Test
void* cache_latency_test(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    if (pin_thread_to_core(data->core_id) != 0) {
        printf("Failed to pin thread to core %d\n", data->core_id);
        return NULL;
    }
    
    const int array_size = 64 * 1024 * 1024; // 64MB
    int* array = (int*)malloc(array_size * sizeof(int));
    int stride = 64 / sizeof(int); // Cache line size
    
    // Initialize array with pointer chasing pattern
    for (int i = 0; i < array_size - stride; i += stride) {
        array[i] = (i + stride) % array_size;
    }
    
    double start_time = get_time_in_seconds();
    data->operations = 0;
    int index = 0;
    
    while (get_time_in_seconds() - start_time < TEST_DURATION_SEC) {
        for (int i = 0; i < 1000000; i++) {
            index = array[index];
        }
        data->operations += 1000000;
    }
    
    data->execution_time = get_time_in_seconds() - start_time;
    // Calculate average latency in nanoseconds
    data->gflops = (data->execution_time * 1000000000.0) / data->operations;
    
    free(array);
    return NULL;
}

void run_benchmark(int test_type, const char* test_name) {
    pthread_t threads[TOTAL_CORES];
    ThreadData thread_data[TOTAL_CORES];
    
    printf("\nRunning %s:\n", test_name);
    printf("----------------------------------------\n");
    
    // Create threads for all cores
    for (int i = 0; i < TOTAL_CORES; i++) {
        thread_data[i].core_id = i;
        thread_data[i].test_type = test_type;
        
        void* (*test_function)(void*);
        switch (test_type) {
            case TEST_CPU_COMPUTE:
                test_function = cpu_compute_test;
                break;
            case TEST_MEMORY_BANDWIDTH:
                test_function = memory_bandwidth_test;
                break;
            case TEST_CACHE_LATENCY:
                test_function = cache_latency_test;
                break;
            default:
                printf("Unknown test type\n");
                return;
        }
        
        pthread_create(&threads[i], NULL, test_function, &thread_data[i]);
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < TOTAL_CORES; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Print results
    printf("\nResults:\n");
    
    // A76 cores results
    printf("\nCortex-A76 Cores:\n");
    double a76_total = 0;
    for (int i = A76_CORE_START; i < A76_CORE_START + NUM_A76_CORES; i++) {
        switch (test_type) {
            case TEST_CPU_COMPUTE:
                printf("Core %d: %.2f GFLOPS\n", i, thread_data[i].gflops);
                break;
            case TEST_MEMORY_BANDWIDTH:
                printf("Core %d: %.2f GB/s\n", i, thread_data[i].gflops);
                break;
            case TEST_CACHE_LATENCY:
                printf("Core %d: %.2f ns average latency\n", i, thread_data[i].gflops);
                break;
        }
        a76_total += thread_data[i].gflops;
    }
    
    // A55 cores results
    printf("\nCortex-A55 Cores:\n");
    double a55_total = 0;
    for (int i = A55_CORE_START; i < A55_CORE_START + NUM_A55_CORES; i++) {
        switch (test_type) {
            case TEST_CPU_COMPUTE:
                printf("Core %d: %.2f GFLOPS\n", i, thread_data[i].gflops);
                break;
            case TEST_MEMORY_BANDWIDTH:
                printf("Core %d: %.2f GB/s\n", i, thread_data[i].gflops);
                break;
            case TEST_CACHE_LATENCY:
                printf("Core %d: %.2f ns average latency\n", i, thread_data[i].gflops);
                break;
        }
        a55_total += thread_data[i].gflops;
    }
    
    // Print averages
    printf("\nAverages:\n");
    printf("A76 Cores: %.2f %s\n", a76_total / NUM_A76_CORES, 
           test_type == TEST_CPU_COMPUTE ? "GFLOPS" : 
           test_type == TEST_MEMORY_BANDWIDTH ? "GB/s" : "ns latency");
    printf("A55 Cores: %.2f %s\n", a55_total / NUM_A55_CORES,
           test_type == TEST_CPU_COMPUTE ? "GFLOPS" : 
           test_type == TEST_MEMORY_BANDWIDTH ? "GB/s" : "ns latency");
}

int main(int argc, char** argv) {
    print_cpu_info();
    
    printf("Starting CPU benchmark suite for RK3588...\n");
    printf("Testing both Cortex-A76 and Cortex-A55 cores\n\n");
    
    // Run all benchmarks
    run_benchmark(TEST_CPU_COMPUTE, "CPU Compute Test (Matrix Multiplication)");
    run_benchmark(TEST_MEMORY_BANDWIDTH, "Memory Bandwidth Test");
    run_benchmark(TEST_CACHE_LATENCY, "Cache Latency Test");
    
    return 0;
}
