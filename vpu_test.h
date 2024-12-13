#ifndef RK3588_VPU_TEST_H
#define RK3588_VPU_TEST_H

#include <stdint.h>
#include <stdbool.h>

// VPU Hardware Capabilities
#define MAX_WIDTH_VDPU121 1920
#define MAX_HEIGHT_VDPU121 1088
#define MAX_WIDTH_VDPU381 65472
#define MAX_HEIGHT_VDPU381 65472
#define MAX_WIDTH_VDPU720 65536
#define MAX_HEIGHT_VDPU720 65536
#define MAX_WIDTH_VEPU121 8192
#define MAX_HEIGHT_VEPU121 8192

// Test Status Codes
typedef enum {
    TEST_SUCCESS = 0,
    TEST_FAIL = -1,
    TEST_UNSUPPORTED = -2
} test_status_t;

// VPU Codec Types
typedef enum {
    CODEC_H264,
    CODEC_H265,
    CODEC_VP8,
    CODEC_VP9,
    CODEC_AV1,
    CODEC_JPEG,
    CODEC_MPEG2,
    CODEC_MPEG4,
    CODEC_H263
} codec_type_t;

// Test Configuration Structure
typedef struct {
    uint32_t width;
    uint32_t height;
    codec_type_t codec;
    uint32_t framerate;
    bool is_10bit;
} test_config_t;

// Function Declarations
test_status_t test_vdpu121(test_config_t *config);
test_status_t test_vdpu381(test_config_t *config);
test_status_t test_vdpu720(test_config_t *config);
test_status_t test_vdpu981(test_config_t *config);
test_status_t test_vepu121(test_config_t *config);
test_status_t test_vepu580(test_config_t *config);

#endif // RK3588_VPU_TEST_H
