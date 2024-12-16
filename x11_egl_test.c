#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GLES3/gl32.h>
#include <EGL/egl.h>

int main(int argc, char *argv[]) {
    Display *x_display;
    Window x_window;
    EGLDisplay display;
    EGLConfig config;
    EGLContext context;
    EGLSurface surface;
    EGLint num_config;
    EGLint major, minor;

    printf("Starting X11 EGL test...\n");

    // Open X display
    printf("Opening X display...\n");
    x_display = XOpenDisplay(NULL);
    if (x_display == NULL) {
        printf("Failed to open X display\n");
        return 1;
    }

    // Get default screen
    Window root = DefaultRootWindow(x_display);
    XSetWindowAttributes swa;
    swa.event_mask = ExposureMask | PointerMotionMask | KeyPressMask;
    
    // Create window
    x_window = XCreateWindow(
        x_display, root,
        0, 0, 100, 100, 0,
        CopyFromParent, InputOutput,
        CopyFromParent, CWEventMask,
        &swa);

    // Show window
    XMapWindow(x_display, x_window);
    XStoreName(x_display, x_window, "EGL Test");

    // Get EGL display
    printf("Getting EGL display...\n");
    display = eglGetDisplay((EGLNativeDisplayType)x_display);
    if (display == EGL_NO_DISPLAY) {
        printf("Failed to get EGL display: 0x%x\n", eglGetError());
        return 1;
    }

    // Initialize EGL
    printf("Initializing EGL...\n");
    if (!eglInitialize(display, &major, &minor)) {
        printf("Failed to initialize EGL: 0x%x\n", eglGetError());
        return 1;
    }

    printf("EGL Version: %d.%d\n", major, minor);
    printf("EGL Vendor: %s\n", eglQueryString(display, EGL_VENDOR));
    printf("EGL Extensions: %s\n", eglQueryString(display, EGL_EXTENSIONS));

    // Configure EGL
    EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    // Choose config
    if (!eglChooseConfig(display, attribs, &config, 1, &num_config)) {
        printf("Failed to choose config: 0x%x\n", eglGetError());
        return 1;
    }

    // Create EGL surface
    surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType)x_window, NULL);
    if (surface == EGL_NO_SURFACE) {
        printf("Failed to create EGL surface: 0x%x\n", eglGetError());
        return 1;
    }

    // Create EGL context
    EGLint contextAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 2,
        EGL_NONE
    };

    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT) {
        printf("Failed to create EGL context: 0x%x\n", eglGetError());
        return 1;
    }

    // Make context current
    if (!eglMakeCurrent(display, surface, surface, context)) {
        printf("Failed to make context current: 0x%x\n", eglGetError());
        return 1;
    }

    // Print OpenGL ES info
    printf("\nOpenGL ES Info:\n");
    printf("Version: %s\n", glGetString(GL_VERSION));
    printf("Vendor: %s\n", glGetString(GL_VENDOR));
    printf("Renderer: %s\n", glGetString(GL_RENDERER));

    // Clear screen
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(display, surface);

    // Wait for keypress
    printf("\nPress any key in the window to exit...\n");
    XEvent xev;
    while (1) {
        XNextEvent(x_display, &xev);
        if (xev.type == KeyPress)
            break;
    }

    // Cleanup
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(display, context);
    eglDestroySurface(display, surface);
    eglTerminate(display);
    XDestroyWindow(x_display, x_window);
    XCloseDisplay(x_display);

    return 0;
}
