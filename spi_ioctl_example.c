#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

// Example showing different SPI_IOC_MESSAGE usage patterns

void example_single_transfer(int fd) {
    // Single transfer example
    uint8_t tx[] = {0x9F, 0, 0, 0};  // READ ID command
    uint8_t rx[4] = {0};
    
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,    // Data to send
        .rx_buf = (unsigned long)rx,    // Buffer for received data
        .len = sizeof(tx),              // Length of transfer
        .speed_hz = 1000000,           // Speed in Hz (1MHz)
        .bits_per_word = 8,            // 8 bits per word
        .delay_usecs = 0,              // Delay after transfer
        .cs_change = 0,                // Keep CS active between transfers
    };

    // Single message transfer
    if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        perror("SPI transfer failed");
    }
}

void example_multi_transfer(int fd) {
    // Multiple transfer example (common in display drivers)
    uint8_t command = 0x3C;           // Command byte
    uint8_t data[3] = {0x12, 0x34, 0x56};
    
    struct spi_ioc_transfer tr[2] = {
        // First transfer: Command
        {
            .tx_buf = (unsigned long)&command,
            .rx_buf = 0,               // No receive for command
            .len = 1,
            .speed_hz = 1000000,
            .bits_per_word = 8,
            .delay_usecs = 0,
            .cs_change = 0,            // Keep CS active for next transfer
        },
        // Second transfer: Data
        {
            .tx_buf = (unsigned long)data,
            .rx_buf = 0,               // No receive for data
            .len = sizeof(data),
            .speed_hz = 1000000,
            .bits_per_word = 8,
            .delay_usecs = 0,
            .cs_change = 1,            // Release CS after transfer
        }
    };

    // Multiple message transfer
    if (ioctl(fd, SPI_IOC_MESSAGE(2), tr) < 0) {
        perror("SPI multi-transfer failed");
    }
}

void example_dma_transfer(int fd) {
    // DMA-friendly transfer example
    #define DMA_BUF_SIZE 4096
    
    // Allocate DMA-friendly buffers (page-aligned)
    void* tx_buf = NULL;
    void* rx_buf = NULL;
    posix_memalign(&tx_buf, 4096, DMA_BUF_SIZE);
    posix_memalign(&rx_buf, 4096, DMA_BUF_SIZE);

    if (!tx_buf || !rx_buf) {
        perror("Buffer allocation failed");
        return;
    }

    // Setup large transfer suitable for DMA
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx_buf,
        .rx_buf = (unsigned long)rx_buf,
        .len = DMA_BUF_SIZE,
        .speed_hz = 10000000,          // 10MHz
        .bits_per_word = 8,
        .delay_usecs = 0,
        .cs_change = 0,
    };

    // Large transfer that will likely use DMA
    if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        perror("SPI DMA transfer failed");
    }

    free(tx_buf);
    free(rx_buf);
}

void example_full_duplex(int fd) {
    // Full-duplex transfer example
    uint8_t tx[] = {0x12, 0x34, 0x56, 0x78};
    uint8_t rx[sizeof(tx)] = {0};
    
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,   // Receive buffer same size as transmit
        .len = sizeof(tx),
        .speed_hz = 1000000,
        .bits_per_word = 8,
        .delay_usecs = 0,
        .cs_change = 0,
    };

    // Full duplex transfer (simultaneous TX and RX)
    if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        perror("SPI full-duplex transfer failed");
    }
}

int main() {
    int fd = open("/dev/spidev0.0", O_RDWR);
    if (fd < 0) {
        perror("Can't open device");
        return -1;
    }

    printf("SPI Transfer Examples:\n");
    printf("1. Single Transfer\n");
    example_single_transfer(fd);

    printf("2. Multiple Transfers\n");
    example_multi_transfer(fd);

    printf("3. DMA Transfer\n");
    example_dma_transfer(fd);

    printf("4. Full Duplex Transfer\n");
    example_full_duplex(fd);

    close(fd);
    return 0;
}
