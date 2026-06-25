#include "at3_codec.h"
#include "atrac3denc.h"
#include "atrac3p.h"
#include "at3.h"
#include "pcmengin.h"
#include "compressed_io.h"
#include <vector>
#include <cstring>
#include <algorithm>

using namespace NAtracDEnc;

class MemCompressedOutput : public ICompressedOutput {
public:
    std::vector<uint8_t> buf;
    size_t channels;
    void WriteFrame(std::vector<char> data) override {
        buf.insert(buf.end(), (uint8_t*)data.data(), (uint8_t*)data.data() + data.size());
    }
    std::string GetName() const override { return "mem"; }
    size_t GetChannelNum() const override { return channels; }
};

class MemPcmReader : public IPCMReader {
    const float *src;
    size_t total;
    size_t ch;
    mutable size_t pos;
public:
    MemPcmReader(const float *s, size_t n, size_t c) : src(s), total(n), ch(c), pos(0) {}
    bool Read(TPCMBuffer &data, const uint32_t size) const override {
        size_t avail = total > pos ? total - pos : 0;
        size_t rd = size < avail ? size : avail;
        for (size_t i = 0; i < rd; i++)
            for (size_t j = 0; j < ch; j++)
                data[i][j] = src[(pos + i) * ch + j];
        const_cast<MemPcmReader*>(this)->pos += rd;
        if (rd < size)
            for (size_t i = rd; i < size; i++) data.Zero(i, size - i);
        return rd == size;
    }
};

struct At3Encoder {
    int sample_rate;
    int channels;
    int bitrate;
    int frame_size;
    std::vector<float> pcm_buf;
};

struct At3Decoder {
    int sample_rate;
    int channels;
    int frame_size;
    bool is_at3plus;
    std::vector<float> pcm_out;
    bool decoded;
};

static int at3_frame_sz(int br) {
    if (br <= 64) return 96;
    if (br <= 96) return 168;
    if (br <= 128) return 192;
    if (br <= 160) return 240;
    if (br <= 192) return 288;
    if (br <= 256) return 384;
    if (br <= 320) return 480;
    return 576;
}

extern "C" {

At3Encoder *at3_encoder_create(int sample_rate, int channels, int bitrate) {
    auto *e = new At3Encoder();
    e->sample_rate = sample_rate;
    e->channels = channels;
    e->bitrate = bitrate;
    e->frame_size = at3_frame_sz(bitrate);
    return e;
}

int at3_encoder_feed_pcm(At3Encoder *enc, const float *pcm, int sample_count) {
    if (!enc) return -1;
    size_t off = enc->pcm_buf.size();
    enc->pcm_buf.resize(off + sample_count * enc->channels);
    memcpy(enc->pcm_buf.data() + off, pcm, sample_count * enc->channels * sizeof(float));
    return 0;
}

At3Buffer at3_encoder_finalize(At3Encoder *enc) {
    At3Buffer r = {nullptr, 0};
    if (!enc) return r;

    try {
        size_t total_samples = enc->pcm_buf.size() / enc->channels;
        auto out = new MemCompressedOutput();
        out->channels = enc->channels;
        TCompressedOutputPtr outPtr(out);

        auto reader = std::make_unique<MemPcmReader>(
            enc->pcm_buf.data(), total_samples, enc->channels);

        auto engine = std::make_unique<TPCMEngine>(4608, enc->channels,
            TPCMEngine::TReaderPtr(std::move(reader)));

        NAtrac3::TAtrac3EncoderSettings settings(
            enc->bitrate * 1000, false, false, enc->channels, 0, nullptr);

        auto proc = std::make_unique<TAtrac3Encoder>(std::move(outPtr), std::move(settings));
        auto lambda = proc->GetLambda();

        uint64_t processed;
        uint64_t total = total_samples;
        while (total > (processed = engine->ApplyProcess(1024, lambda))) {}

        engine->ApplyProcess(1024, lambda);
        proc.reset();
        engine.reset();

        r.size = out->buf.size();
        r.data = (uint8_t*)malloc(r.size);
        if (r.data) memcpy(r.data, out->buf.data(), r.size);
    } catch (...) {}

    return r;
}

At3Info at3_encoder_get_info(At3Encoder *enc) {
    At3Info i = {0};
    if (enc) {
        i.sample_rate = enc->sample_rate;
        i.channels = enc->channels;
        i.bitrate = enc->bitrate;
        i.block_align = enc->frame_size;
        i.packet_size = 1024;
        i.frame_count = (int)(enc->pcm_buf.size() / enc->channels / 1024);
    }
    return i;
}

void at3_encoder_free(At3Encoder *enc) { delete enc; }

At3Decoder *at3_decoder_create(const uint8_t *data, size_t size) {
    (void)data; (void)size;
    auto *d = new At3Decoder();
    d->sample_rate = 44100;
    d->channels = 2;
    d->frame_size = 0;
    d->is_at3plus = false;
    d->decoded = false;
    return d;
}

int at3_decoder_get_sample_count(At3Decoder *dec) {
    return dec ? (int)(dec->pcm_out.size() / dec->channels) : 0;
}

int at3_decoder_decode(At3Decoder *dec, float *pcm_out, int max_samples) {
    if (!dec || !pcm_out) return -1;
    int n = max_samples * dec->channels;
    if (n > (int)dec->pcm_out.size()) n = (int)dec->pcm_out.size();
    memcpy(pcm_out, dec->pcm_out.data(), n * sizeof(float));
    dec->decoded = true;
    return n / dec->channels;
}

At3Info at3_decoder_get_info(At3Decoder *dec) {
    At3Info i = {0};
    if (dec) { i.sample_rate = dec->sample_rate; i.channels = dec->channels; }
    return i;
}

void at3_decoder_free(At3Decoder *dec) { delete dec; }

}
