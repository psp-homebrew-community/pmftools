#include "pmf_demux.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const uint8_t PACK_START[4] = {0x00, 0x00, 0x01, 0xBA};
static const uint8_t PACK_END[4]   = {0x00, 0x00, 0x01, 0xB9};
static const uint8_t AA3_MAGIC[4]  = {0x65, 0x61, 0x33, 0x03};
static const uint8_t EA3_MAGIC[4]  = {0x45, 0x41, 0x33, 0x01};
static const uint8_t AVC_PREFIX[4] = {0x00, 0x00, 0x00, 0x01};

static const uint8_t AA3_HEADER_CHUNK[] = {
    0x65,0x61,0x33,0x03,0x00,0x00,0x00,0x00,0x07,0x76,0x47,0x45,0x4F,0x42,0x00,0x00,
    0x01,0xC6,0x00,0x00,0x02,0x62,0x69,0x6E,0x61,0x72,0x79,0x00,0x00,0x00,0x00,0x4F,
    0x00,0x4D,0x00,0x47,0x00,0x5F,0x00,0x4C,0x00,0x53,0x00,0x49,0x00,0x00,0x00,0x01,
    0x00,0x40,0x00,0xDC,0x00,0x70,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x4B,0x45,
    0x59,0x52,0x49,0x4E,0x47
};
static const uint8_t EA3_HEADER_CHUNK_DATA[8] = {
    0x45, 0x41, 0x33, 0x01, 0x00, 0x60, 0xFF, 0xFF
};

static int find_bytes(const uint8_t *data, size_t size, const uint8_t *needle, int needle_len, size_t start) {
    if (start + needle_len > size) return -1;
    for (size_t i = start; i <= size - needle_len; i++) {
        if (memcmp(data + i, needle, needle_len) == 0) return (int)i;
    }
    return -1;
}

static int audio_pes_header_size(const uint8_t *data, size_t size, long offset) {
    if ((size_t)offset + 8 >= size) return 3;
    return (data[offset + 8] & 0xFF) + 7;
}

static int video_pes_header_size(const uint8_t *data, size_t size, long offset) {
    if ((size_t)offset + 8 >= size) return 3;
    return (data[offset + 8] & 0xFF) + 3;
}

static uint8_t get_audio_stream_id(const uint8_t *data, size_t size, long offset) {
    if ((size_t)offset + 8 >= size) return 0;
    int size_val = data[offset + 8];
    int check_offset = size_val + 6 + 7 - 4;
    if ((size_t)(offset + check_offset) < size) {
        return data[offset + check_offset];
    }
    return 0;
}

static bool is_mp2_stream(const uint8_t *data) {
    uint8_t high_nibble = (data[4] >> 4) & 0xF;
    return high_nibble == 4;
}

static PmfBlockEntry get_block_info(const uint8_t *block_id) {
    PmfBlockEntry e = {0};
    uint32_t code = read_u32be(block_id);

    if (code == MPEG_PACK_START) { e.size_type = PSIZE_STATIC; e.size = 0xE; }
    else if (code == MPEG_PACK_END) { e.size_type = PSIZE_EOF; e.size = -1; }
    else if (code == MPEG_SYSTEM_HDR || code == MPEG_PRIV1 || code == MPEG_PADDING || code == MPEG_PRIV2) {
        e.size_type = PSIZE_SIZE_BYTES; e.size = 2;
    }
    else if (code >= MPEG_AUDIO_FIRST && code <= MPEG_AUDIO_LAST) {
        e.size_type = PSIZE_SIZE_BYTES; e.size = 2;
    }
    else if (code >= MPEG_VIDEO_FIRST && code <= MPEG_VIDEO_LAST) {
        e.size_type = PSIZE_SIZE_BYTES; e.size = 2;
    }
    else if ((code & 0xFFFFFF00) == 0x00000100) {
        e.size_type = PSIZE_STATIC; e.size = 0xE;
    } else {
        e.size_type = PSIZE_SIZE_BYTES; e.size = 2;
    }
    return e;
}

static long find_data_start(const uint8_t *data, size_t size) {
    int start = find_bytes(data, size, PACK_START, 4, 0);
    if (start < 0) return 0;

    if (is_mp2_stream(data + start)) {
        return start;
    }

    long off = start + 4;
    if ((size_t)(off + 4) > size) return start;

    uint32_t seek_offsets = read_u32be(data + 0x86);
    uint32_t seek_count = read_u32be(data + 0x8A);

    if (seek_offsets > 0) {
        long data_start = seek_offsets + (seek_count * 0x0A);
        int next_start = find_bytes(data, size, PACK_START, 4, data_start);
        if (next_start >= 0) return next_start;
    }
    return start;
}

PmfContext *pmf_open(const uint8_t *data, size_t size) {
    PmfContext *ctx = calloc(1, sizeof(PmfContext));
    if (!ctx) return NULL;

    ctx->input_data = (uint8_t *)data;
    ctx->input_size = size;

    if (size >= PMF_HEADER_SIZE) {
        memcpy(ctx->header, data, PMF_HEADER_SIZE);
    }
    if (size > 0x8A) {
        ctx->seek_offsets = read_u32be(data + 0x86);
        ctx->seek_count = read_u32be(data + 0x8A);
    }

    ctx->data_start_offset = find_data_start(data, size);

    ctx->has_audio = false;
    ctx->has_video = false;
    ctx->video_only = true;

    for (int i = 0; i < PMF_MAX_AUDIO_TRACKS; i++) {
        ctx->audio_stream_types[i] = PMF_STREAM_NONE;
    }

    return ctx;
}

