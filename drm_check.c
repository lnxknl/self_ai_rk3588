#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

void check_drm_device(const char *path) {
    int fd;
    drmVersionPtr version;
    
    printf("\nChecking DRM device: %s\n", path);
    printf("---------------------------\n");
    
    // Check if file exists and get permissions
    struct stat st;
    if (stat(path, &st) == 0) {
        printf("File exists\n");
        printf("Permissions: %o\n", st.st_mode & 0777);
        printf("Owner: %d, Group: %d\n", st.st_uid, st.st_gid);
    } else {
        printf("File does not exist: %s\n", strerror(errno));
        return;
    }
    
    // Try to open the device
    fd = open(path, O_RDWR);
    if (fd < 0) {
        printf("Failed to open device: %s\n", strerror(errno));
        return;
    }
    printf("Successfully opened device\n");
    
    // Check if it's a DRM device
    if (drmGetVersion(fd)) {
        printf("Valid DRM device detected\n");
        
        // Get driver version
        version = drmGetVersion(fd);
        if (version) {
            printf("Driver name: %s\n", version->name);
            printf("Driver version: %d.%d.%d\n",
                   version->version_major,
                   version->version_minor,
                   version->version_patchlevel);
            printf("Driver date: %s\n", version->date);
            printf("Driver description: %s\n", version->desc);
            drmFreeVersion(version);
        }
        
        // Try to get resources
        drmModeRes *resources = drmModeGetResources(fd);
        if (resources) {
            printf("\nDRM Resources:\n");
            printf("FB count: %d\n", resources->count_fbs);
            printf("CRTC count: %d\n", resources->count_crtcs);
            printf("Connector count: %d\n", resources->count_connectors);
            printf("Encoder count: %d\n", resources->count_encoders);
            
            // Print connector information
            printf("\nConnectors:\n");
            for (int i = 0; i < resources->count_connectors; i++) {
                drmModeConnector *conn = drmModeGetConnector(fd, resources->connectors[i]);
                if (conn) {
                    printf("Connector %d:\n", i);
                    printf("  ID: %d\n", conn->connector_id);
                    printf("  Type: %d\n", conn->connector_type);
                    printf("  Status: %s\n", 
                           conn->connection == DRM_MODE_CONNECTED ? "connected" :
                           conn->connection == DRM_MODE_DISCONNECTED ? "disconnected" : "unknown");
                    printf("  Modes: %d\n", conn->count_modes);
                    drmModeFreeConnector(conn);
                }
            }
            
            drmModeFreeResources(resources);
        } else {
            printf("Failed to get DRM resources: %s\n", strerror(errno));
        }
    } else {
        printf("Not a valid DRM device\n");
    }
    
    close(fd);
}

void list_drm_devices() {
    DIR *dir;
    struct dirent *entry;
    char path[256];
    
    printf("Scanning /dev/dri/ directory:\n");
    printf("============================\n");
    
    dir = opendir("/dev/dri");
    if (!dir) {
        printf("Failed to open /dev/dri directory: %s\n", strerror(errno));
        return;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "card", 4) == 0 ||
            strncmp(entry->d_name, "renderD", 7) == 0) {
            snprintf(path, sizeof(path), "/dev/dri/%s", entry->d_name);
            check_drm_device(path);
        }
    }
    
    closedir(dir);
}

int main() {
    printf("DRM Device Check Tool\n");
    printf("====================\n");
    
    // Check user and group info
    printf("\nUser Information:\n");
    printf("----------------\n");
    system("id");
    
    // List loaded DRM modules
    printf("\nLoaded DRM Modules:\n");
    printf("------------------\n");
    system("lsmod | grep drm");
    
    // Check all DRM devices
    list_drm_devices();
    
    return 0;
}
