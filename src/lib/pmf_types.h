#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PMF_HEADER_SIZE 2048
#define PMF_PACK_SIZE 2048
#define PMF_MAGIC "PSMF"
#define PMF_VERSION 0x30303132

#define PMF_VIDEO_START_TICKS 90000L
#define PMF_FRAME_DURATION_TICKS 3003L
#define PMF_SCR_INITIAL 3044236
#define PMF_SCR_INITIAL_STARTUP 15342067

#define MPEG_PACK_START 0x000001BAu
#define MPEG_PACK_END   0x000001B9u
#define MPEG_SYSTEM_HDR 0x000001BBu
#define MPEG_PRIV1      0x000001BDu
#define MPEG_PADDING    0x000001BEu
#define MPEG_PRIV2      0x000001BFu

#define MPEG_VIDEO_FIRST 0x000001E0u
#define MPEG_VIDEO_LAST  0x000001EFu
#define MPEG_AUDIO_FIRST 0x000001C0u
#define MPEG_AUDIO_LAST  0x000001DFu

#define PMF_STREAM_ATRAC3_MIN 0x00
#define PMF_STREAM_ATRAC3_MAX 0x1F
#define PMF_STREAM_LPCM_MIN   0x40
#define PMF_STREAM_LPCM_MAX   0x4F
#define PMF_STREAM_SUBTITLE_MIN 0x80
#define PMF_STREAM_SUBTITLE_MAX 0x9F

#define SID_VIDEO  0xE0
#define SID_AUDIO  0xBD
#define SID_SYSTEM 0xBB
#define SID_PRIV2  0xBF
#define SID_PADDING 0xBE

#define PMF_MUX_RATE 25000
#define PMF_MUX_RATE_DEN (PMF_MUX_RATE * 50ULL)
#define PMF_CLOCK_27M 27000000ULL
#define PMF_PTS_VIDEO_START 90000UL
#define PMF_PTS_AUDIO_START 85069UL

#define PMF_MAX_AUDIO_TRACKS 9

#define PMF_AUDIO_PAYLOAD_FIRST (PMF_PACK_SIZE - 14 - 6 - 3 - 8 - 4)
#define PMF_AUDIO_PAYLOAD_NORM  (PMF_PACK_SIZE - 14 - 6 - 3 - 5 - 4)
#define PMF_VIDEO_PAYLOAD       (PMF_PACK_SIZE - 14 - 9)

typedef enum {
    PSIZE_STATIC,
    PSIZE_SIZE_BYTES,
    PSIZE_EOF
} PacketSizeType;

typedef struct {
    PacketSizeType type;
    int size;
} BlockSizeDef;

typedef struct {
    bool extract_video;
    bool extract_audio;
    bool add_header;
    bool split_audio;
    bool add_playback_hacks;
    const char *video_path;
    const char *audio_path;
} DemuxOptions;

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} Buffer;

typedef enum {
    PMF_STREAM_NONE,
    PMF_STREAM_ATRAC3,
    PMF_STREAM_LPCM,
    PMF_STREAM_SUBTITLE
} PmfStreamType;

static inline uint32_t read_u32be(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static inline uint32_t read_u32le(const uint8_t *p) {
    return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0];
}

static inline uint16_t read_u16be(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

static inline uint16_t read_u16le(const uint8_t *p) {
    return ((uint16_t)p[1] << 8) | p[0];
}

static inline void write_u32be(uint8_t *p, uint32_t v) {
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8) & 0xFF;
    p[3] = v & 0xFF;
}

static inline void write_u32le(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

static inline void write_u16be(uint8_t *p, uint16_t v) {
    p[0] = (v >> 8) & 0xFF;
    p[1] = v & 0xFF;
}

static inline uint32_t mpeg_make_start_code(uint8_t b) {
    return 0x00000100u | b;
}

static inline bool is_video_stream_id(uint8_t id) {
    return id >= 0xE0 && id <= 0xEF;
}

static inline bool is_audio_stream_id(uint8_t id) {
    return id == 0xBD || (id >= 0xC0 && id <= 0xDF);
}

static inline bool is_mpeg_start_code(const uint8_t *p) {
    return p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01;
}
