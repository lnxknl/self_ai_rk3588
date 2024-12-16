#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <GLES3/gl32.h>
#include <EGL/egl.h>

#define CHECK_ERROR(msg) \
    do { \
        EGLint error = eglGetError(); \
        if (error != EGL_SUCCESS) { \
            printf("%s failed with error: 0x%x\n", msg, error); \
            goto cleanup; \
        } \
    } while (0)

int main() {
    int ret = -1;
    int mali_fd = -1;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    
    printf("Starting Mali GPU Direct Test...\n");

    // Try to open Mali device
    const char *mali_paths[] = {
        "/dev/mali0",
        "/dev/mali",
        NULL
    };

    for (const char **path = mali_paths; *path; path++) {
        mali_fd = open(*path, O_RDWR);
        if (mali_fd >= 0) {
            printf("Successfully opened Mali device: %s\n", *path);
            break;
        }
    }

    if (mali_fd < 0) {
        printf("Failed to open Mali device\n");
        return -1;
    }

    // Try to get EGL display
    printf("Getting EGL display...\n");
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        printf("Failed to get EGL display\n");
        CHECK_ERROR("eglGetDisplay");
    }

    // Initialize EGL
    printf("Initializing EGL...\n");
    EGLint major, minor;
    if (!eglInitialize(display, &major, &minor)) {
        CHECK_ERROR("eglInitialize");
    }

    printf("EGL Version: %d.%d\n", major, minor);
    printf("EGL Vendor: %s\n", eglQueryString(display, EGL_VENDOR));
    printf("EGL Extensions: %s\n", eglQueryString(display, EGL_EXTENSIONS));

    // Configure EGL
    printf("Configuring EGL...\n");
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        CHECK_ERROR("eglBindAPI");
    }

    // Try different configurations
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_config;
    if (!eglChooseConfig(display, config_attribs, &config, 1, &num_config)) {
        CHECK_ERROR("eglChooseConfig");
    }

    // Create pbuffer surface
    printf("Creating surface...\n");
    const EGLint surface_attribs[] = {
        EGL_WIDTH, 16,
        EGL_HEIGHT, 16,
        EGL_NONE
    };

    surface = eglCreatePbufferSurface(display, config, surface_attribs);
    if (surface == EGL_NO_SURFACE) {
        CHECK_ERROR("eglCreatePbufferSurface");
    }

    // Create EGL context
    printf("Creating context...\n");
    const EGLint context_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 2,
        EGL_NONE
    };

    context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
    if (context == EGL_NO_CONTEXT) {
        CHECK_ERROR("eglCreateContext");
    }

    // Make context current
    printf("Making context current...\n");
    if (!eglMakeCurrent(display, surface, surface, context)) {
        CHECK_ERROR("eglMakeCurrent");
    }

    // Print OpenGL ES information
    printf("\nOpenGL ES Information:\n");
    printf("Version: %s\n", glGetString(GL_VERSION));
    printf("Vendor: %s\n", glGetString(GL_VENDOR));
    printf("Renderer: %s\n", glGetString(GL_RENDERER));

    // Simple render test
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        printf("OpenGL error: 0x%x\n", gl_error);
    } else {
        printf("\nRender test successful!\n");
    }

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
    if (mali_fd >= 0)
        close(mali_fd);

    return ret;
}
