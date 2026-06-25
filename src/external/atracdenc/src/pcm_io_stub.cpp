#include "wav.h"
#include <stdexcept>

class StubPcmIO : public IPCMProviderImpl {
    size_t channels_;
    size_t sampleRate_;
public:
    StubPcmIO(size_t ch, size_t sr) : channels_(ch), sampleRate_(sr) {}
    StubPcmIO() : channels_(2), sampleRate_(44100) {}
    size_t GetChannelsNum() const override { return channels_; }
    size_t GetSampleRate() const override { return sampleRate_; }
    size_t GetTotalSamples() const override { return 0; }
    size_t Read(TPCMBuffer&, size_t) override { return 0; }
    size_t Write(const TPCMBuffer&, size_t) override { return 0; }
};

IPCMProviderImpl* CreatePCMIOReadImpl(const std::string& path) {
    (void)path;
    return new StubPcmIO();
}

IPCMProviderImpl* CreatePCMIOWriteImpl(const std::string& path, int channels, int sampleRate) {
    (void)path;
    return new StubPcmIO(channels, sampleRate);
}
