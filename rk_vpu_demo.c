#include "rk_vpu_demo.h"
#include <stdlib.h>
#include <unistd.h>
#include <rockchip/mpp_log.h>

MPP_RET init_vpu_decoder(VpuDecContext *ctx, const char *input_file, const char *output_file)
{
    MPP_RET ret = MPP_OK;
    MppCodingType coding = MPP_VIDEO_CodingAVC;  // H.264 decoder

    if (!ctx || !input_file || !output_file)
        return MPP_ERR_NULL_PTR;

    memset(ctx, 0, sizeof(VpuDecContext));

    // Open input and output files
    ctx->fp_input = fopen(input_file, "rb");
    if (!ctx->fp_input) {
        mpp_err("Failed to open input file %s\n", input_file);
        return MPP_ERR_OPEN_FILE;
    }

    ctx->fp_output = fopen(output_file, "wb");
    if (!ctx->fp_output) {
        mpp_err("Failed to open output file %s\n", output_file);
        fclose(ctx->fp_input);
        return MPP_ERR_OPEN_FILE;
    }

    // Create MPP context and decoder
    ret = mpp_create(&ctx->ctx, &ctx->mpi);
    if (ret) {
        mpp_err("mpp_create failed\n");
        goto ERR_RET;
    }

    // Initialize decoder
    ret = mpp_init(ctx->ctx, MPP_CTX_DEC, coding);
    if (ret) {
        mpp_err("mpp_init failed\n");
        goto ERR_RET;
    }

    // Setup frame buffer group
    ret = mpp_buffer_group_get_internal(&ctx->frm_grp, MPP_BUFFER_TYPE_DRM);
    if (ret) {
        mpp_err("Failed to get buffer group\n");
        goto ERR_RET;
    }

    // Allocate packet buffer
    ctx->buf_size = READ_BUF_SIZE;
    ret = mpp_buffer_get(ctx->frm_grp, &ctx->pkt_buf, ctx->buf_size);
    if (ret) {
        mpp_err("Failed to get packet buffer\n");
        goto ERR_RET;
    }

    ctx->buf = mpp_buffer_get_ptr(ctx->pkt_buf);
    ctx->type = coding;
    ctx->packet_size = 0;
    ctx->frame_count = 0;

    return MPP_OK;

ERR_RET:
    deinit_vpu_decoder(ctx);
    return ret;
}

