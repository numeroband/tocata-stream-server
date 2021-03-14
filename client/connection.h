#pragma once

#include "samples_queue.h"

#include "juice/juice.h"
#include "opus_wrapper.h"

#include <vector>
#include <deque>
#include <mutex>

namespace tocata {

class Connection {
public:
  static const uint32_t kSampleRate = 48000;
  static const uint32_t kFrameSize = 480;
  static const uint32_t kNumChannels = 2;

  struct AudioInfo {
      uint32_t seq;
      int64_t sample_id;
      uint8_t stream_id;
      uint8_t channels;
  };
  
  using Candidates = std::vector<std::string>;
  using SendCandidatesCb = std::function<void(Candidates)>;
  using ConnectedCb = std::function<void()>;

  static std::vector<uint8_t> BuildAudioMessage(const AudioInfo& info, const float* samples, opus::Encoder& encoder);

  Connection();
  void connect(const std::string& remote);
  void setRemoteCandidates(Candidates candidates);
  void close();
  std::string description();
  bool connected() { return _connected; };
  void setSendCandidatesCb(SendCandidatesCb cb) { _sendCandidatesCb = cb; }
  void setConnectedCb(ConnectedCb cb) { _connectedCb = cb; }
  void send(const void* data, size_t length);
  size_t receive(const AudioInfo& info, float* samples[], size_t num_samples);

private:
  enum MessageType : uint32_t {
    kInvalid,
    kPingRequest,
    kPingResponse,
    kAudio,
  };

  struct MessageHeader {
    MessageType type;
    uint32_t seq;
  };

  struct PingRequest {
    MessageHeader header;
  };

  struct PingResponse {
    MessageHeader header;
    uint64_t host_timestamp;
  };

  struct AudioMessage {
    MessageHeader header;
    int64_t sample_id;
    uint64_t host_timestamp;
    uint8_t stream_id;
    uint8_t channels;
    uint16_t size;
    uint8_t bytes[];
  };

  struct SampleTimestamp {
    int64_t sample_id;
    uint64_t timestamp;
    operator bool() { return timestamp != 0; }
  };

  static constexpr size_t kMaxQueueSize = 4 * kFrameSize;
  static constexpr size_t kMaxEncodedFrame = 512 * kNumChannels * sizeof(float); // 512;
  static constexpr float kSamplePeriod = 1e9 / kSampleRate;
  static constexpr int64_t kInvalidOffset = INT64_MAX;

  void onStateChanged(juice_state_t state);
  void onCandidate(const char *sdp);
  void onGatheringDone();
  void onRecv(const void *data, size_t size);
  void onAudio(const void *data, size_t size);
  void onPingRequest(const void *data, size_t size);
  void onPingResponse(const void *data, size_t size);
  void calculateSampleOffset();

  std::atomic<bool> _connected = false;
  std::unique_ptr<juice_agent_t, decltype(&juice_destroy)> _agent{nullptr, &juice_destroy};
  Candidates _candidates;
  SendCandidatesCb _sendCandidatesCb;
  ConnectedCb _connectedCb;
  uint32_t _lastSeq = 0;
  opus::Decoder _decoder{kSampleRate, kNumChannels};
  std::mutex _mutex{};
  bool _received = false;
  std::chrono::system_clock::time_point _pingSent;
  SamplesQueue _samples{kMaxQueueSize, kNumChannels};
  int64_t _sample_offset = kInvalidOffset;
  SampleTimestamp _local_sample_timestamp{};
  SampleTimestamp _remote_sample_timestamp{};
};

}