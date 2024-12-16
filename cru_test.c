#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>

// RK3588 CRU Base Address
#define CRU_BASE            0xFD7C0000
#define CRU_LENGTH          0x1000

// CRU Register Offsets
#define CRU_GPLL_CON0      0x0040
#define CRU_GPLL_CON1      0x0044
#define CRU_GPLL_CON2      0x0048
#define CRU_MODE_CON0      0x0280
#define CRU_CLKSEL_CON0    0x0300
#define CRU_CLKGATE_CON0   0x0800
#define CRU_SOFTRST_CON0   0x0A00

// Clock Gates
#define PCLK_GPIO1_GATE    (1 << 3)
#define HCLK_SDMMC_GATE    (1 << 4)
#define ACLK_USB3_GATE     (1 << 5)

// PLL Configuration
#define PLL_POWER_DOWN     (1 << 0)
#define PLL_POWER_UP       (0 << 0)
#define PLL_LOCK_STATUS    (1 << 31)

typedef struct {
    int mem_fd;
    void *cru_base;
} cru_context_t;

// Initialize CRU access
static int cru_init(cru_context_t *ctx) {
    ctx->mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (ctx->mem_fd < 0) {
        perror("Failed to open /dev/mem");
        return -1;
    }

    ctx->cru_base = mmap(NULL, CRU_LENGTH, PROT_READ | PROT_WRITE,
                        MAP_SHARED, ctx->mem_fd, CRU_BASE);
    if (ctx->cru_base == MAP_FAILED) {
        perror("Failed to map CRU registers");
        close(ctx->mem_fd);
        return -1;
    }

    return 0;
}

// Clean up CRU access
static void cru_cleanup(cru_context_t *ctx) {
    if (ctx->cru_base != MAP_FAILED) {
        munmap(ctx->cru_base, CRU_LENGTH);
    }
    if (ctx->mem_fd >= 0) {
        close(ctx->mem_fd);
    }
}

// Read CRU register
static uint32_t cru_read(cru_context_t *ctx, uint32_t offset) {
    return *(volatile uint32_t*)((char*)ctx->cru_base + offset);
}

// Write CRU register
static void cru_write(cru_context_t *ctx, uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)((char*)ctx->cru_base + offset) = value;
}

// Configure GPLL (General Purpose PLL)
static int configure_gpll(cru_context_t *ctx, uint32_t rate_hz) {
    uint32_t value;
    
    // Power down PLL first
    value = cru_read(ctx, CRU_GPLL_CON0);
    value |= PLL_POWER_DOWN;
    cru_write(ctx, CRU_GPLL_CON0, value);
    usleep(10);

    // Calculate PLL parameters (simplified example)
    uint32_t fbdiv = rate_hz / 24000000;  // Assuming 24MHz reference
    
    // Set PLL parameters
    value = (fbdiv << 8) | PLL_POWER_UP;
    cru_write(ctx, CRU_GPLL_CON0, value);

    // Wait for PLL lock
    int timeout = 1000;
    while (timeout--) {
        if (cru_read(ctx, CRU_GPLL_CON0) & PLL_LOCK_STATUS) {
            return 0;
        }
        usleep(1);
    }

    printf("Failed to lock GPLL\n");
    return -1;
}

// Enable/disable peripheral clock
static void set_peripheral_clock(cru_context_t *ctx, uint32_t gate_offset, uint32_t gate_mask, int enable) {
    uint32_t value = cru_read(ctx, gate_offset);
    if (enable) {
        value &= ~gate_mask;  // Clear bit to enable clock
    } else {
        value |= gate_mask;   // Set bit to disable clock
    }
    cru_write(ctx, gate_offset, value);
}

// Monitor clock status
static void monitor_clocks(cru_context_t *ctx) {
    printf("\nClock Status:\n");
    printf("------------\n");

    // Read GPLL status
    uint32_t gpll_con0 = cru_read(ctx, CRU_GPLL_CON0);
    printf("GPLL: %s, %s\n",
           (gpll_con0 & PLL_POWER_DOWN) ? "Powered Down" : "Powered Up",
           (gpll_con0 & PLL_LOCK_STATUS) ? "Locked" : "Unlocked");

    // Read clock gates
    uint32_t gate_con0 = cru_read(ctx, CRU_CLKGATE_CON0);
    printf("Clock Gates:\n");
    printf("  PCLK_GPIO1: %s\n", (gate_con0 & PCLK_GPIO1_GATE) ? "Disabled" : "Enabled");
    printf("  HCLK_SDMMC: %s\n", (gate_con0 & HCLK_SDMMC_GATE) ? "Disabled" : "Enabled");
    printf("  ACLK_USB3:  %s\n", (gate_con0 & ACLK_USB3_GATE) ? "Disabled" : "Enabled");
}

// Test different clock configurations
static void test_clock_configs(cru_context_t *ctx) {
    printf("\nTesting Clock Configurations:\n");
    printf("--------------------------\n");

    // Test GPLL configurations
    uint32_t test_freqs[] = {
        600000000,  // 600 MHz
        800000000,  // 800 MHz
        1000000000, // 1.0 GHz
        1200000000  // 1.2 GHz
    };

    for (int i = 0; i < sizeof(test_freqs)/sizeof(test_freqs[0]); i++) {
        printf("\nTesting GPLL at %u Hz... ", test_freqs[i]);
        if (configure_gpll(ctx, test_freqs[i]) == 0) {
            printf("Success\n");
        } else {
            printf("Failed\n");
        }
        monitor_clocks(ctx);
        sleep(1);
    }

    // Test peripheral clocks
    printf("\nTesting Peripheral Clocks:\n");
    
    printf("Enabling PCLK_GPIO1... ");
    set_peripheral_clock(ctx, CRU_CLKGATE_CON0, PCLK_GPIO1_GATE, 1);
    printf("Done\n");
    monitor_clocks(ctx);
    sleep(1);

    printf("\nEnabling HCLK_SDMMC... ");
    set_peripheral_clock(ctx, CRU_CLKGATE_CON0, HCLK_SDMMC_GATE, 1);
    printf("Done\n");
    monitor_clocks(ctx);
    sleep(1);

    printf("\nEnabling ACLK_USB3... ");
    set_peripheral_clock(ctx, CRU_CLKGATE_CON0, ACLK_USB3_GATE, 1);
    printf("Done\n");
    monitor_clocks(ctx);
}

int main() {
    cru_context_t ctx = {
        .mem_fd = -1,
        .cru_base = MAP_FAILED
    };

    printf("RK3588 CRU Test\n");
    printf("==============\n");

    if (cru_init(&ctx) < 0) {
        return -1;
    }

    // Show initial clock status
    printf("\nInitial Clock Status:\n");
    monitor_clocks(&ctx);

    // Run clock configuration tests
    test_clock_configs(&ctx);

    // Clean up
    cru_cleanup(&ctx);
    return 0;
}
