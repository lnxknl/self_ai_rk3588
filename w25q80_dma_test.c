#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>

// W25Q80 Commands
#define W25Q80_READ_ID          0x9F
#define W25Q80_READ_STATUS      0x05
#define W25Q80_WRITE_ENABLE     0x06
#define W25Q80_PAGE_PROGRAM     0x02
#define W25Q80_READ_DATA        0x03
#define W25Q80_FAST_READ        0x0B
#define W25Q80_SECTOR_ERASE     0x20
#define W25Q80_CHIP_ERASE       0xC7

// W25Q80 Parameters
#define W25Q80_PAGE_SIZE        256
#define W25Q80_SECTOR_SIZE      4096
#define W25Q80_TOTAL_SIZE       1024*1024  // 1MB
#define MAX_TRANSFER_SIZE       (32*1024)   // 32KB for DMA

// SPI Parameters
#define SPI_SPEED_HZ           25000000    // 25MHz
#define SPI_BITS_PER_WORD     8
#define SPI_MODE              SPI_MODE_0

typedef struct {
    int fd;
    uint32_t speed;
    uint8_t mode;
    uint8_t bits;
} spi_device_t;

// Utility functions
static void hex_dump(const void* data, size_t size) {
    const uint8_t* p = (const uint8_t*)data;
    for (size_t i = 0; i < size; i++) {
        printf("%02X ", p[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
}

// Initialize SPI device
static int spi_init(spi_device_t* dev, const char* device) {
    dev->fd = open(device, O_RDWR);
    if (dev->fd < 0) {
        printf("Failed to open %s: %s\n", device, strerror(errno));
        return -1;
    }

    dev->mode = SPI_MODE;
    dev->bits = SPI_BITS_PER_WORD;
    dev->speed = SPI_SPEED_HZ;

    if (ioctl(dev->fd, SPI_IOC_WR_MODE, &dev->mode) < 0) {
        printf("Failed to set SPI mode\n");
        return -1;
    }

    if (ioctl(dev->fd, SPI_IOC_WR_BITS_PER_WORD, &dev->bits) < 0) {
        printf("Failed to set bits per word\n");
        return -1;
    }

    if (ioctl(dev->fd, SPI_IOC_WR_MAX_SPEED_HZ, &dev->speed) < 0) {
        printf("Failed to set speed\n");
        return -1;
    }

    return 0;
}

// Read W25Q80 ID
static int read_flash_id(spi_device_t* dev, uint8_t* id) {
    uint8_t tx[] = {W25Q80_READ_ID, 0, 0, 0};
    uint8_t rx[4] = {0};
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = sizeof(tx),
        .speed_hz = dev->speed,
        .bits_per_word = dev->bits,
        .delay_usecs = 0,
    };

    if (ioctl(dev->fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        printf("Failed to read flash ID\n");
        return -1;
    }

    memcpy(id, rx + 1, 3);
    return 0;
}

// DMA-assisted read
static int dma_read_flash(spi_device_t* dev, uint32_t addr, uint8_t* buffer, size_t len) {
    if (len > MAX_TRANSFER_SIZE) {
        printf("Transfer size too large for DMA\n");
        return -1;
    }

    uint8_t* tx = malloc(len + 4);  // Command + 3 address bytes + data
    uint8_t* rx = malloc(len + 4);
    if (!tx || !rx) {
        printf("Failed to allocate DMA buffers\n");
        free(tx);
        free(rx);
        return -1;
    }

    // Setup command and address
    tx[0] = W25Q80_FAST_READ;
    tx[1] = (addr >> 16) & 0xFF;
    tx[2] = (addr >> 8) & 0xFF;
    tx[3] = addr & 0xFF;
    memset(tx + 4, 0, len);  // Dummy bytes for data reception

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = len + 4,
        .speed_hz = dev->speed,
        .bits_per_word = dev->bits,
        .delay_usecs = 0,
    };

    if (ioctl(dev->fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        printf("DMA read failed\n");
        free(tx);
        free(rx);
        return -1;
    }

    memcpy(buffer, rx + 4, len);
    free(tx);
    free(rx);
    return 0;
}

// Performance test structure
typedef struct {
    size_t size;
    double time_ms;
    double bandwidth_mbps;
} perf_result_t;

// Measure read performance
static void measure_read_performance(spi_device_t* dev, perf_result_t* results, int num_tests) {
    const size_t test_sizes[] = {256, 1024, 4096, 16384, 32768};  // Test different sizes
    uint8_t* buffer = malloc(MAX_TRANSFER_SIZE);
    if (!buffer) {
        printf("Failed to allocate test buffer\n");
        return;
    }

    printf("\nRead Performance Test:\n");
    printf("---------------------\n");
    
    for (int i = 0; i < sizeof(test_sizes)/sizeof(test_sizes[0]); i++) {
        size_t size = test_sizes[i];
        struct timespec start, end;
        double total_time = 0;

        for (int j = 0; j < num_tests; j++) {
            clock_gettime(CLOCK_MONOTONIC, &start);
            if (dma_read_flash(dev, 0, buffer, size) < 0) {
                printf("Failed test at size %zu\n", size);
                break;
            }
            clock_gettime(CLOCK_MONOTONIC, &end);
            
            double time_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                           (end.tv_nsec - start.tv_nsec) / 1000000.0;
            total_time += time_ms;
        }

        results[i].size = size;
        results[i].time_ms = total_time / num_tests;
        results[i].bandwidth_mbps = (size * 8.0) / (results[i].time_ms * 1000.0);

        printf("Size: %6zu bytes, Time: %7.2f ms, Bandwidth: %7.2f Mbps\n",
               size, results[i].time_ms, results[i].bandwidth_mbps);
    }

    free(buffer);
}

int main() {
    spi_device_t spi_dev;
    uint8_t flash_id[3];
    perf_result_t results[5];  // For different test sizes
    const int NUM_TESTS = 10;  // Number of iterations per test

    printf("W25Q80 DMA Test\n");
    printf("==============\n");

    // Initialize SPI
    if (spi_init(&spi_dev, "/dev/spidev0.0") < 0) {
        return -1;
    }

    // Read and verify flash ID
    if (read_flash_id(&spi_dev, flash_id) < 0) {
        close(spi_dev.fd);
        return -1;
    }

    printf("Flash ID: %02X %02X %02X\n", flash_id[0], flash_id[1], flash_id[2]);
    if (flash_id[0] != 0xEF || flash_id[1] != 0x13) {  // Winbond W25Q80 ID
        printf("Unexpected flash ID - not a W25Q80?\n");
        close(spi_dev.fd);
        return -1;
    }

    // Run performance tests
    measure_read_performance(&spi_dev, results, NUM_TESTS);

    // Test random access reads
    printf("\nRandom Access Test:\n");
    printf("-----------------\n");
    uint8_t test_buffer[256];
    uint32_t test_addresses[] = {0, 4096, 8192, 16384, 32768};
    
    for (int i = 0; i < sizeof(test_addresses)/sizeof(test_addresses[0]); i++) {
        if (dma_read_flash(&spi_dev, test_addresses[i], test_buffer, sizeof(test_buffer)) == 0) {
            printf("Address 0x%05X: First bytes: ", test_addresses[i]);
            for (int j = 0; j < 8; j++) {
                printf("%02X ", test_buffer[j]);
            }
            printf("\n");
        }
    }

    close(spi_dev.fd);
    printf("\nTest completed!\n");
    return 0;
}
