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

// PLL Configuration Masks
#define PLL_POSTDIV1_MASK  (0x7 << 12)
#define PLL_POSTDIV2_MASK  (0x7 << 6)
#define PLL_REFDIV_MASK    (0x3f << 0)
#define PLL_FBDIV_MASK     (0xfff << 0)

// PLL Configuration Shifts
#define PLL_POSTDIV1_SHIFT 12
#define PLL_POSTDIV2_SHIFT 6
#define PLL_REFDIV_SHIFT   0
#define PLL_FBDIV_SHIFT    0

// PLL Status and Control
#define PLL_POWER_DOWN     (1 << 0)
#define PLL_POWER_UP       (0 << 0)
#define PLL_LOCK_STATUS    (1 << 31)

// Write masks (RK3588 specific)
#define PLL_POSTDIV1_W_MASK  (0x7 << (12 + 16))
#define PLL_POSTDIV2_W_MASK  (0x7 << (6 + 16))
#define PLL_REFDIV_W_MASK    (0x3f << 16)
#define PLL_FBDIV_W_MASK     (0xfff << 16)
#define PLL_POWER_MASK       (1 << 16)  // Write mask for power bit

typedef struct {
    int mem_fd;
    void *cru_base;
} cru_context_t;

// PLL configuration structure
typedef struct {
    uint32_t rate;      // Target rate in Hz
    uint32_t fbdiv;     // Feedback divider
    uint32_t postdiv1;  // Post divider 1
    uint32_t postdiv2;  // Post divider 2
    uint32_t refdiv;    // Reference clock divider
} pll_config_t;

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

// Write CRU register with mask
static void cru_write_mask(cru_context_t *ctx, uint32_t offset, uint32_t value, uint32_t mask) {
    uint32_t old = cru_read(ctx, offset);
    uint32_t new = (old & ~mask) | (value & mask);
    *(volatile uint32_t*)((char*)ctx->cru_base + offset) = new | (mask << 16);
}

// Calculate PLL parameters for target frequency
static void calculate_pll_config(uint32_t target_hz, pll_config_t *config) {
    const uint32_t ref_hz = 24000000;  // 24MHz reference clock
    uint32_t vco_hz;
    
    config->rate = target_hz;
    
    // VCO should be between 800MHz and 2000MHz
    if (target_hz < 400000000) {
        config->postdiv1 = 2;
        config->postdiv2 = 1;
        vco_hz = target_hz * 2;
    } else {
        config->postdiv1 = 1;
        config->postdiv2 = 1;
        vco_hz = target_hz;
    }

    // Use refdiv=1 for simplicity
    config->refdiv = 1;
    
    // Calculate feedback divider
    config->fbdiv = vco_hz / (ref_hz / config->refdiv);
    
    printf("PLL Config: fbdiv=%d, postdiv1=%d, postdiv2=%d, refdiv=%d\n",
           config->fbdiv, config->postdiv1, config->postdiv2, config->refdiv);
}

// Configure GPLL
static int configure_gpll(cru_context_t *ctx, uint32_t rate_hz) {
    pll_config_t config;
    uint32_t v;
    
    calculate_pll_config(rate_hz, &config);
    
    // Power down PLL first
    cru_write_mask(ctx, CRU_GPLL_CON0, PLL_POWER_DOWN, PLL_POWER_MASK);
    usleep(10);

    // Set PLL parameters
    // CON0: fbdiv
    v = (config.fbdiv << PLL_FBDIV_SHIFT);
    cru_write_mask(ctx, CRU_GPLL_CON0, v, PLL_FBDIV_MASK);

    // CON1: postdiv1, postdiv2, refdiv
    v = (config.postdiv1 << PLL_POSTDIV1_SHIFT) |
        (config.postdiv2 << PLL_POSTDIV2_SHIFT) |
        (config.refdiv << PLL_REFDIV_SHIFT);
    cru_write_mask(ctx, CRU_GPLL_CON1, v, 
                   PLL_POSTDIV1_MASK | PLL_POSTDIV2_MASK | PLL_REFDIV_MASK);

    // Power up PLL
    cru_write_mask(ctx, CRU_GPLL_CON0, PLL_POWER_UP, PLL_POWER_MASK);

    // Wait for PLL lock
    int timeout = 1000;
    while (timeout--) {
        if (cru_read(ctx, CRU_GPLL_CON0) & PLL_LOCK_STATUS) {
            printf("PLL locked, timeout remaining: %d\n", timeout);
            return 0;
        }
        usleep(1);
    }

    printf("Failed to lock GPLL (CON0=0x%08x, CON1=0x%08x)\n",
           cru_read(ctx, CRU_GPLL_CON0),
           cru_read(ctx, CRU_GPLL_CON1));
    return -1;
}

// Monitor clock status
static void monitor_clocks(cru_context_t *ctx) {
    printf("\nClock Status:\n");
    printf("------------\n");

    uint32_t gpll_con0 = cru_read(ctx, CRU_GPLL_CON0);
    uint32_t gpll_con1 = cru_read(ctx, CRU_GPLL_CON1);
    
    printf("GPLL:\n");
    printf("  Status: %s, %s\n",
           (gpll_con0 & PLL_POWER_DOWN) ? "Powered Down" : "Powered Up",
           (gpll_con0 & PLL_LOCK_STATUS) ? "Locked" : "Unlocked");
    printf("  CON0: 0x%08x\n", gpll_con0);
    printf("  CON1: 0x%08x\n", gpll_con1);
    printf("  FBDIV: %d\n", (gpll_con0 & PLL_FBDIV_MASK) >> PLL_FBDIV_SHIFT);
    printf("  POSTDIV1: %d\n", (gpll_con1 & PLL_POSTDIV1_MASK) >> PLL_POSTDIV1_SHIFT);
    printf("  POSTDIV2: %d\n", (gpll_con1 & PLL_POSTDIV2_MASK) >> PLL_POSTDIV2_SHIFT);
    printf("  REFDIV: %d\n", (gpll_con1 & PLL_REFDIV_MASK) >> PLL_REFDIV_SHIFT);
}

int main() {
    cru_context_t ctx = {
        .mem_fd = -1,
        .cru_base = MAP_FAILED
    };

    printf("RK3588 CRU Test (Enhanced PLL Configuration)\n");
    printf("==========================================\n");

    if (cru_init(&ctx) < 0) {
        return -1;
    }

    // Show initial clock status
    printf("\nInitial Clock Status:\n");
    monitor_clocks(&ctx);

    // Test frequencies
    uint32_t test_freqs[] = {
        408000000,  // 408 MHz
        600000000,  // 600 MHz
        816000000,  // 816 MHz
        1008000000  // 1008 MHz
    };

    for (int i = 0; i < sizeof(test_freqs)/sizeof(test_freqs[0]); i++) {
        printf("\nTesting GPLL at %u Hz...\n", test_freqs[i]);
        if (configure_gpll(&ctx, test_freqs[i]) == 0) {
            printf("Success\n");
        } else {
            printf("Failed\n");
        }
        monitor_clocks(&ctx);
        sleep(1);
    }

    cru_cleanup(&ctx);
    return 0;
}
