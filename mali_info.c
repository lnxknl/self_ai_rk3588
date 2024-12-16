#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

void check_mali_driver() {
    printf("Checking Mali GPU Driver Status:\n");
    printf("================================\n");

    // Check kernel modules
    FILE *fp = popen("lsmod | grep mali", "r");
    if (fp) {
        char buffer[256];
        printf("\nLoaded Mali Kernel Modules:\n");
        while (fgets(buffer, sizeof(buffer), fp)) {
            printf("%s", buffer);
        }
        pclose(fp);
    }

    // Check DRM devices
    printf("\nDRM Devices:\n");
    DIR *dir = opendir("/dev/dri");
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, "card", 4) == 0 ||
                strncmp(entry->d_name, "renderD", 7) == 0) {
                char path[256];
                snprintf(path, sizeof(path), "/dev/dri/%s", entry->d_name);
                struct stat st;
                if (stat(path, &st) == 0) {
                    printf("%s: %s (permissions: %o)\n", 
                           path, 
                           S_ISCHR(st.st_mode) ? "character device" : "unknown",
                           st.st_mode & 0777);
                }
            }
        }
        closedir(dir);
    }

    // Check device nodes
    printf("\nMali Device Nodes:\n");
    const char *mali_paths[] = {
        "/dev/mali",
        "/dev/mali0",
        "/dev/umplock",
        "/dev/graphics/fb0",
        NULL
    };

    for (const char **path = mali_paths; *path; path++) {
        struct stat st;
        if (stat(*path, &st) == 0) {
            printf("%s: exists (permissions: %o)\n", *path, st.st_mode & 0777);
        }
    }

    // Check driver info from sysfs
    printf("\nGPU Information from sysfs:\n");
    const char *sysfs_paths[] = {
        "/sys/class/gpu",
        "/sys/class/misc/mali0",
        "/sys/kernel/debug/mali",
        NULL
    };

    for (const char **path = sysfs_paths; *path; path++) {
        DIR *dir = opendir(*path);
        if (dir) {
            printf("\nContents of %s:\n", *path);
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] != '.') {
                    printf("  %s\n", entry->d_name);
                }
            }
            closedir(dir);
        }
    }

    // Check user permissions
    printf("\nUser and Group Information:\n");
    system("id");
    system("groups");
}

void check_gpu_devices() {
    printf("\nChecking Available GPU Devices:\n");
    printf("==============================\n");

    // Try to open and read from card0
    int fd = open("/dev/dri/card0", O_RDWR);
    if (fd >= 0) {
        printf("/dev/dri/card0: Successfully opened\n");
        close(fd);
    } else {
        printf("/dev/dri/card0: Failed to open (errno: %d)\n", errno);
        perror("Error");
    }

    // Check render node
    fd = open("/dev/dri/renderD128", O_RDWR);
    if (fd >= 0) {
        printf("/dev/dri/renderD128: Successfully opened\n");
        close(fd);
    } else {
        printf("/dev/dri/renderD128: Failed to open (errno: %d)\n", errno);
        perror("Error");
    }
}

int main() {
    printf("Mali GPU Driver Check Tool\n");
    printf("=========================\n\n");

    check_mali_driver();
    check_gpu_devices();

    return 0;
}
