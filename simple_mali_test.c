#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
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
    
    // Open DRM device
    drm_fd = open("/dev/dri/card0", O_RDWR);
    if (drm_fd < 0) {
        printf("Failed to open DRM device\n");
        return -1;
    }
    
    // Create GBM device
    gbm = gbm_create_device(drm_fd);
    if (!gbm) {
        printf("Failed to create GBM device\n");
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
        printf("Failed to initialize EGL\n");
        goto cleanup;
    }
    
    printf("EGL Version: %d.%d\n", major, minor);
    printf("EGL Vendor: %s\n", eglQueryString(display, EGL_VENDOR));
    printf("EGL Extensions: %s\n", eglQueryString(display, EGL_EXTENSIONS));
    
    // Bind OpenGL ES API
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        EXIT_ON_ERROR("eglBindAPI");
    }
    
    // Configure EGL
    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    
    EGLConfig config;
    EGLint num_configs;
    if (!eglChooseConfig(display, config_attribs, &config, 1, &num_configs)) {
        EXIT_ON_ERROR("eglChooseConfig");
    }
    
    // Create GBM surface
    struct gbm_surface *gbm_surface = gbm_surface_create(
        gbm, 64, 64, // Minimal size for testing
        GBM_FORMAT_XRGB8888,
        GBM_BO_USE_RENDERING
    );
    if (!gbm_surface) {
        EXIT_ON_ERROR("gbm_surface_create");
    }
    
    // Create EGL surface
    surface = eglCreateWindowSurface(display, config, 
                                   (EGLNativeWindowType)gbm_surface, NULL);
    if (surface == EGL_NO_SURFACE) {
        EXIT_ON_ERROR("eglCreateWindowSurface");
    }
    
    // Create EGL context
    const EGLint context_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 2,
        EGL_NONE
    };
    
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
    if (context == EGL_NO_CONTEXT) {
        EXIT_ON_ERROR("eglCreateContext");
    }
    
    // Make context current
    if (!eglMakeCurrent(display, surface, surface, context)) {
        EXIT_ON_ERROR("eglMakeCurrent");
    }
    
    // Print OpenGL ES information
    printf("\nOpenGL ES Information:\n");
    printf("Version: %s\n", glGetString(GL_VERSION));
    printf("Vendor: %s\n", glGetString(GL_VENDOR));
    printf("Renderer: %s\n", glGetString(GL_RENDERER));
    
    // Simple rendering test
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(display, surface);
    
    printf("\nGPU test completed successfully!\n");
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
    if (gbm)
        gbm_device_destroy(gbm);
    if (drm_fd >= 0)
        close(drm_fd);
    
    return ret;
}
