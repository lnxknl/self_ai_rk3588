#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>

// RK3588 MCU Subsystem Base Addresses
#define MCU_BASE                0xFE780000
#define MCU_LENGTH             0x10000
#define MCU_TCM_BASE           0xFE790000
#define MCU_TCM_LENGTH         0x4000  // 16KB TCM

// MCU Control Registers
#define MCU_CTRL               0x0000
#define MCU_STATUS             0x0004
#define MCU_INTEN              0x0008
#define MCU_INTMASK            0x000C
#define MCU_INTSTAT            0x0010
#define MCU_BOOT_ADDR          0x0014
#define MCU_TCM_CTRL           0x0018
#define MCU_MAILBOX_0          0x0020
#define MCU_MAILBOX_1          0x0024
#define MCU_MAILBOX_2          0x0028
#define MCU_MAILBOX_3          0x002C

// MCU Control Register Bits
#define MCU_CTRL_EN            (1 << 0)   // MCU Enable
#define MCU_CTRL_SLEEPING      (1 << 1)   // MCU Sleep Status
#define MCU_CTRL_RESET         (1 << 2)   // MCU Reset
#define MCU_CTRL_TCM_EN        (1 << 3)   // TCM Enable
#define MCU_CTRL_CACHE_EN      (1 << 4)   // Cache Enable
#define MCU_CTRL_IRQ_EN        (1 << 5)   // Global IRQ Enable

// Example MCU Firmware (simple counter)
static const uint32_t mcu_firmware[] = {
    0x20001000,  // Stack pointer
    0x00000041,  // Reset vector
    0x00000000,  // NMI vector
    0x00000000,  // HardFault vector
    // Simple counter program
    0x2000B510,  // PUSH {R4,LR}
    0x4C034801,  // LDR R0, =counter
    0x60013001,  // ADDS R1, #1
    0x60016801,  // STR R1, [R0]
    0xBD102000,  // POP {R4,PC}
    0x20000000,  // counter: .word 0
};

typedef struct {
    int mem_fd;
    void *mcu_base;
    void *tcm_base;
} mcu_context_t;

// Initialize MCU access
static int mcu_init(mcu_context_t *ctx) {
    ctx->mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (ctx->mem_fd < 0) {
        perror("Failed to open /dev/mem");
        return -1;
    }

    // Map MCU registers
    ctx->mcu_base = mmap(NULL, MCU_LENGTH, PROT_READ | PROT_WRITE,
                        MAP_SHARED, ctx->mem_fd, MCU_BASE);
    if (ctx->mcu_base == MAP_FAILED) {
        perror("Failed to map MCU registers");
        close(ctx->mem_fd);
        return -1;
    }

    // Map TCM memory
    ctx->tcm_base = mmap(NULL, MCU_TCM_LENGTH, PROT_READ | PROT_WRITE,
                        MAP_SHARED, ctx->mem_fd, MCU_TCM_BASE);
    if (ctx->tcm_base == MAP_FAILED) {
        perror("Failed to map TCM memory");
        munmap(ctx->mcu_base, MCU_LENGTH);
        close(ctx->mem_fd);
        return -1;
    }

    return 0;
}

// Clean up MCU access
static void mcu_cleanup(mcu_context_t *ctx) {
    if (ctx->tcm_base != MAP_FAILED) {
        munmap(ctx->tcm_base, MCU_TCM_LENGTH);
    }
    if (ctx->mcu_base != MAP_FAILED) {
        munmap(ctx->mcu_base, MCU_LENGTH);
    }
    if (ctx->mem_fd >= 0) {
        close(ctx->mem_fd);
    }
}

// Read MCU register
static uint32_t mcu_read(mcu_context_t *ctx, uint32_t offset) {
    return *(volatile uint32_t*)((char*)ctx->mcu_base + offset);
}

// Write MCU register
static void mcu_write(mcu_context_t *ctx, uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)((char*)ctx->mcu_base + offset) = value;
}

// Load firmware to TCM
static int load_firmware(mcu_context_t *ctx) {
    // Reset MCU first
    mcu_write(ctx, MCU_CTRL, MCU_CTRL_RESET);
    usleep(1000);
    
    // Copy firmware to TCM
    memcpy(ctx->tcm_base, mcu_firmware, sizeof(mcu_firmware));
    
    // Set boot address to TCM start
    mcu_write(ctx, MCU_BOOT_ADDR, MCU_TCM_BASE);
    
    printf("Firmware loaded to TCM\n");
    return 0;
}

// Configure and start MCU
static int start_mcu(mcu_context_t *ctx) {
    uint32_t ctrl;
    
    // Enable TCM and Cache
    ctrl = MCU_CTRL_TCM_EN | MCU_CTRL_CACHE_EN;
    mcu_write(ctx, MCU_CTRL, ctrl);
    usleep(1000);
    
    // Enable MCU and IRQs
    ctrl |= MCU_CTRL_EN | MCU_CTRL_IRQ_EN;
    mcu_write(ctx, MCU_CTRL, ctrl);
    
    printf("MCU started\n");
    return 0;
}

// Test mailbox communication
static void test_mailbox(mcu_context_t *ctx) {
    uint32_t msg;
    
    printf("\nTesting Mailbox Communication:\n");
    printf("-----------------------------\n");
    
    // Write test message
    msg = 0x12345678;
    printf("Writing to mailbox 0: 0x%08x\n", msg);
    mcu_write(ctx, MCU_MAILBOX_0, msg);
    
    // Read back
    msg = mcu_read(ctx, MCU_MAILBOX_0);
    printf("Read from mailbox 0: 0x%08x\n", msg);
}

// Monitor MCU status
static void monitor_mcu(mcu_context_t *ctx) {
    uint32_t status = mcu_read(ctx, MCU_STATUS);
    uint32_t intstat = mcu_read(ctx, MCU_INTSTAT);
    
    printf("\nMCU Status:\n");
    printf("-----------\n");
    printf("Status Register: 0x%08x\n", status);
    printf("  Running: %s\n", (status & MCU_CTRL_EN) ? "Yes" : "No");
    printf("  Sleeping: %s\n", (status & MCU_CTRL_SLEEPING) ? "Yes" : "No");
    printf("  TCM Enabled: %s\n", (status & MCU_CTRL_TCM_EN) ? "Yes" : "No");
    printf("  Cache Enabled: %s\n", (status & MCU_CTRL_CACHE_EN) ? "Yes" : "No");
    printf("\nInterrupt Status: 0x%08x\n", intstat);
}

int main() {
    mcu_context_t ctx = {
        .mem_fd = -1,
        .mcu_base = MAP_FAILED,
        .tcm_base = MAP_FAILED
    };

    printf("RK3588 MCU Subsystem Test\n");
    printf("=========================\n");

    if (mcu_init(&ctx) < 0) {
        return -1;
    }

    // Load test firmware
    if (load_firmware(&ctx) < 0) {
        mcu_cleanup(&ctx);
        return -1;
    }

    // Start MCU
    if (start_mcu(&ctx) < 0) {
        mcu_cleanup(&ctx);
        return -1;
    }

    // Test mailbox communication
    test_mailbox(&ctx);

    // Monitor MCU status
    monitor_mcu(&ctx);

    // Wait a bit to see MCU running
    printf("\nWaiting for MCU execution...\n");
    sleep(1);

    // Check status again
    monitor_mcu(&ctx);

    // Clean up
    mcu_cleanup(&ctx);
    return 0;
}
