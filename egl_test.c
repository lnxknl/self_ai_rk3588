#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GLES3/gl32.h>
#include <EGL/egl.h>

static const EGLint configAttribs[] = {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_NONE
};

static const EGLint pbufferAttribs[] = {
    EGL_WIDTH, 16,
    EGL_HEIGHT, 16,
    EGL_NONE
};

int main(int argc, char *argv[]) {
    EGLDisplay display;
    EGLConfig config;
    EGLContext context;
    EGLSurface surface;
    EGLint numConfigs;
    EGLint major;
    EGLint minor;

    printf("Starting EGL test...\n");

    // 1. Get EGL display
    printf("Getting display...\n");
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        printf("Failed to get EGL display: 0x%x\n", eglGetError());
        return 1;
    }

    // 2. Initialize EGL
    printf("Initializing EGL...\n");
    if (!eglInitialize(display, &major, &minor)) {
        printf("Failed to initialize EGL: 0x%x\n", eglGetError());
        return 1;
    }

    printf("EGL Version: %d.%d\n", major, minor);
    printf("EGL Vendor: %s\n", eglQueryString(display, EGL_VENDOR));
    printf("EGL Extensions: %s\n", eglQueryString(display, EGL_EXTENSIONS));

    // 3. Select configuration
    printf("Choosing config...\n");
    if (!eglChooseConfig(display, configAttribs, &config, 1, &numConfigs)) {
        printf("Failed to choose config: 0x%x\n", eglGetError());
        return 1;
    }

    // 4. Bind OpenGL ES API
    printf("Binding API...\n");
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        printf("Failed to bind API: 0x%x\n", eglGetError());
        return 1;
    }

    // 5. Create pbuffer surface
    printf("Creating surface...\n");
    surface = eglCreatePbufferSurface(display, config, pbufferAttribs);
    if (surface == EGL_NO_SURFACE) {
        printf("Failed to create surface: 0x%x\n", eglGetError());
        return 1;
    }

    // 6. Create context
    printf("Creating context...\n");
    EGLint contextAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 2,
        EGL_NONE
    };
    
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT) {
        printf("Failed to create context: 0x%x\n", eglGetError());
        return 1;
    }

    // 7. Make current
    printf("Making current...\n");
    if (!eglMakeCurrent(display, surface, surface, context)) {
        printf("Failed to make current: 0x%x\n", eglGetError());
        return 1;
    }

    // 8. Get OpenGL ES info
    printf("\nOpenGL ES Info:\n");
    printf("Version: %s\n", glGetString(GL_VERSION));
    printf("Vendor: %s\n", glGetString(GL_VENDOR));
    printf("Renderer: %s\n", glGetString(GL_RENDERER));

    // 9. Simple render test
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    if (glGetError() != GL_NO_ERROR) {
        printf("OpenGL error occurred\n");
    } else {
        printf("\nRender test passed!\n");
    }

    // 10. Cleanup
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(display, context);
    eglDestroySurface(display, surface);
    eglTerminate(display);

    printf("Test completed successfully!\n");
    return 0;
}
