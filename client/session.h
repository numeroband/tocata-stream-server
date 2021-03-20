#pragma once

#include <memory>
#include <vector>

namespace tocata {

class Session
{
public:
    static constexpr uint8_t kMaxStreams = 2;

    struct AudioInfo {
        int64_t sample_id;
        uint32_t sample_rate;
        uint8_t stream_id;
        uint8_t channels;
    };

    enum Status {
        kConnected,
        kInvalidUser,
        kInvalidPassword,
        kDisconnected,
    };

    using StatusCb = std::function<void(Status status, const std::string& name)>;
    using PeerCb = std::function<void(const std::string& peer_id, const std::string& name, bool connected)>;

    Session();
    ~Session();
    void connect(const std::string& username, const std::string& password, StatusCb status_cb, PeerCb peer_cb);
    void sendSamples(const AudioInfo& info, float* samples[], size_t num_samples);
    size_t receiveSamples(const std::string& peer_id, const AudioInfo& info, float* samples[], size_t num_samples);

private:
    struct Impl;
    std::unique_ptr<Impl> _pimpl;
};

}