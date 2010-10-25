
#include "input.h"

#include "player.h"
#include "queue.h"

#include <pthread.h>
#include <libavcodec/avcodec.h>

#define MAX_PACKET_QUEUE_SIZE 32

static int stop = 0;

static pthread_t intid = 0;

static void* input_thread(void* para) {
    int err, aq, vq, sq;
    AVPacket* pkt;

    err = 0;
    for (;;) {
        if (stop) {
            pthread_exit(0);
        }
        if (err) {
            err = 0;
            usleep(25 * 1000);
            continue;
        }
        aq = gCtx->audio_enabled ? audio_packet_queue_size() : 0;
        vq = gCtx->video_enabled ? video_packet_queue_size() : 0;
        sq = gCtx->subtitle_enabled ? subtitle_packet_queue_size() : 0;
        // TODO: vq is growing too fast...
        if ((vq && ((aq + vq + sq) >= MAX_PACKET_QUEUE_SIZE) && (100 * vq / (aq + vq + sq) < 75)) ||
            (!vq && (aq + sq >= MAX_PACKET_QUEUE_SIZE)) ||
            (aq && aq >= MAX_PACKET_QUEUE_SIZE / 2 && vq && vq >= MAX_PACKET_QUEUE_SIZE)) {
            usleep(25 * 1000);
            continue;
        }
        pkt = (AVPacket*) av_mallocz(sizeof(AVPacket));
        if (!pkt) {
            err = -1;
            continue;
        }
        err = av_read_frame(gCtx->av_ctx, pkt);
        if (err < 0) {
            if (pkt)
                free_AVPacket(pkt);
            continue;
        }
        if (gCtx->audio_enabled && (pkt->stream_index == gCtx->audio_st_idx)) {
            audio_packet_queue_push_head(pkt);
        }
        else if (gCtx->video_enabled && (pkt->stream_index == gCtx->video_st_idx)) {
            video_packet_queue_push_head(pkt);
        }
        else if (gCtx->subtitle_enabled && (pkt->stream_index == gCtx->subtitle_st_idx)) {
            subtitle_packet_queue_push_head(pkt);
        }
        else {
            free_AVPacket(pkt);
        }
    }

    return 0;
}

int input_init() {
    if (!gCtx || intid)
        return -1;

    stop = 0;

    intid = 0;

    pthread_create(&intid, 0, input_thread, 0);

    return 0;
}

void input_free() {
    stop = -1;
    if (intid) {
        pthread_join(intid, 0);
        intid = 0;
    }
}

