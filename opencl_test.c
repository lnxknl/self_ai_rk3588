#define CL_TARGET_OPENCL_VERSION 220
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CL/cl.h>

// Simple OpenCL kernel to perform vector addition
const char *kernel_source = "\n"\
"__kernel void vector_add(__global const float *A,    \n"\
"                        __global const float *B,     \n"\
"                        __global float *C)           \n"\
"{                                                    \n"\
"    int i = get_global_id(0);                       \n"\
"    C[i] = A[i] + B[i];                            \n"\
"}                                                    \n";

#define ARRAY_SIZE 1024
#define MAX_PLATFORMS 4
#define MAX_DEVICES 4

void print_device_info(cl_device_id device) {
    char buffer[1024];
    cl_uint value;
    cl_ulong value_long;
    size_t size;

    // Device name
    clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(buffer), buffer, NULL);
    printf("\nDevice Name: %s\n", buffer);

    // Hardware version
    clGetDeviceInfo(device, CL_DEVICE_VERSION, sizeof(buffer), buffer, NULL);
    printf("Hardware version: %s\n", buffer);

    // Software version
    clGetDeviceInfo(device, CL_DRIVER_VERSION, sizeof(buffer), buffer, NULL);
    printf("Software version: %s\n", buffer);

    // OpenCL C version
    clGetDeviceInfo(device, CL_DEVICE_OPENCL_C_VERSION, sizeof(buffer), buffer, NULL);
    printf("OpenCL C version: %s\n", buffer);

    // Parallel compute units
    clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(value), &value, NULL);
    printf("Parallel compute units: %d\n", value);

    // Clock frequency
    clGetDeviceInfo(device, CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(value), &value, NULL);
    printf("Max clock frequency: %d MHz\n", value);

    // Global memory
    clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(value_long), &value_long, NULL);
    printf("Global memory: %lu MB\n", value_long/(1024*1024));

    // Local memory
    clGetDeviceInfo(device, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(value_long), &value_long, NULL);
    printf("Local memory: %lu KB\n", value_long/1024);

    // Max work group size
    clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size), &size, NULL);
    printf("Max work group size: %zu\n", size);
}

int main() {
    cl_platform_id platforms[MAX_PLATFORMS];
    cl_device_id devices[MAX_DEVICES];
    cl_uint num_platforms, num_devices;
    cl_int err;

    // Get platforms
    err = clGetPlatformIDs(MAX_PLATFORMS, platforms, &num_platforms);
    if (err != CL_SUCCESS) {
        printf("Failed to get platforms: %d\n", err);
//        return -1;
    }
    printf("Found %d OpenCL platforms\n", num_platforms);

    // Iterate through platforms
    for (cl_uint p = 0; p < num_platforms; p++) {
        char platform_name[128];
        char platform_vendor[128];
        char platform_version[128];

        clGetPlatformInfo(platforms[p], CL_PLATFORM_NAME, sizeof(platform_name), platform_name, NULL);
        clGetPlatformInfo(platforms[p], CL_PLATFORM_VENDOR, sizeof(platform_vendor), platform_vendor, NULL);
        clGetPlatformInfo(platforms[p], CL_PLATFORM_VERSION, sizeof(platform_version), platform_version, NULL);

        printf("\nPlatform %d:\n", p);
        printf("Name: %s\n", platform_name);
        printf("Vendor: %s\n", platform_vendor);
        printf("Version: %s\n", platform_version);

        // Get devices for this platform
        err = clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_ALL, MAX_DEVICES, devices, &num_devices);
        if (err != CL_SUCCESS) {
            printf("Failed to get devices for platform %d: %d\n", p, err);
            continue;
        }
        printf("Found %d devices\n", num_devices);

        // Print info for each device
        for (cl_uint d = 0; d < num_devices; d++) {
            printf("\nDevice %d:", d);
            print_device_info(devices[d]);
        }

        // Create a context and command queue for the first device
        cl_context context = clCreateContext(NULL, 1, &devices[0], NULL, NULL, &err);
        if (err != CL_SUCCESS) {
            printf("Failed to create context: %d\n", err);
            continue;
        }

        cl_command_queue queue = clCreateCommandQueue(context, devices[0], 0, &err);
        if (err != CL_SUCCESS) {
            printf("Failed to create command queue: %d\n", err);
            clReleaseContext(context);
            continue;
        }

        // Create and build the program
        cl_program program = clCreateProgramWithSource(context, 1, &kernel_source, NULL, &err);
        if (err != CL_SUCCESS) {
            printf("Failed to create program: %d\n", err);
            clReleaseCommandQueue(queue);
            clReleaseContext(context);
            continue;
        }

        err = clBuildProgram(program, 1, &devices[0], NULL, NULL, NULL);
        if (err != CL_SUCCESS) {
            printf("Failed to build program: %d\n", err);
            size_t log_size;
            clGetProgramBuildInfo(program, devices[0], CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
            char *log = (char *)malloc(log_size);
            clGetProgramBuildInfo(program, devices[0], CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
            printf("Build log:\n%s\n", log);
            free(log);
            clReleaseProgram(program);
            clReleaseCommandQueue(queue);
            clReleaseContext(context);
            continue;
        }

        // Create kernel
        cl_kernel kernel = clCreateKernel(program, "vector_add", &err);
        if (err != CL_SUCCESS) {
            printf("Failed to create kernel: %d\n", err);
            clReleaseProgram(program);
            clReleaseCommandQueue(queue);
            clReleaseContext(context);
            continue;
        }

        // Prepare data
        float *A = (float *)malloc(ARRAY_SIZE * sizeof(float));
        float *B = (float *)malloc(ARRAY_SIZE * sizeof(float));
        float *C = (float *)malloc(ARRAY_SIZE * sizeof(float));

        for (int i = 0; i < ARRAY_SIZE; i++) {
            A[i] = i * 1.0f;
            B[i] = i * 2.0f;
        }

        // Create buffers
        cl_mem bufA = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                   ARRAY_SIZE * sizeof(float), A, &err);
        cl_mem bufB = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                   ARRAY_SIZE * sizeof(float), B, &err);
        cl_mem bufC = clCreateBuffer(context, CL_MEM_WRITE_ONLY,
                                   ARRAY_SIZE * sizeof(float), NULL, &err);

        // Set kernel arguments
        clSetKernelArg(kernel, 0, sizeof(cl_mem), &bufA);
        clSetKernelArg(kernel, 1, sizeof(cl_mem), &bufB);
        clSetKernelArg(kernel, 2, sizeof(cl_mem), &bufC);

        // Execute kernel
        size_t global_size = ARRAY_SIZE;
        err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, NULL, 0, NULL, NULL);
        if (err != CL_SUCCESS) {
            printf("Failed to execute kernel: %d\n", err);
        } else {
            // Read results
            clEnqueueReadBuffer(queue, bufC, CL_TRUE, 0, ARRAY_SIZE * sizeof(float), C, 0, NULL, NULL);

            // Verify results
            int correct = 1;
            for (int i = 0; i < ARRAY_SIZE; i++) {
                if (C[i] != A[i] + B[i]) {
                    correct = 0;
                    break;
                }
            }
            printf("\nVector addition test: %s\n", correct ? "PASSED" : "FAILED");
        }

        // Cleanup
        clReleaseMemObject(bufA);
        clReleaseMemObject(bufB);
        clReleaseMemObject(bufC);
        clReleaseKernel(kernel);
        clReleaseProgram(program);
        clReleaseCommandQueue(queue);
        clReleaseContext(context);
        free(A);
        free(B);
        free(C);
    }

    return 0;
}
