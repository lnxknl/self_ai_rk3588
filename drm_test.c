#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <GLES3/gl32.h>
#include <EGL/egl.h>

#define EXIT_ON_ERROR(msg) \
    do { \
        printf("%s failed\n", msg); \
        goto cleanup; \
    } while (0)

int main() {
    int ret = -1;
    int drm_fd = -1;
    struct gbm_device *gbm = NULL;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    drmModeRes *resources = NULL;
    drmModeConnector *connector = NULL;
    drmModeModeInfo *mode = NULL;
    drmModeCrtc *crtc = NULL;
    struct gbm_surface *gbm_surface = NULL;
    uint32_t connector_id;
    
    printf("Starting DRM/GBM/EGL test...\n");

    // Open DRM device
    drm_fd = open("/dev/dri/card0", O_RDWR);
    if (drm_fd < 0) {
        printf("Failed to open DRM device\n");
        return -1;
    }
    
    // Get DRM resources
    resources = drmModeGetResources(drm_fd);
    if (!resources) {
        printf("Failed to get DRM resources\n");
        goto cleanup;
    }

    // Find a connected connector
    for (int i = 0; i < resources->count_connectors; i++) {
        connector = drmModeGetConnector(drm_fd, resources->connectors[i]);
        if (connector->connection == DRM_MODE_CONNECTED) {
            connector_id = connector->connector_id;
            mode = &connector->modes[0];  // Use first mode
            break;
        }
        drmModeFreeConnector(connector);
        connector = NULL;
    }

    if (!connector) {
        printf("No connected DRM connector found\n");
        goto cleanup;
    }

    printf("Found connector: id=%d, modes=%d\n", 
           connector_id, connector->count_modes);
    printf("Using mode: %dx%d\n", mode->hdisplay, mode->vdisplay);

    // Create GBM device
    gbm = gbm_create_device(drm_fd);
    if (!gbm) {
        printf("Failed to create GBM device\n");
        goto cleanup;
    }

    // Create GBM surface
    gbm_surface = gbm_surface_create(
        gbm,
        mode->hdisplay, mode->vdisplay,
        GBM_FORMAT_XRGB8888,
        GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING
    );
    if (!gbm_surface) {
        printf("Failed to create GBM surface\n");
        goto cleanup;
    }

    // Get EGL display
    display = eglGetDisplay((EGLNativeDisplayType)gbm);
    if (display == EGL_NO_DISPLAY) {
        printf("Failed to get EGL display\n");
        goto cleanup;
    }

    // Initialize EGL
    EGLint major, minor;
    if (!eglInitialize(display, &major, &minor)) {
        printf("Failed to initialize EGL: 0x%x\n", eglGetError());
        goto cleanup;
    }

    printf("EGL Version: %d.%d\n", major, minor);
    printf("EGL Vendor: %s\n", eglQueryString(display, EGL_VENDOR));
    printf("EGL Extensions: %s\n", eglQueryString(display, EGL_EXTENSIONS));

    // Configure EGL
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        printf("Failed to bind OpenGL ES API\n");
        goto cleanup;
    }

    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 0,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_config;
    if (!eglChooseConfig(display, config_attribs, &config, 1, &num_config)) {
        printf("Failed to choose config: 0x%x\n", eglGetError());
        goto cleanup;
    }

    // Create EGL surface
    surface = eglCreateWindowSurface(display, config, 
                                   (EGLNativeWindowType)gbm_surface, NULL);
    if (surface == EGL_NO_SURFACE) {
        printf("Failed to create EGL surface: 0x%x\n", eglGetError());
        goto cleanup;
    }

    // Create EGL context
    const EGLint context_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 2,
        EGL_NONE
    };

    context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
    if (context == EGL_NO_CONTEXT) {
        printf("Failed to create EGL context: 0x%x\n", eglGetError());
        goto cleanup;
    }

    // Make context current
    if (!eglMakeCurrent(display, surface, surface, context)) {
        printf("Failed to make context current: 0x%x\n", eglGetError());
        goto cleanup;
    }

    // Print OpenGL ES information
    printf("\nOpenGL ES Information:\n");
    printf("Version: %s\n", glGetString(GL_VERSION));
    printf("Vendor: %s\n", glGetString(GL_VENDOR));
    printf("Renderer: %s\n", glGetString(GL_RENDERER));

    // Simple render test
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(display, surface);

    printf("\nRender test completed!\n");
    printf("Press Enter to exit...\n");
    getchar();

    ret = 0;

cleanup:
    if (context != EGL_NO_CONTEXT)
        eglDestroyContext(display, context);
    if (surface != EGL_NO_SURFACE)
        eglDestroySurface(display, surface);
    if (display != EGL_NO_DISPLAY) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglTerminate(display);
    }
    if (gbm_surface)
        gbm_surface_destroy(gbm_surface);
    if (gbm)
        gbm_device_destroy(gbm);
    if (connector)
        drmModeFreeConnector(connector);
    if (resources)
        drmModeFreeResources(resources);
    if (drm_fd >= 0)
        close(drm_fd);

    return ret;
}
