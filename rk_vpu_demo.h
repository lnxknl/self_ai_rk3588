#ifndef RK_VPU_DEMO_H
#define RK_VPU_DEMO_H

#include <stdio.h>
#include <string.h>
#include <rockchip/rk_mpi.h>
#include <rockchip/mpp_buffer.h>
#include <rockchip/mpp_frame.h>
#include <rockchip/mpp_packet.h>

// Maximum frame width and height
#define MAX_FRAME_WIDTH   3840
#define MAX_FRAME_HEIGHT  2160

// Buffer size for reading input stream
#define READ_BUF_SIZE     (SZ_1M)
#define MAX_FRAMES        300

typedef struct {
    // Input/output file handles
    FILE            *fp_input;
    FILE            *fp_output;
    
    // MPP contexts
    MppCtx          ctx;
    MppApi         *mpi;
    
    // Packet for input data
    MppPacket       packet;
    // Frame for decoded data
    MppFrame        frame;
    
    // Buffer group for decoder
    MppBufferGroup  frm_grp;
    MppBuffer       pkt_buf;
    MppBuffer       frm_buf;
    
    // Frame counter
    RK_U32          frame_count;
    // Frame size
    RK_U32          frame_size;
    
    // Parameters
    RK_U32          width;
    RK_U32          height;
    MppCodingType   type;
    
    // Buffer for reading input stream
    char           *buf;
    size_t          buf_size;
    size_t          packet_size;
} VpuDecContext;

// Function declarations
MPP_RET init_vpu_decoder(VpuDecContext *ctx, const char *input_file, const char *output_file);
MPP_RET decode_frames(VpuDecContext *ctx);
void deinit_vpu_decoder(VpuDecContext *ctx);

#endif // RK_VPU_DEMO_H
