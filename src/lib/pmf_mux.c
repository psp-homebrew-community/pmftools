#include "pmf_mux.h"
#include "pmf_header.h"
#include "h264_parse.h"
#include <stdlib.h>
#include <string.h>

int pmf_mux_is_mps(const uint8_t *data, size_t size) {
    if (size < 5) return 0;
    return (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01 && data[3] == 0xBA && data[4] == 0x44) ? 1 : 0;
}

int pmf_mux_has_audio(const uint8_t *data, size_t size) {
    if (size < 4) return 0;
    for (size_t i = 0; i + 3 < size; i++) {
        if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x01 && data[i + 3] == 0xBD)
            return 1;
    }
    return 0;
}

static long get_file_size_from_data(const uint8_t *data, size_t size) {
    (void)data;
    return (long)size;
}

static void set_duration_in_header(uint8_t *header, long duration_ticks) {
    header[92] = (duration_ticks >> 24) & 0xFF;
    header[118] = (duration_ticks >> 24) & 0xFF;
    header[93] = (duration_ticks >> 16) & 0xFF;
    header[119] = (duration_ticks >> 16) & 0xFF;
    header[94] = (duration_ticks >> 8) & 0xFF;
    header[120] = (duration_ticks >> 8) & 0xFF;
    header[95] = duration_ticks & 0xFF;
    header[121] = duration_ticks & 0xFF;
}

static void set_file_size_in_header(uint8_t *header, long file_size) {
    header[12] = (file_size >> 24) & 0xFF;
    header[13] = (file_size >> 16) & 0xFF;
    header[14] = (file_size >> 8) & 0xFF;
    header[15] = file_size & 0xFF;
}

static void configure_video_only_header(uint8_t *header) {
    header[83] = 0x3E;
    header[104] = 0x01;
    header[109] = 0x24;
    header[127] = 0x12;
    header[129] = 0x01;
    for (int i = 146; i <= 149; i++) header[i] = 0x00;
    header[160] = 0x00;
    header[161] = 0x00;
}

static void configure_icon_header(uint8_t *header) {
    header[7] = 0x34;
    header[83] = 0x3E;
    header[104] = 0x01;
    header[109] = 0x24;
    header[127] = 0x12;
    header[129] = 0x01;
    header[132] = 0x20;
    header[133] = 0x14;
    header[142] = 0x09;
    header[143] = 0x05;
    header[146] = 0x00;
    header[148] = 0x00;
    header[149] = 0x00;
    header[150] = 0x00;
    header[151] = 0x00;
    header[160] = 0x00;
    header[161] = 0x00;
}

static long infer_video_duration_ticks(const uint8_t *data, size_t size) {
    int frames = 0;
    for (size_t i = 0; i + 9 < size;) {
        if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x01 && data[i + 3] == 0xE0) {
            int packet_len = (data[i + 4] << 8) | data[i + 5];
            size_t packet_end = (size_t)(i + 6 + packet_len) < size ? (size_t)(i + 6 + packet_len) : size;
            size_t payload_start = (size_t)(i + 9 + data[i + 8]) < packet_end ? (size_t)(i + 9 + data[i + 8]) : packet_end;

            for (size_t j = payload_start; j + 3 < packet_end; j++) {
                int prefix = 0;
                if (data[j] == 0 && data[j + 1] == 0) {
                    if (j + 2 < packet_end && data[j + 2] == 1) prefix = 3;
                    else if (j + 3 < packet_end && data[j + 2] == 0 && data[j + 3] == 1) prefix = 4;
                }
                if (!prefix) continue;
                int nal = (j + (size_t)prefix < packet_end) ? (data[j + prefix] & 0x1F) : 0;
                if (nal == 1 || nal == 5) frames++;
                j += prefix;
            }
            i = packet_end;
        } else {
            i++;
        }
    }
    if (frames <= 0) return -1;
    return PMF_VIDEO_START_TICKS + frames * PMF_FRAME_DURATION_TICKS;
}

PmfMuxResult pmf_mux_convert(const uint8_t *mps_data, size_t mps_size,
                               int mins, int secs, bool make_icon, bool video_only) {
    PmfMuxResult result = {NULL, 0};
    if (!mps_data || mps_size == 0) return result;

    uint8_t header[PMF_HEADER_SIZE];
    memcpy(header, PMF_DEFAULT_HEADER, PMF_HEADER_SIZE);

    if (make_icon) {
        configure_icon_header(header);
    }

    long total_time = ((long)mins * 60L + secs) * 90000L;
    int has_audio = video_only ? 0 : pmf_mux_has_audio(mps_data, mps_size);

    if (has_audio == 0 || video_only) {
        configure_video_only_header(header);
        long video_dur = infer_video_duration_ticks(mps_data, mps_size);
        if (video_dur > 0) total_time = video_dur;
    }

    set_duration_in_header(header, total_time);
    set_file_size_in_header(header, get_file_size_from_data(mps_data, mps_size));

    result.size = PMF_HEADER_SIZE + mps_size;
    result.data = (uint8_t*)malloc(result.size);
    if (!result.data) { result.size = 0; return result; }

    memcpy(result.data, header, PMF_HEADER_SIZE);
    memcpy(result.data + PMF_HEADER_SIZE, mps_data, mps_size);

    return result;
}

void pmf_mux_result_free(PmfMuxResult *result) {
    if (result && result->data) {
        free(result->data);
        result->data = NULL;
        result->size = 0;
    }
}
