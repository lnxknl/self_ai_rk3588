#include <GLES3/gl32.h>
#include <EGL/egl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define COMPUTE_SIZE 1024
#define TEST_ITERATIONS 1000

// Vertex shader for graphics test
const char* vertexShaderSource = 
    "#version 320 es\n"
    "layout(location = 0) in vec4 vPosition;\n"
    "layout(location = 1) in vec4 vColor;\n"
    "out vec4 fragColor;\n"
    "uniform float uScale;\n"
    "void main() {\n"
    "    gl_Position = vec4(vPosition.xy * uScale, vPosition.zw);\n"
    "    fragColor = vColor;\n"
    "}\n";

// Fragment shader for graphics test
const char* fragmentShaderSource = 
    "#version 320 es\n"
    "precision mediump float;\n"
    "in vec4 fragColor;\n"
    "out vec4 outColor;\n"
    "void main() {\n"
    "    outColor = fragColor;\n"
    "}\n";

// Compute shader for parallel processing test
const char* computeShaderSource = 
    "#version 320 es\n"
    "layout(local_size_x = 256) in;\n"
    "layout(std430, binding = 0) buffer InputBuffer {\n"
    "    float data[];\n"
    "} input_buffer;\n"
    "layout(std430, binding = 1) buffer OutputBuffer {\n"
    "    float data[];\n"
    "} output_buffer;\n"
    "void main() {\n"
    "    uint gid = gl_GlobalInvocationID.x;\n"
    "    if (gid < input_buffer.data.length()) {\n"
    "        float value = input_buffer.data[gid];\n"
    "        // Perform complex computation\n"
    "        value = sin(value) * cos(value) * sqrt(abs(value));\n"
    "        output_buffer.data[gid] = value;\n"
    "    }\n"
    "}\n";

// Shader compilation helper
GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, sizeof(infoLog), NULL, infoLog);
        printf("Shader compilation failed: %s\n", infoLog);
        return 0;
    }
    return shader;
}

// Program linking helper
GLuint createProgram(GLuint vertexShader, GLuint fragmentShader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, sizeof(infoLog), NULL, infoLog);
        printf("Program linking failed: %s\n", infoLog);
        return 0;
    }
    return program;
}

// Initialize EGL
EGLDisplay initEGL() {
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        printf("Failed to get EGL display\n");
        return NULL;
    }
    
    if (!eglInitialize(display, NULL, NULL)) {
        printf("Failed to initialize EGL\n");
        return NULL;
    }
    
    return display;
}

// Create EGL surface
EGLSurface createEGLSurface(EGLDisplay display, EGLConfig config) {
    EGLint surfaceAttribs[] = {
        EGL_WIDTH, WINDOW_WIDTH,
        EGL_HEIGHT, WINDOW_HEIGHT,
        EGL_NONE
    };
    
    EGLSurface surface = eglCreatePbufferSurface(display, config, surfaceAttribs);
    if (surface == EGL_NO_SURFACE) {
        printf("Failed to create EGL surface\n");
        return NULL;
    }
    
    return surface;
}

// Test graphics performance
void testGraphicsPerformance(GLuint program) {
    printf("\nTesting Graphics Performance:\n");
    printf("----------------------------\n");
    
    // Vertex data for a rotating triangle
    float vertices[] = {
         0.0f,  0.5f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.0f, 1.0f
    };
    
    float colors[] = {
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f
    };
    
    // Create vertex buffers
    GLuint vbo[2];
    glGenBuffers(2, vbo);
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
    
    // Set up vertex attributes
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0);
    
    // Measure rendering performance
    GLuint scaleLocation = glGetUniformLocation(program, "uScale");
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        float scale = 0.5f + 0.5f * sin(i * 0.01f);
        glUniform1f(scaleLocation, scale);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    
    glFinish();
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                    (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Graphics test: %.2f FPS\n", TEST_ITERATIONS / elapsed);
    
    // Cleanup
    glDeleteBuffers(2, vbo);
}

// Test compute performance
void testComputePerformance(GLuint computeProgram) {
    printf("\nTesting Compute Performance:\n");
    printf("---------------------------\n");
    
    // Create input and output buffers
    float* inputData = (float*)malloc(COMPUTE_SIZE * sizeof(float));
    for (int i = 0; i < COMPUTE_SIZE; i++) {
        inputData[i] = (float)i;
    }
    
    GLuint ssbo[2];
    glGenBuffers(2, ssbo);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[0]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, COMPUTE_SIZE * sizeof(float), 
                 inputData, GL_STATIC_DRAW);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo[1]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, COMPUTE_SIZE * sizeof(float), 
                 NULL, GL_STATIC_READ);
    
    // Bind buffers to compute shader
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo[0]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo[1]);
    
    // Run compute shader and measure performance
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        glDispatchCompute(COMPUTE_SIZE / 256, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }
    
    glFinish();
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                    (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Compute test: %.2f million operations per second\n",
           (TEST_ITERATIONS * COMPUTE_SIZE) / (elapsed * 1e6));
    
    // Cleanup
    free(inputData);
    glDeleteBuffers(2, ssbo);
}

int main() {
    // Initialize EGL
    EGLDisplay display = initEGL();
    if (!display) return 1;
    
    // Configure EGL
    EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };
    
    EGLConfig config;
    EGLint numConfigs;
    if (!eglChooseConfig(display, configAttribs, &config, 1, &numConfigs)) {
        printf("Failed to choose EGL config\n");
        return 1;
    }
    
    // Create EGL surface
    EGLSurface surface = createEGLSurface(display, config);
    if (!surface) return 1;
    
    // Create EGL context
    EGLint contextAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 2,
        EGL_NONE
    };
    
    EGLContext context = eglCreateContext(display, config, 
                                        EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT) {
        printf("Failed to create EGL context\n");
        return 1;
    }
    
    if (!eglMakeCurrent(display, surface, surface, context)) {
        printf("Failed to make EGL context current\n");
        return 1;
    }
    
    printf("Mali-G610 GPU Test\n");
    printf("==================\n");
    printf("OpenGL ES Version: %s\n", glGetString(GL_VERSION));
    printf("GPU Vendor: %s\n", glGetString(GL_VENDOR));
    printf("GPU Renderer: %s\n", glGetString(GL_RENDERER));
    
    // Create graphics program
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    GLuint graphicsProgram = createProgram(vertexShader, fragmentShader);
    
    // Create compute program
    GLuint computeShader = compileShader(GL_COMPUTE_SHADER, computeShaderSource);
    GLuint computeProgram = glCreateProgram();
    glAttachShader(computeProgram, computeShader);
    glLinkProgram(computeProgram);
    
    // Set up viewport
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    
    // Run performance tests
    glUseProgram(graphicsProgram);
    testGraphicsPerformance(graphicsProgram);
    
    glUseProgram(computeProgram);
    testComputePerformance(computeProgram);
    
    // Cleanup
    glDeleteProgram(graphicsProgram);
    glDeleteProgram(computeProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glDeleteShader(computeShader);
    
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(display, context);
    eglDestroySurface(display, surface);
    eglTerminate(display);
    
    return 0;
}
