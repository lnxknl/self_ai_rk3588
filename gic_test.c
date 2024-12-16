#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <linux/sched.h>

// RK3588 has 8 cores (4xA76 + 4xA55)
#define NUM_CORES 8
#define A55_CORE_START 0
#define A76_CORE_START 4

// Shared data structure for inter-core communication
typedef struct {
    volatile int interrupt_count[NUM_CORES];
    volatile int core_active[NUM_CORES];
    pthread_mutex_t mutex;
} shared_data_t;

shared_data_t *shared_data;

// Signal handler for our IPI simulation
void signal_handler(int signo, siginfo_t *info, void *context) {
    int core_id = sched_getcpu();
    
    if (signo == SIGUSR1) {
        pthread_mutex_lock(&shared_data->mutex);
        shared_data->interrupt_count[core_id]++;
        printf("Core %d received IPI #%d\n", 
               core_id, shared_data->interrupt_count[core_id]);
        pthread_mutex_unlock(&shared_data->mutex);
    }
}

// Set up signal handling for IPI simulation
void setup_signal_handling() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGUSR1, &sa, NULL);
}

// Pin thread to specific CPU core
int pin_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_t current_thread = pthread_self();
    return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

// Worker thread function
void* core_worker(void *arg) {
    int core_id = *(int*)arg;
    
    // Pin this thread to its designated core
    if (pin_to_core(core_id) != 0) {
        printf("Failed to pin thread to core %d\n", core_id);
        return NULL;
    }
    
    printf("Worker thread started on core %d\n", core_id);
    
    // Set up IPI handling
    setup_signal_handling();
    
    // Mark core as active
    pthread_mutex_lock(&shared_data->mutex);
    shared_data->core_active[core_id] = 1;
    pthread_mutex_unlock(&shared_data->mutex);
    
    // Wait for IPIs
    while (shared_data->core_active[core_id]) {
        // Simple busy-wait loop
        usleep(1000); // Sleep for 1ms to reduce CPU usage
    }
    
    printf("Worker thread on core %d exiting\n", core_id);
    return NULL;
}

// Send IPI to specific core
void send_ipi(int target_core) {
    // Find thread ID for target core
    for (int i = 0; i < NUM_CORES; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/proc/self/task/%ld/cpu", syscall(SYS_gettid));
        FILE *f = fopen(path, "r");
        if (f) {
            int cpu;
            if (fscanf(f, "%d", &cpu) == 1 && cpu == target_core) {
                pthread_kill(pthread_self(), SIGUSR1);
                break;
            }
            fclose(f);
        }
    }
}

// Test inter-core communication pattern
void test_interrupt_pattern() {
    printf("\nTesting interrupt patterns:\n");
    printf("---------------------------\n");
    
    // Test 1: A76 to A55 communication
    printf("\nTest 1: A76 (Core 4) to A55 (Core 0) communication\n");
    for (int i = 0; i < 5; i++) {
        send_ipi(0); // Send from A76 to A55
        usleep(1000);
    }
    
    // Test 2: A55 to A76 communication
    printf("\nTest 2: A55 (Core 0) to A76 (Core 4) communication\n");
    for (int i = 0; i < 5; i++) {
        send_ipi(4); // Send from A55 to A76
        usleep(1000);
    }
    
    // Test 3: Round-robin communication
    printf("\nTest 3: Round-robin communication across all cores\n");
    for (int i = 0; i < NUM_CORES; i++) {
        send_ipi((i + 1) % NUM_CORES);
        usleep(1000);
    }
}

int main() {
    printf("RK3588 GIC Test\n");
    printf("===============\n");
    
    // Initialize shared data
    shared_data = malloc(sizeof(shared_data_t));
    memset(shared_data, 0, sizeof(shared_data_t));
    pthread_mutex_init(&shared_data->mutex, NULL);
    
    // Create worker threads for each core
    pthread_t threads[NUM_CORES];
    int core_ids[NUM_CORES];
    
    for (int i = 0; i < NUM_CORES; i++) {
        core_ids[i] = i;
        if (pthread_create(&threads[i], NULL, core_worker, &core_ids[i]) != 0) {
            printf("Failed to create thread for core %d\n", i);
            exit(1);
        }
    }
    
    // Wait for all cores to become active
    sleep(1);
    
    // Run interrupt tests
    test_interrupt_pattern();
    
    // Print final interrupt counts
    printf("\nFinal interrupt counts:\n");
    printf("----------------------\n");
    for (int i = 0; i < NUM_CORES; i++) {
        printf("Core %d: %d interrupts\n", i, shared_data->interrupt_count[i]);
    }
    
    // Signal threads to exit
    for (int i = 0; i < NUM_CORES; i++) {
        shared_data->core_active[i] = 0;
    }
    
    // Wait for all threads to finish
    for (int i = 0; i < NUM_CORES; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Cleanup
    pthread_mutex_destroy(&shared_data->mutex);
    free(shared_data);
    
    return 0;
}
