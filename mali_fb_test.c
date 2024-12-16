#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
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
    int fb_fd = -1;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    
    printf("Starting Mali GPU Framebuffer Test...\n");

    // Open framebuffer device
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        printf("Failed to open framebuffer device: %s\n", strerror(errno));
        return -1;
    }

    // Get variable screen information
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        printf("Failed to get variable screen info: %s\n", strerror(errno));
        goto cleanup;
    }

    // Get fixed screen information
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        printf("Failed to get fixed screen info: %s\n", strerror(errno));
        goto cleanup;
    }

    printf("Framebuffer Information:\n");
    printf("Resolution: %dx%d\n", vinfo.xres, vinfo.yres);
    printf("Bits per pixel: %d\n", vinfo.bits_per_pixel);
    printf("Frame buffer memory: %s\n", finfo.id);
    printf("Line length: %d\n", finfo.line_length);

    // Try to get EGL display using framebuffer
    printf("\nGetting EGL display...\n");
    display = eglGetDisplay((EGLNativeDisplayType)fb_fd);
    if (display == EGL_NO_DISPLAY) {
        printf("Failed with fb_fd, trying DEFAULT_DISPLAY...\n");
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display == EGL_NO_DISPLAY) {
            printf("Failed to get EGL display\n");
            CHECK_ERROR("eglGetDisplay");
        }
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
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, vinfo.red.length,
        EGL_GREEN_SIZE, vinfo.green.length,
        EGL_BLUE_SIZE, vinfo.blue.length,
        EGL_ALPHA_SIZE, vinfo.transp.length,
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_config;
    if (!eglChooseConfig(display, config_attribs, &config, 1, &num_config)) {
        CHECK_ERROR("eglChooseConfig");
    }

    // Create window surface
    printf("Creating surface...\n");
    surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType)fb_fd, NULL);
    if (surface == EGL_NO_SURFACE) {
        CHECK_ERROR("eglCreateWindowSurface");
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
    glViewport(0, 0, vinfo.xres, vinfo.yres);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(display, surface);

    GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        printf("OpenGL error: 0x%x\n", gl_error);
    } else {
        printf("\nRender test successful!\n");
        printf("Screen should now be red. Press Enter to exit...\n");
        getchar();
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
    if (fb_fd >= 0)
        close(fb_fd);

    return ret;
}
