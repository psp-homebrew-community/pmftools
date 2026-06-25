#pragma once

#include "pmf_types.h"

typedef struct {
    int frame_index;
    size_t byte_offset;
    size_t byte_length;
    bool is_sync;
    int nal_count;
} H264FrameInfo;

typedef struct {
    H264FrameInfo *frames;
    int frame_count;
    int capacity;
    uint8_t *data;
    size_t data_size;
} H264ParseResult;

H264ParseResult *h264_parse_annexb(const uint8_t *data, size_t size);
void h264_parse_free(H264ParseResult *r);
int h264_count_frames(const uint8_t *data, size_t size);
int h264_count_sync_frames(const uint8_t *data, size_t size);
bool h264_is_sync_frame(const uint8_t *data, size_t size);
