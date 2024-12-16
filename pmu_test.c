#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>

// RK3588 PMU Base Addresses
#define PMU_BASE                0xFD8D0000
#define PMU_LENGTH             0x1000

// PMU Register Offsets
#define PMU_PWRDN_CON         0x0000
#define PMU_PWRDN_ST          0x0004
#define PMU_BUS_IDLE_REQ      0x000C
#define PMU_BUS_IDLE_ST       0x0010
#define PMU_POWER_ST          0x0014
#define PMU_OSC_CNT           0x0020
#define PMU_PLLLOCK_CNT       0x0024
#define PMU_STABLE_CNT        0x0028
#define PMU_WAKEUP_RST_CLR_1  0x002C
#define PMU_SFT_CON           0x0030

// Power Domains
#define PD_CPU_0              (1 << 0)
#define PD_CPU_1              (1 << 1)
#define PD_CPU_2              (1 << 2)
#define PD_CPU_3              (1 << 3)
#define PD_GPU                (1 << 4)
#define PD_NPU                (1 << 5)
#define PD_VCODEC            (1 << 6)
#define PD_VDU               (1 << 7)
#define PD_RGA               (1 << 8)
#define PD_VOP               (1 << 9)
#define PD_VI                (1 << 10)
#define PD_VO                (1 << 11)
#define PD_ISP               (1 << 12)
#define PD_PCIE              (1 << 13)
#define PD_PHP               (1 << 14)

// Bus Idle Request/Status bits
#define BUS_IDLE_REQ_CPU      (1 << 0)
#define BUS_IDLE_REQ_PERI     (1 << 1)
#define BUS_IDLE_REQ_VIO      (1 << 2)
#define BUS_IDLE_REQ_VPU      (1 << 3)
#define BUS_IDLE_REQ_GPU      (1 << 4)
#define BUS_IDLE_REQ_NPU      (1 << 5)

typedef struct {
    int mem_fd;
    void *pmu_base;
} pmu_context_t;

// Initialize PMU access
static int pmu_init(pmu_context_t *ctx) {
    ctx->mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (ctx->mem_fd < 0) {
        perror("Failed to open /dev/mem");
        return -1;
    }

    ctx->pmu_base = mmap(NULL, PMU_LENGTH, PROT_READ | PROT_WRITE,
                        MAP_SHARED, ctx->mem_fd, PMU_BASE);
    if (ctx->pmu_base == MAP_FAILED) {
        perror("Failed to map PMU registers");
        close(ctx->mem_fd);
        return -1;
    }

    return 0;
}

// Clean up PMU access
static void pmu_cleanup(pmu_context_t *ctx) {
    if (ctx->pmu_base != MAP_FAILED) {
        munmap(ctx->pmu_base, PMU_LENGTH);
    }
    if (ctx->mem_fd >= 0) {
        close(ctx->mem_fd);
    }
}

// Read PMU register
static uint32_t pmu_read(pmu_context_t *ctx, uint32_t offset) {
    return *(volatile uint32_t*)((char*)ctx->pmu_base + offset);
}

// Write PMU register with mask
static void pmu_write_mask(pmu_context_t *ctx, uint32_t offset, uint32_t value, uint32_t mask) {
    uint32_t old = pmu_read(ctx, offset);
    uint32_t new = (old & ~mask) | (value & mask);
    *(volatile uint32_t*)((char*)ctx->pmu_base + offset) = new | (mask << 16);
}

// Get power domain status
static void print_power_status(pmu_context_t *ctx) {
    uint32_t status = pmu_read(ctx, PMU_POWER_ST);
    printf("\nPower Domain Status:\n");
    printf("-------------------\n");
    printf("CPU_0:   %s\n", (status & PD_CPU_0) ? "ON" : "OFF");
    printf("CPU_1:   %s\n", (status & PD_CPU_1) ? "ON" : "OFF");
    printf("CPU_2:   %s\n", (status & PD_CPU_2) ? "ON" : "OFF");
    printf("CPU_3:   %s\n", (status & PD_CPU_3) ? "ON" : "OFF");
    printf("GPU:     %s\n", (status & PD_GPU) ? "ON" : "OFF");
    printf("NPU:     %s\n", (status & PD_NPU) ? "ON" : "OFF");
    printf("VCODEC:  %s\n", (status & PD_VCODEC) ? "ON" : "OFF");
    printf("VDU:     %s\n", (status & PD_VDU) ? "ON" : "OFF");
    printf("RGA:     %s\n", (status & PD_RGA) ? "ON" : "OFF");
    printf("VOP:     %s\n", (status & PD_VOP) ? "ON" : "OFF");
    printf("ISP:     %s\n", (status & PD_ISP) ? "ON" : "OFF");
    printf("PCIE:    %s\n", (status & PD_PCIE) ? "ON" : "OFF");
}

