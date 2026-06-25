#pragma once

#include "pmf_types.h"

typedef struct {
    uint8_t *data;
    size_t size;
} MpsMuxResult;

typedef struct {
    uint8_t *h264_data;
    size_t h264_size;
    uint8_t **audio_data;
    size_t *audio_sizes;
    int *audio_block_aligns;
    int *audio_packet_sizes;
    int audio_track_count;
    bool has_audio;
    long duration_ticks;
    bool video_only;
    bool make_icon;
    int mins;
    int secs;
} MpsMuxInput;

MpsMuxResult mps_mux_build(MpsMuxInput *input);
void mps_mux_result_free(MpsMuxResult *result);
