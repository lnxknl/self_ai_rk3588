#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

void check_file(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        printf("Found: %s\n", path);
        printf("  Size: %ld bytes\n", st.st_size);
        printf("  Permissions: %o\n", st.st_mode & 0777);
        printf("  Owner: %d, Group: %d\n", st.st_uid, st.st_gid);
    } else {
        printf("Not found: %s\n", path);
    }
}

void check_directory(const char* path) {
    DIR *dir = opendir(path);
    if (dir) {
        printf("\nContents of %s:\n", path);
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] != '.') {  // Skip hidden files
                char full_path[512];
                snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
                check_file(full_path);
            }
        }
        closedir(dir);
    } else {
        printf("Directory not found: %s\n", path);
    }
}

int main() {
    printf("OpenCL Environment Check\n");
    printf("=======================\n");

    // Check common OpenCL ICD paths
    printf("\nChecking OpenCL ICD loaders:\n");
    check_file("/etc/OpenCL/vendors/mali.icd");
    check_file("/etc/OpenCL/vendors/arm.icd");
    check_file("/usr/lib/OpenCL/vendors/mali.icd");
    check_file("/usr/share/OpenCL/vendors/mali.icd");

    // Check Mali library paths
    printf("\nChecking Mali libraries:\n");
    check_file("/usr/lib/libmali.so");
    check_file("/usr/lib/arm-linux-gnueabihf/libmali.so");
    check_file("/usr/lib/aarch64-linux-gnu/libmali.so");
    
    // Check OpenCL library paths
    printf("\nChecking OpenCL libraries:\n");
    check_file("/usr/lib/libOpenCL.so");
    check_file("/usr/lib/libOpenCL.so.1");
    check_file("/usr/lib/aarch64-linux-gnu/libOpenCL.so");
    check_file("/usr/lib/aarch64-linux-gnu/libOpenCL.so.1");

    // Check vendor directories
    printf("\nChecking vendor directories:\n");
    check_directory("/etc/OpenCL/vendors");
    check_directory("/usr/lib/OpenCL/vendors");

    // Check environment variables
    printf("\nChecking environment variables:\n");
    const char* env_vars[] = {
        "OPENCL_VENDOR_PATH",
        "LD_LIBRARY_PATH",
        "PATH",
        NULL
    };

    for (const char** var = env_vars; *var; var++) {
        const char* value = getenv(*var);
        printf("%s=%s\n", *var, value ? value : "not set");
    }

    // Check system configuration
    printf("\nSystem configuration:\n");
    system("ldconfig -p | grep -i mali");
    system("ldconfig -p | grep -i opencl");

    return 0;
}
