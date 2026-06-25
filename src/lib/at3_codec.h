#pragma once

#include "pmf_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *data;
    size_t size;
} At3Buffer;

typedef struct {
    int sample_rate;
    int channels;
    int bitrate;
    int block_align;
    int frame_count;
    int packet_size;
} At3Info;

typedef struct At3Encoder At3Encoder;
typedef struct At3Decoder At3Decoder;

At3Encoder *at3_encoder_create(int sample_rate, int channels, int bitrate);
int at3_encoder_feed_pcm(At3Encoder *enc, const float *pcm, int sample_count);
At3Buffer at3_encoder_finalize(At3Encoder *enc);
At3Info at3_encoder_get_info(At3Encoder *enc);
void at3_encoder_free(At3Encoder *enc);

At3Decoder *at3_decoder_create(const uint8_t *at3_data, size_t size);
int at3_decoder_get_sample_count(At3Decoder *dec);
int at3_decoder_decode(At3Decoder *dec, float *pcm_out, int max_samples);
At3Info at3_decoder_get_info(At3Decoder *dec);
void at3_decoder_free(At3Decoder *dec);

#ifdef __cplusplus
}
#endif
