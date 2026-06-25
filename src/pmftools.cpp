#include <emscripten.h>
#include <string>
#include <vector>
#include <cstring>

extern "C" {
#include "./lib/pmf_demux.h"
#include "./lib/pmf_mux.h"
#include "./lib/h264_parse.h"
#ifdef PMFTOOLS_HAS_ATRACDENC
#include "./lib/at3_codec.h"
#endif
}

static PmfContext *g_ctx = nullptr;
static std::vector<uint8_t> g_input;
static std::vector<uint8_t> g_output;
static std::vector<float> g_pcm_output;

#ifdef PMFTOOLS_HAS_ATRACDENC
static At3Encoder *g_at3_enc = nullptr;
static At3Decoder *g_at3_dec = nullptr;
static std::vector<uint8_t> g_at3_output;
#endif

extern "C" {

EMSCRIPTEN_KEEPALIVE
int wasm_pmf_open(const uint8_t *data, size_t size) {
    g_input.assign(data, data + size);
    if (g_ctx) pmf_close(g_ctx);
    g_ctx = pmf_open(g_input.data(), g_input.size());
    return g_ctx ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int wasm_pmf_demux(int extract_video, int extract_audio) {
    if (!g_ctx) return 0;
    DemuxOptions opts = {};
    opts.extract_video = extract_video != 0;
    opts.extract_audio = extract_audio != 0;
    opts.add_header = false;
    return pmf_demux(g_ctx, opts) == 0 ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
uint8_t *wasm_pmf_get_video_ptr(void) {
    if (!g_ctx) return nullptr;
    PmfDemuxBuffer buf = pmf_get_video(g_ctx);
    return buf.data;
}

EMSCRIPTEN_KEEPALIVE
size_t wasm_pmf_get_video_size(void) {
    if (!g_ctx) return 0;
    PmfDemuxBuffer buf = pmf_get_video(g_ctx);
    return buf.size;
}

EMSCRIPTEN_KEEPALIVE
uint8_t *wasm_pmf_get_audio_ptr(int index) {
    if (!g_ctx) return nullptr;
    PmfDemuxBuffer buf = pmf_get_audio(g_ctx, index);
    return buf.data;
}

EMSCRIPTEN_KEEPALIVE
size_t wasm_pmf_get_audio_size(int index) {
    if (!g_ctx) return 0;
    PmfDemuxBuffer buf = pmf_get_audio(g_ctx, index);
    return buf.size;
}

EMSCRIPTEN_KEEPALIVE
int wasm_pmf_get_audio_count(void) {
    return pmf_get_audio_count(g_ctx);
}

EMSCRIPTEN_KEEPALIVE
void wasm_pmf_close(void) {
    if (g_ctx) { pmf_close(g_ctx); g_ctx = nullptr; }
    g_input.clear();
}

EMSCRIPTEN_KEEPALIVE
int wasm_pmf_is_mps(const uint8_t *data, size_t size) {
    return pmf_mux_is_mps(data, size);
}

EMSCRIPTEN_KEEPALIVE
int wasm_pmf_has_audio(const uint8_t *data, size_t size) {
    return pmf_mux_has_audio(data, size);
}

EMSCRIPTEN_KEEPALIVE
uint8_t *wasm_pmf_mps_to_pmf(const uint8_t *mps_data, size_t mps_size,
                              int mins, int secs, int make_icon, int video_only,
                              size_t *out_size) {
    PmfMuxResult r = pmf_mux_convert(mps_data, mps_size, mins, secs,
                                      make_icon != 0, video_only != 0);
    if (!r.data) { *out_size = 0; return nullptr; }
    g_output.assign(r.data, r.data + r.size);
    pmf_mux_result_free(&r);
    *out_size = g_output.size();
    return g_output.data();
}

EMSCRIPTEN_KEEPALIVE
int wasm_pmf_count_h264_frames(const uint8_t *data, size_t size) {
    return h264_count_frames(data, size);
}

EMSCRIPTEN_KEEPALIVE
int wasm_pmf_count_sync_frames(const uint8_t *data, size_t size) {
    return h264_count_sync_frames(data, size);
}

EMSCRIPTEN_KEEPALIVE
const char *wasm_pmf_get_version(void) {
    return pmf_version();
}

#ifdef PMFTOOLS_HAS_ATRACDENC

EMSCRIPTEN_KEEPALIVE
int wasm_at3_encoder_create(int sample_rate, int channels, int bitrate) {
    if (g_at3_enc) at3_encoder_free(g_at3_enc);
    g_at3_enc = at3_encoder_create(sample_rate, channels, bitrate);
    return g_at3_enc ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int wasm_at3_encoder_feed(float *pcm, int sample_count) {
    if (!g_at3_enc) return -1;
    return at3_encoder_feed_pcm(g_at3_enc, pcm, sample_count);
}

EMSCRIPTEN_KEEPALIVE
int wasm_at3_encoder_finalize(void) {
    if (!g_at3_enc) return 0;
    At3Buffer buf = at3_encoder_finalize(g_at3_enc);
    g_at3_output.assign(buf.data, buf.data + buf.size);
    free(buf.data);
    at3_encoder_free(g_at3_enc);
    g_at3_enc = nullptr;
    return (int)g_at3_output.size();
}

EMSCRIPTEN_KEEPALIVE
uint8_t *wasm_at3_get_encoded_ptr(void) {
    return g_at3_output.data();
}

EMSCRIPTEN_KEEPALIVE
int wasm_at3_get_encoded_size(void) {
    return (int)g_at3_output.size();
}

EMSCRIPTEN_KEEPALIVE
int wasm_at3_decoder_create(const uint8_t *data, int size) {
    if (g_at3_dec) at3_decoder_free(g_at3_dec);
    g_at3_dec = at3_decoder_create(data, (size_t)size);
    return g_at3_dec ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int wasm_at3_decoder_get_samples(void) {
    if (!g_at3_dec) return 0;
    return at3_decoder_get_sample_count(g_at3_dec);
}

EMSCRIPTEN_KEEPALIVE
int wasm_at3_decoder_get_pcm(void) {
    if (!g_at3_dec) return 0;
    int samples = at3_decoder_get_sample_count(g_at3_dec);
    g_pcm_output.resize(samples * 2);
    int decoded = at3_decoder_decode(g_at3_dec, g_pcm_output.data(), samples);
    return decoded;
}

EMSCRIPTEN_KEEPALIVE
float *wasm_at3_get_decoded_ptr(void) {
    return g_pcm_output.data();
}

EMSCRIPTEN_KEEPALIVE
int wasm_at3_get_decoded_samples(void) {
    return (int)(g_pcm_output.size() / 2);
}

EMSCRIPTEN_KEEPALIVE
void wasm_at3_decoder_free(void) {
    if (g_at3_dec) { at3_decoder_free(g_at3_dec); g_at3_dec = nullptr; }
    g_pcm_output.clear();
}

EMSCRIPTEN_KEEPALIVE
void wasm_at3_encoder_free(void) {
    if (g_at3_enc) { at3_encoder_free(g_at3_enc); g_at3_enc = nullptr; }
    g_at3_output.clear();
}

EMSCRIPTEN_KEEPALIVE
int wasm_at3_encode_pcm(const float *pcm, int sample_count, int sample_rate, int channels, int bitrate) {
    At3Encoder *enc = at3_encoder_create(sample_rate, channels, bitrate);
    if (!enc) return -1;
    at3_encoder_feed_pcm(enc, pcm, sample_count);
    At3Buffer buf = at3_encoder_finalize(enc);
    g_at3_output.assign(buf.data, buf.data + buf.size);
    free(buf.data);
    at3_encoder_free(enc);
    return (int)g_at3_output.size();
}

EMSCRIPTEN_KEEPALIVE
int wasm_at3_decode(const uint8_t *data, int size) {
    At3Decoder *dec = at3_decoder_create(data, (size_t)size);
    if (!dec) return -1;
    int samples = at3_decoder_get_sample_count(dec);
    g_pcm_output.resize(samples * 2);
    int decoded = at3_decoder_decode(dec, g_pcm_output.data(), samples);
    at3_decoder_free(dec);
    return decoded;
}

#endif

}
