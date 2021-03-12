#pragma once

#include <memory>
#include <vector>

namespace tocata {

class Session
{
public:
    static constexpr uint8_t kMaxStreams = 2;

    struct AudioInfo {
        int64_t sample_timestamp;
        uint64_t host_timestamp;
        uint32_t sample_rate;
        uint8_t stream_id;
        uint8_t channels;
    };

    Session();
    ~Session();
    void connect(const std::string& username, const std::string& password);
    void sendSamples(const AudioInfo& info, float* samples[], size_t num_samples);
    void receiveSamples(const std::string& peer, const AudioInfo& info, float* samples[], size_t num_samples);

private:
    struct Impl;
    std::unique_ptr<Impl> _pimpl;
};

}