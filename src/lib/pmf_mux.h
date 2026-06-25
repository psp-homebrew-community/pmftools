#pragma once

#include "pmf_types.h"

typedef struct {
    uint8_t *data;
    size_t size;
} PmfMuxResult;

PmfMuxResult pmf_mux_convert(const uint8_t *mps_data, size_t mps_size,
                               int mins, int secs, bool make_icon, bool video_only);
void pmf_mux_result_free(PmfMuxResult *result);

int pmf_mux_is_mps(const uint8_t *data, size_t size);
int pmf_mux_has_audio(const uint8_t *data, size_t size);
