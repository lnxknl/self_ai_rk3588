#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <signal.h>

// RK3588 timer frequency is 24MHz
#define TIMER_FREQ_MHZ 24
#define NS_PER_SEC 1000000000ULL

// Test durations
#define SHORT_DELAY_NS  1000000    // 1ms
#define MEDIUM_DELAY_NS 10000000   // 10ms
#define LONG_DELAY_NS   100000000  // 100ms

// Read the 64-bit system counter using inline assembly
static inline uint64_t read_system_counter(void) {
    uint64_t val;
    asm volatile("mrs %0, cntvct_el0" : "=r" (val));
    return val;
}

// Read the timer frequency
static inline uint64_t read_timer_freq(void) {
    uint64_t freq;
    asm volatile("mrs %0, cntfrq_el0" : "=r" (freq));
    return freq;
}

// Convert counter ticks to nanoseconds
uint64_t ticks_to_ns(uint64_t ticks) {
    return (ticks * NS_PER_SEC) / read_timer_freq();
}

// Convert nanoseconds to counter ticks
uint64_t ns_to_ticks(uint64_t ns) {
    return (ns * read_timer_freq()) / NS_PER_SEC;
}

// High-precision sleep using the system counter
void precise_sleep_ns(uint64_t ns) {
    uint64_t start_ticks = read_system_counter();
    uint64_t target_ticks = start_ticks + ns_to_ticks(ns);
    
    while (read_system_counter() < target_ticks) {
        // Busy wait
        asm volatile("yield");
    }
}

// Test timer accuracy
void test_timer_accuracy(uint64_t delay_ns, const char* test_name) {
    struct timespec ts_start, ts_end;
    uint64_t start_ticks, end_ticks;
    
    printf("\nTesting %s delay (%lu ns):\n", test_name, delay_ns);
    printf("----------------------------------------\n");
    
    // Measure using system counter
    start_ticks = read_system_counter();
    precise_sleep_ns(delay_ns);
    end_ticks = read_system_counter();
    
    uint64_t elapsed_ns = ticks_to_ns(end_ticks - start_ticks);
    double error_percent = ((double)elapsed_ns - delay_ns) * 100.0 / delay_ns;
    
    printf("System Counter Measurement:\n");
    printf("  Target delay:  %lu ns\n", delay_ns);
    printf("  Actual delay:  %lu ns\n", elapsed_ns);
    printf("  Error:        %.3f%%\n", error_percent);
    
    // Compare with standard POSIX clock
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    precise_sleep_ns(delay_ns);
    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    
    uint64_t posix_ns = (ts_end.tv_sec - ts_start.tv_sec) * NS_PER_SEC +
                        (ts_end.tv_nsec - ts_start.tv_nsec);
    error_percent = ((double)posix_ns - delay_ns) * 100.0 / delay_ns;
    
    printf("\nPOSIX Clock Measurement:\n");
    printf("  Target delay:  %lu ns\n", delay_ns);
    printf("  Actual delay:  %lu ns\n", posix_ns);
    printf("  Error:        %.3f%%\n", error_percent);
}

// Timer interrupt handler
void timer_handler(int signo, siginfo_t *info, void *context) {
    uint64_t counter = read_system_counter();
    printf("Timer interrupt at counter value: %lu\n", counter);
}

// Set up timer interrupt
void setup_timer_interrupt() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = timer_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGALRM, &sa, NULL);
}

// Pin thread to specific CPU core
int pin_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_t current_thread = pthread_self();
    return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

int main() {
    // Pin to core 0 (A55) for consistent measurements
    if (pin_to_core(0) != 0) {
        printf("Failed to pin thread to core 0\n");
        return 1;
    }
    
    printf("RK3588 Generic Timer Test\n");
    printf("=========================\n");
    
    // Print timer information
    uint64_t freq = read_timer_freq();
    printf("System Counter Frequency: %lu Hz\n", freq);
    printf("Current Counter Value: %lu\n", read_system_counter());
    
    // Set up timer interrupt
    setup_timer_interrupt();
    
    // Test different delay durations
    test_timer_accuracy(SHORT_DELAY_NS, "short");
    test_timer_accuracy(MEDIUM_DELAY_NS, "medium");
    test_timer_accuracy(LONG_DELAY_NS, "long");
    
    // Demonstrate timer interrupt
    printf("\nTesting timer interrupts:\n");
    printf("------------------------\n");
    
    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 100000;  // 100ms
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 100000;
    
    setitimer(ITIMER_REAL, &timer, NULL);
    
    // Wait for a few timer interrupts
    sleep(1);
    
    // Disable timer
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &timer, NULL);
    
    return 0;
}