static void buffer_append(PmfOutputStream *os, const uint8_t *data, size_t len) {
    if (os->capacity < os->size + len) {
        size_t new_cap = os->capacity ? os->capacity * 2 : 65536;
        while (new_cap < os->size + len) new_cap *= 2;
        os->data = realloc(os->data, new_cap);
        os->capacity = new_cap;
    }
    memcpy(os->data + os->size, data, len);
    os->size += len;
}

int pmf_demux(PmfContext *ctx, DemuxOptions opts) {
    if (!ctx) return -1;

    const uint8_t *data = ctx->input_data;
    size_t size = ctx->input_size;
    long offset = ctx->data_start_offset;
    bool eof = false;

    ctx->audio_stream_count = 0;
    ctx->has_audio = false;
    ctx->has_video = false;

    while ((size_t)offset + 10 < size && !eof) {
        uint8_t block_id[4];
        if ((size_t)offset + 4 > size) break;
        memcpy(block_id, data + offset, 4);

        uint32_t code = read_u32be(block_id);
        PmfBlockEntry be = get_block_info(block_id);

        switch (be.size_type) {
            case PSIZE_STATIC:
                offset += be.size;
                break;

            case PSIZE_EOF:
                eof = true;
                break;

            case PSIZE_SIZE_BYTES: {
                uint32_t block_size = 0;
                if ((size_t)(offset + 6) > size) { offset += 6; break; }
                if (be.size == 2) block_size = read_u16be(data + offset + 4);
                else if (be.size == 4) block_size = read_u32be(data + offset + 4);

                long payload_offset = offset + 4 + be.size;
                bool is_audio = (code == MPEG_PRIV1);
                bool is_video = (code >= MPEG_VIDEO_FIRST && code <= MPEG_VIDEO_LAST);

                if (opts.extract_audio && is_audio) {
                    uint8_t sid = get_audio_stream_id(data, size, offset);
                    int header_size = audio_pes_header_size(data, size, offset);
                    long audio_data_offset = payload_offset + header_size;
                    long audio_data_size = (long)block_size - header_size;

                    int track_idx = -1;
                    for (int t = 0; t < ctx->audio_stream_count; t++) {
                        if (ctx->audio_stream_ids[t] == sid) { track_idx = t; break; }
                    }
                    if (track_idx < 0 && ctx->audio_stream_count < PMF_MAX_AUDIO_TRACKS) {
                        track_idx = ctx->audio_stream_count++;
                        ctx->audio_stream_ids[track_idx] = sid;
                        if (sid < 0x20) ctx->audio_stream_types[track_idx] = PMF_STREAM_ATRAC3;
                        else if (sid >= 0x40 && sid < 0x50) ctx->audio_stream_types[track_idx] = PMF_STREAM_LPCM;
                        else if (sid >= 0x80 && sid < 0x9F) ctx->audio_stream_types[track_idx] = PMF_STREAM_SUBTITLE;
                        else ctx->audio_stream_types[track_idx] = PMF_STREAM_NONE;
                    }

                    if (track_idx >= 0 && audio_data_size > 0 &&
                        (size_t)(audio_data_offset + audio_data_size) <= size) {
                        buffer_append(&ctx->audio_streams[track_idx],
                                      data + audio_data_offset, audio_data_size);
                    }
                    ctx->has_audio = true;
                }
                else if (opts.extract_video && is_video) {
                    int header_size = video_pes_header_size(data, size, offset);
                    long video_data_offset = payload_offset + header_size;
                    long video_data_size = (long)block_size - header_size;

                    if (video_data_size > 0 &&
                        (size_t)(video_data_offset + video_data_size) <= size) {
                        buffer_append(&ctx->video_stream,
                                      data + video_data_offset, video_data_size);
                    }
                    ctx->has_video = true;
                }

                offset += 4 + be.size + block_size;
                break;
            }
        }
    }
    return 0;
}

PmfDemuxBuffer pmf_get_video(PmfContext *ctx) {
    PmfDemuxBuffer buf = {NULL, 0};
    if (ctx && ctx->video_stream.size > 0) {
        buf.data = ctx->video_stream.data;
        buf.size = ctx->video_stream.size;
    }
    return buf;
}

PmfDemuxBuffer pmf_get_audio(PmfContext *ctx, int index) {
    PmfDemuxBuffer buf = {NULL, 0};
    if (ctx && index >= 0 && index < ctx->audio_stream_count) {
        buf.data = ctx->audio_streams[index].data;
        buf.size = ctx->audio_streams[index].size;
    }
    return buf;
}

int pmf_get_audio_count(PmfContext *ctx) {
    return ctx ? ctx->audio_stream_count : 0;
}

void pmf_close(PmfContext *ctx) {
    if (!ctx) return;
    free(ctx->video_stream.data);
    for (int i = 0; i < PMF_MAX_AUDIO_TRACKS; i++) {
        free(ctx->audio_streams[i].data);
    }
    free(ctx);
}

const char *pmf_version(void) {
    return "pmftools-plus 2.0.0 (C/C++)";
}