// Get bus idle status
static void print_bus_idle_status(pmu_context_t *ctx) {
    uint32_t status = pmu_read(ctx, PMU_BUS_IDLE_ST);
    printf("\nBus Idle Status:\n");
    printf("---------------\n");
    printf("CPU:     %s\n", (status & BUS_IDLE_REQ_CPU) ? "IDLE" : "ACTIVE");
    printf("PERI:    %s\n", (status & BUS_IDLE_REQ_PERI) ? "IDLE" : "ACTIVE");
    printf("VIO:     %s\n", (status & BUS_IDLE_REQ_VIO) ? "IDLE" : "ACTIVE");
    printf("VPU:     %s\n", (status & BUS_IDLE_REQ_VPU) ? "IDLE" : "ACTIVE");
    printf("GPU:     %s\n", (status & BUS_IDLE_REQ_GPU) ? "IDLE" : "ACTIVE");
    printf("NPU:     %s\n", (status & BUS_IDLE_REQ_NPU) ? "IDLE" : "ACTIVE");
}

// Power domain control example
static int power_domain_test(pmu_context_t *ctx) {
    printf("\nPower Domain Control Test:\n");
    printf("-----------------------\n");

    // Test power domains that are safe to toggle
    uint32_t test_domains[] = {
        PD_GPU,
        PD_NPU,
        PD_RGA,
        0  // End marker
    };

    for (int i = 0; test_domains[i]; i++) {
        uint32_t domain = test_domains[i];
        
        // Try to power down
        printf("\nAttempting to power down domain 0x%08x\n", domain);
        pmu_write_mask(ctx, PMU_PWRDN_CON, domain, domain);
        usleep(1000);  // Wait for power down
        print_power_status(ctx);
        
        // Try to power up
        printf("\nAttempting to power up domain 0x%08x\n", domain);
        pmu_write_mask(ctx, PMU_PWRDN_CON, 0, domain);
        usleep(1000);  // Wait for power up
        print_power_status(ctx);
    }

    return 0;
}

// Bus idle control example
static int bus_idle_test(pmu_context_t *ctx) {
    printf("\nBus Idle Control Test:\n");
    printf("--------------------\n");

    // Test bus domains that are safe to toggle
    uint32_t test_buses[] = {
        BUS_IDLE_REQ_GPU,
        BUS_IDLE_REQ_NPU,
        0  // End marker
    };

    for (int i = 0; test_buses[i]; i++) {
        uint32_t bus = test_buses[i];
        
        // Request idle
        printf("\nRequesting bus idle for domain 0x%08x\n", bus);
        pmu_write_mask(ctx, PMU_BUS_IDLE_REQ, bus, bus);
        usleep(1000);  // Wait for idle
        print_bus_idle_status(ctx);
        
        // Release idle
        printf("\nReleasing bus idle for domain 0x%08x\n", bus);
        pmu_write_mask(ctx, PMU_BUS_IDLE_REQ, 0, bus);
        usleep(1000);  // Wait for active
        print_bus_idle_status(ctx);
    }

    return 0;
}

int main() {
    pmu_context_t ctx = {
        .mem_fd = -1,
        .pmu_base = MAP_FAILED
    };

    printf("RK3588 PMU Test\n");
    printf("==============\n");

    if (pmu_init(&ctx) < 0) {
        return -1;
    }

    // Show initial status
    printf("\nInitial Status:\n");
    print_power_status(&ctx);
    print_bus_idle_status(&ctx);

    // Run power domain tests
    power_domain_test(&ctx);

    // Run bus idle tests
    bus_idle_test(&ctx);

    // Clean up
    pmu_cleanup(&ctx);
    return 0;
}