MPP_RET decode_frames(VpuDecContext *ctx)
{
    MPP_RET ret = MPP_OK;
    MppTask task = NULL;
    size_t read_size;
    RK_U32 pkt_done = 0;
    RK_U32 frm_eos = 0;

    while (!frm_eos) {
        if (!pkt_done) {
            read_size = fread(ctx->buf, 1, READ_BUF_SIZE, ctx->fp_input);
            if (read_size != READ_BUF_SIZE) {
                mpp_log("File EOF, read size %d\n", read_size);
                pkt_done = 1;
            }

            // Create packet
            ret = mpp_packet_init(&ctx->packet, ctx->buf, read_size);
            if (ret) {
                mpp_err("mpp_packet_init failed\n");
                goto OUT;
            }

            if (pkt_done)
                mpp_packet_set_eos(ctx->packet);
        }

        // Get task
        ret = ctx->mpi->poll(ctx->ctx, MPP_PORT_INPUT, MPP_POLL_BLOCK);
        if (ret) {
            mpp_err("mpp input poll failed\n");
            goto OUT;
        }

        ret = ctx->mpi->dequeue(ctx->ctx, MPP_PORT_INPUT, &task);
        if (ret) {
            mpp_err("mpp task input dequeue failed\n");
            goto OUT;
        }

        mpp_task_meta_set_packet(task, KEY_INPUT_PACKET, ctx->packet);
        mpp_task_meta_set_frame (task, KEY_OUTPUT_FRAME, ctx->frame);

        ret = ctx->mpi->enqueue(ctx->ctx, MPP_PORT_INPUT, task);
        if (ret) {
            mpp_err("mpp task input enqueue failed\n");
            goto OUT;
        }

        // Get frame
        ret = ctx->mpi->poll(ctx->ctx, MPP_PORT_OUTPUT, MPP_POLL_BLOCK);
        if (ret) {
            mpp_err("mpp output poll failed\n");
            goto OUT;
        }

        ret = ctx->mpi->dequeue(ctx->ctx, MPP_PORT_OUTPUT, &task);
        if (ret) {
            mpp_err("mpp task output dequeue failed\n");
            goto OUT;
        }

        if (task) {
            MppFrame frame_out = NULL;
            mpp_task_meta_get_frame(task, KEY_OUTPUT_FRAME, &frame_out);

            if (frame_out) {
                // Write YUV data to file
                if (mpp_frame_get_info_change(frame_out)) {
                    RK_U32 width = mpp_frame_get_width(frame_out);
                    RK_U32 height = mpp_frame_get_height(frame_out);
                    RK_U32 hor_stride = mpp_frame_get_hor_stride(frame_out);
                    RK_U32 ver_stride = mpp_frame_get_ver_stride(frame_out);

                    ctx->width = width;
                    ctx->height = height;
                    ctx->frame_size = hor_stride * ver_stride * 3 / 2;

                    ret = ctx->mpi->control(ctx->ctx, MPP_DEC_SET_INFO_CHANGE_READY, NULL);
                    if (ret) {
                        mpp_err("info change ready failed\n");
                        goto OUT;
                    }
                } else {
                    void *ptr = mpp_buffer_get_ptr(mpp_frame_get_buffer(frame_out));
                    size_t len = fwrite(ptr, 1, ctx->frame_size, ctx->fp_output);
                    if (len != ctx->frame_size) {
                        mpp_err("Failed to write frame data\n");
                        goto OUT;
                    }
                    ctx->frame_count++;
                }

                frm_eos = mpp_frame_get_eos(frame_out);
                mpp_frame_deinit(&frame_out);
            }

            ret = ctx->mpi->enqueue(ctx->ctx, MPP_PORT_OUTPUT, task);
            if (ret) {
                mpp_err("mpp task output enqueue failed\n");
                goto OUT;
            }
        }
    }

OUT:
    return ret;
}

void deinit_vpu_decoder(VpuDecContext *ctx)
{
    if (ctx->packet) {
        mpp_packet_deinit(&ctx->packet);
        ctx->packet = NULL;
    }

    if (ctx->frame) {
        mpp_frame_deinit(&ctx->frame);
        ctx->frame = NULL;
    }

    if (ctx->ctx) {
        mpp_destroy(ctx->ctx);
        ctx->ctx = NULL;
    }

    if (ctx->frm_grp) {
        mpp_buffer_group_put(ctx->frm_grp);
        ctx->frm_grp = NULL;
    }

    if (ctx->fp_input) {
        fclose(ctx->fp_input);
        ctx->fp_input = NULL;
    }

    if (ctx->fp_output) {
        fclose(ctx->fp_output);
        ctx->fp_output = NULL;
    }
}

int main(int argc, char **argv)
{
    MPP_RET ret = MPP_OK;
    VpuDecContext ctx;

    if (argc != 3) {
        mpp_err("Usage: %s input_file output_file\n", argv[0]);
        return 1;
    }

    // Initialize decoder
    ret = init_vpu_decoder(&ctx, argv[1], argv[2]);
    if (ret) {
        mpp_err("Failed to initialize decoder\n");
        return 1;
    }

    // Start decoding
    ret = decode_frames(&ctx);
    if (ret) {
        mpp_err("Failed to decode frames\n");
    }

    // Print statistics
    mpp_log("Decoded %d frames\n", ctx.frame_count);

    // Cleanup
    deinit_vpu_decoder(&ctx);

    return ret;
}
