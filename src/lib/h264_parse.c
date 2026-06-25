#include "h264_parse.h"
#include <stdlib.h>
#include <string.h>

H264ParseResult *h264_parse_annexb(const uint8_t *data, size_t size) {
    H264ParseResult *r = calloc(1, sizeof(H264ParseResult));
    if (!r) return NULL;
    r->data = (uint8_t *)data;
    r->data_size = size;

    size_t pos = 0;
    size_t frame_start = 0;
    int frame_idx = 0;

    while (pos + 3 < size) {
        int prefix = 0;
        if (data[pos] == 0 && data[pos + 1] == 0) {
            if (data[pos + 2] == 1) prefix = 3;
            else if (pos + 3 < size && data[pos + 2] == 0 && data[pos + 3] == 1) prefix = 4;
        }
        if (prefix == 0) { pos++; continue; }

        int nal_ref = data[pos + prefix] & 0x1F;
        pos += prefix;

        if (nal_ref == 1 || nal_ref == 5) {
            if (frame_start < pos && frame_idx > 0) {
                r->frames[frame_idx - 1].byte_length = pos - frame_start - r->frames[frame_idx - 1].byte_offset;
            }
            if (r->frame_count >= r->capacity) {
                r->capacity = r->capacity ? r->capacity * 2 : 256;
                r->frames = realloc(r->frames, r->capacity * sizeof(H264FrameInfo));
            }
            r->frames[r->frame_count].frame_index = frame_idx;
            r->frames[r->frame_count].byte_offset = frame_start;
            r->frames[r->frame_count].is_sync = (nal_ref == 5);
            r->frames[r->frame_count].nal_count = 0;
            r->frame_count++;
            frame_start = pos - prefix;
            frame_idx = r->frame_count - 1;
        }
        if (frame_idx < r->frame_count) r->frames[frame_idx].nal_count++;
    }

    if (r->frame_count > 0) {
        r->frames[r->frame_count - 1].byte_length = size - r->frames[r->frame_count - 1].byte_offset;
    }
    return r;
}

void h264_parse_free(H264ParseResult *r) {
    if (r) {
        free(r->frames);
        free(r);
    }
}

int h264_count_frames(const uint8_t *data, size_t size) {
    int count = 0;
    for (size_t i = 0; i + 3 < size; i++) {
        int prefix = 0;
        if (data[i] == 0 && data[i + 1] == 0) {
            if (data[i + 2] == 1) prefix = 3;
            else if (i + 3 < size && data[i + 2] == 0 && data[i + 3] == 1) prefix = 4;
        }
        if (!prefix) continue;
        int nal = data[i + prefix] & 0x1F;
        if (nal == 1 || nal == 5) count++;
        i += prefix;
    }
    return count;
}

int h264_count_sync_frames(const uint8_t *data, size_t size) {
    int count = 0;
    for (size_t i = 0; i + 3 < size; i++) {
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 && data[i + 3] == 1) {
            if ((data[i + 4] & 0x1F) == 5) count++;
            i += 3;
        } else if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
            if ((data[i + 3] & 0x1F) == 5) count++;
            i += 2;
        }
    }
    return count;
}

bool h264_is_sync_frame(const uint8_t *data, size_t size) {
    bool has_idr = false, has_sps = false, has_pps = false;
    for (size_t i = 0; i + 3 < size; i++) {
        int prefix = 0;
        if (data[i] == 0 && data[i + 1] == 0) {
            if (data[i + 2] == 1) prefix = 3;
            else if (i + 3 < size && data[i + 2] == 0 && data[i + 3] == 1) prefix = 4;
        }
        if (!prefix) continue;
        int nal = data[i + prefix] & 0x1F;
        if (nal == 5) has_idr = true;
        if (nal == 7) has_sps = true;
        if (nal == 8) has_pps = true;
        if (has_idr && has_sps && has_pps) return true;
        i += prefix;
    }
    return false;
}
