#pragma once

#include "pmf_types.h"

typedef struct {
    uint32_t start_code;
    PacketSizeType size_type;
    int size;
} PmfBlockEntry;

typedef struct {
    uint16_t stream_id;
    uint8_t *data;
    size_t size;
    size_t capacity;
} PmfOutputStream;

typedef struct {
    uint8_t header[PMF_HEADER_SIZE];
    uint32_t seek_offsets;
    uint32_t seek_count;
    uint32_t file_size_location;
    int has_audio;
    int has_video;
    bool video_only;
    long total_time_ticks;

    uint8_t *input_data;
    size_t input_size;
    long data_start_offset;

    PmfOutputStream video_stream;
    PmfOutputStream audio_streams[PMF_MAX_AUDIO_TRACKS];
    int audio_stream_count;
    uint8_t audio_stream_ids[PMF_MAX_AUDIO_TRACKS];
    PmfStreamType audio_stream_types[PMF_MAX_AUDIO_TRACKS];
} PmfContext;

typedef struct {
    uint8_t *data;
    size_t size;
} PmfDemuxBuffer;

PmfContext *pmf_open(const uint8_t *data, size_t size);
int pmf_demux(PmfContext *ctx, DemuxOptions opts);
PmfDemuxBuffer pmf_get_video(PmfContext *ctx);
PmfDemuxBuffer pmf_get_audio(PmfContext *ctx, int index);
int pmf_get_audio_count(PmfContext *ctx);
void pmf_close(PmfContext *ctx);

const char *pmf_version(void);
