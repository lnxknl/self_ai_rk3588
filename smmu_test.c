#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>

// MMU600 Base Address for RK3588
#define SMMU_BASE               0xFE080000
#define SMMU_LENGTH            0x40000

// MMU600 Register Offsets
#define SMMU_CR0               0x0000  // Control Register 0
#define SMMU_CR1               0x0004  // Control Register 1
#define SMMU_CR2               0x0008  // Control Register 2
#define SMMU_ACR               0x0010  // Auxiliary Control Register
#define SMMU_IDR0              0x0020  // ID Register 0
#define SMMU_IDR1              0x0024  // ID Register 1
#define SMMU_IDR2              0x0028  // ID Register 2
#define SMMU_STLBIALL         0x0060  // TLB Invalidate All
#define SMMU_TLBIVMID         0x0064  // TLB Invalidate by VMID
#define SMMU_TLBIASID         0x0068  // TLB Invalidate by ASID
#define SMMU_TLBGSYNC         0x0070  // Global Synchronization
#define SMMU_GBPA             0x0044  // Global Bypass Attribute

// Stream Table Base Registers
#define SMMU_STBR             0x0080
#define SMMU_STBR_HIGH        0x0084
#define SMMU_STBR_LOW         0x0088

// Context Bank Registers (for first context bank)
#define SMMU_CB0_SCTLR        0x8000  // System Control Register
#define SMMU_CB0_ACTLR        0x8004  // Auxiliary Control Register
#define SMMU_CB0_RESUME       0x8008  // Resume Register
#define SMMU_CB0_TTBR0_LOW    0x8020  // Translation Table Base Register 0 Low
#define SMMU_CB0_TTBR0_HIGH   0x8024  // Translation Table Base Register 0 High
#define SMMU_CB0_TCR          0x8030  // Translation Control Register
#define SMMU_CB0_FSR          0x8058  // Fault Status Register
#define SMMU_CB0_FAR_LOW      0x8060  // Fault Address Register Low
#define SMMU_CB0_FAR_HIGH     0x8064  // Fault Address Register High
#define SMMU_CB0_FSYNR0       0x8068  // Fault Syndrome Register 0

// Control Register Bits
#define CR0_SMMUEN            (1 << 0)   // SMMU Enable
#define CR0_MTCFG             (1 << 1)   // Memory Type Configuration
#define CR0_BSU_SHIFT         4          // Barrier Shareable Unit
#define CR0_FB                (1 << 7)   // Force Broadcast
#define CR0_VMIDPNE          (1 << 8)   // VMID Private Not Enable
#define CR0_PTM              (1 << 9)   // Private TLB Maintenance
#define CR0_CLIENTPD         (1 << 10)  // Client Port Disable

// Context Bank Control Bits
#define SCTLR_M              (1 << 0)   // MMU Enable
#define SCTLR_TRE            (1 << 1)   // TEX Remap Enable
#define SCTLR_AFE            (1 << 2)   // Access Flag Enable
#define SCTLR_AFFD           (1 << 3)   // Access Flag Fault Disable
#define SCTLR_E              (1 << 4)   // Endianness

typedef struct {
    int mem_fd;
    void *smmu_base;
} smmu_context_t;

// Initialize SMMU access
static int smmu_init(smmu_context_t *ctx) {
    ctx->mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (ctx->mem_fd < 0) {
        perror("Failed to open /dev/mem");
        return -1;
    }

    ctx->smmu_base = mmap(NULL, SMMU_LENGTH, PROT_READ | PROT_WRITE,
                         MAP_SHARED, ctx->mem_fd, SMMU_BASE);
    if (ctx->smmu_base == MAP_FAILED) {
        perror("Failed to map SMMU registers");
        close(ctx->mem_fd);
        return -1;
    }

    return 0;
}

// Clean up SMMU access
static void smmu_cleanup(smmu_context_t *ctx) {
    if (ctx->smmu_base != MAP_FAILED) {
        munmap(ctx->smmu_base, SMMU_LENGTH);
    }
    if (ctx->mem_fd >= 0) {
        close(ctx->mem_fd);
    }
}

// Read SMMU register
static uint32_t smmu_read(smmu_context_t *ctx, uint32_t offset) {
    return *(volatile uint32_t*)((char*)ctx->smmu_base + offset);
}

// Write SMMU register
static void smmu_write(smmu_context_t *ctx, uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)((char*)ctx->smmu_base + offset) = value;
}

// Print SMMU capabilities
static void print_smmu_info(smmu_context_t *ctx) {
    uint32_t idr0 = smmu_read(ctx, SMMU_IDR0);
    uint32_t idr1 = smmu_read(ctx, SMMU_IDR1);
    uint32_t idr2 = smmu_read(ctx, SMMU_IDR2);

    printf("\nSMMU Capabilities:\n");
    printf("-----------------\n");
    printf("IDR0: 0x%08x\n", idr0);
    printf("  - Number of context banks: %d\n", ((idr0 >> 0) & 0xFF) + 1);
    printf("  - Number of stream mapping groups: %d\n", ((idr0 >> 16) & 0xFF) + 1);
    printf("  - Supports stage 2 translation: %s\n", (idr0 & (1 << 24)) ? "Yes" : "No");
    
    printf("\nIDR1: 0x%08x\n", idr1);
    printf("  - Page sizes supported: %s%s%s\n",
           (idr1 & (1 << 0)) ? "4KB " : "",
           (idr1 & (1 << 1)) ? "64KB " : "",
           (idr1 & (1 << 2)) ? "1MB " : "");
    
    printf("\nIDR2: 0x%08x\n", idr2);
    printf("  - IAS (Input Address Size): %d bits\n", ((idr2 >> 0) & 0xF) + 32);
    printf("  - OAS (Output Address Size): %d bits\n", ((idr2 >> 4) & 0xF) + 32);
}

// Configure basic SMMU settings
static int configure_smmu(smmu_context_t *ctx) {
    uint32_t cr0;
    
    // Read current CR0
    cr0 = smmu_read(ctx, SMMU_CR0);
    printf("\nCurrent CR0: 0x%08x\n", cr0);

    // Configure CR0
    cr0 &= ~CR0_SMMUEN;  // Disable SMMU first
    smmu_write(ctx, SMMU_CR0, cr0);
    
    // Wait for any pending transactions
    usleep(1000);
    
    // Configure basic settings
    cr0 |= (0x3 << CR0_BSU_SHIFT);  // Inner shareable
    cr0 |= CR0_FB;                   // Force broadcast
    cr0 &= ~CR0_VMIDPNE;            // Enable VMID
    
    printf("New CR0: 0x%08x\n", cr0);
    smmu_write(ctx, SMMU_CR0, cr0);
    
    return 0;
}

// Configure context bank 0
static int configure_context_bank(smmu_context_t *ctx) {
    uint32_t sctlr;
    
    // Read current SCTLR
    sctlr = smmu_read(ctx, SMMU_CB0_SCTLR);
    printf("\nCurrent CB0 SCTLR: 0x%08x\n", sctlr);
    
    // Configure SCTLR
    sctlr |= SCTLR_M;    // Enable MMU
    sctlr |= SCTLR_TRE;  // Enable TEX remap
    sctlr |= SCTLR_AFE;  // Enable access flag
    
    printf("New CB0 SCTLR: 0x%08x\n", sctlr);
    smmu_write(ctx, SMMU_CB0_SCTLR, sctlr);
    
    return 0;
}

// Check fault status
static void check_fault_status(smmu_context_t *ctx) {
    uint32_t fsr = smmu_read(ctx, SMMU_CB0_FSR);
    uint32_t far_low = smmu_read(ctx, SMMU_CB0_FAR_LOW);
    uint32_t far_high = smmu_read(ctx, SMMU_CB0_FAR_HIGH);
    uint32_t fsynr0 = smmu_read(ctx, SMMU_CB0_FSYNR0);

    printf("\nFault Status:\n");
    printf("-------------\n");
    printf("FSR: 0x%08x\n", fsr);
    if (fsr) {
        printf("  Fault Address: 0x%08x%08x\n", far_high, far_low);
        printf("  Fault Syndrome: 0x%08x\n", fsynr0);
    } else {
        printf("  No faults detected\n");
    }
}

int main() {
    smmu_context_t ctx = {
        .mem_fd = -1,
        .smmu_base = MAP_FAILED
    };

    printf("RK3588 MMU600 SMMU Test\n");
    printf("=======================\n");

    if (smmu_init(&ctx) < 0) {
        return -1;
    }

    // Print SMMU capabilities
    print_smmu_info(&ctx);

    // Configure SMMU
    if (configure_smmu(&ctx) < 0) {
        smmu_cleanup(&ctx);
        return -1;
    }

    // Configure context bank
    if (configure_context_bank(&ctx) < 0) {
        smmu_cleanup(&ctx);
        return -1;
    }

    // Check for any faults
    check_fault_status(&ctx);

    // Clean up
    smmu_cleanup(&ctx);
    return 0;
}
