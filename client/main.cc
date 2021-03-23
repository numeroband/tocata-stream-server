#include "session.h"
#include "codec.h"

#include <iostream>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <chrono>

static constexpr const char* kInitStr = "init";
static constexpr const char* kSendStr = "send";
static constexpr const char* kRecvStr = "recv";
static constexpr const char* kEchoStr = "echo";

static constexpr size_t kBufferSize = 256;
static constexpr size_t kSampleRate = 44100;
static constexpr float kSamplePeriod = 1e9 / kSampleRate;
static constexpr size_t kMaxSamples = 238080; // ~5 seconds aligned to 512 and 480

enum Mode {
  kModeInit,
  kModeSend,
  kModeReceive,
  kModeEcho,
};

int main(int argc, char* argv[]) try {
  std::string const mode_str = argc > 1 ? argv[1] : "";
  Mode mode;
  if (mode_str == kInitStr) {
    mode = kModeInit;
  } else if (mode_str == kSendStr) {
    mode = kModeSend;
  } else if (mode_str == kRecvStr) {
    mode = kModeReceive;
  } else if (mode_str == kEchoStr) {
    mode = kModeEcho;
  } else {
    mode = kModeReceive;
  }
  bool sending = (mode == kModeSend || mode == kModeEcho);
  bool receiving = (mode == kModeReceive || mode == kModeEcho);
  std::string const username = argc > 2 ? argv[2] : (sending ? "javimadu@gmail.com" : "lorenzo.soto@gmail.com");
  std::string const password = argc > 3 ? argv[3] : "password";
  std::string const filename = argc > 4 ? argv[4] : (mode == kModeReceive ? "/tmp/received.bin" : "/tmp/recording.bin");

  FILE* rec;
  if (mode != kModeEcho) {
    rec = fopen(filename.c_str(), sending ? "rb" : "wb");
    std::cout << "Opened " << filename << std::endl;
  } 

  if (mode == kModeInit) {
    for (uint32_t i = 1; i <= (2 * kMaxSamples); ++i) {
      float sample = tocata::Decoder::toFloat(static_cast<uint16_t>(i));
      size_t ret = fwrite(&sample, sizeof(sample), 1, rec);
      assert(ret == 1);
    }
    fclose(rec);
    return 0;
  }

  uint64_t peer_connected = 0;
  tocata::Session session{};
  std::thread thread{[&]{
    session.connect(username, password,
      [&](auto status, auto peer_id, auto name, auto gain) {
        if (mode == kModeEcho) {
          *gain = 0.0;
        }
      },
      [&](auto peer_id, auto name, auto connected, auto gain) {
        peer_connected = peer_id;
      });
  }};

  if (sending) {
    std::cout << "Waiting for connection" << std::endl;
    while (!peer_connected) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  static float stereo_samples[2 * kBufferSize] = {};
  float* samples[] = {stereo_samples, stereo_samples + kBufferSize};

  auto period = std::chrono::nanoseconds(uint64_t(kBufferSize * kSamplePeriod));
  auto next = std::chrono::steady_clock::now();
  size_t total_samples = 0;
  int64_t sample_id = 0;
  while (mode == kModeEcho || total_samples < kMaxSamples) {
    tocata::Session::AudioInfo info{
      sample_id,
      kSampleRate,
      0,
      2,
    };
    sample_id += kBufferSize;    
    next += period;
    // Simulate audio buffering
    std::this_thread::sleep_until(std::chrono::steady_clock::time_point(next));

    size_t received = kBufferSize;

    for (size_t i = 0; i < received; ++i) {
      for (size_t channel = 0; channel < 2; ++channel) {
        if (mode == kModeSend) {
          size_t ret = fread(samples[channel] + i, sizeof(samples[channel][i]), 1, rec);
          assert(ret == 1);
        } else if (mode == kModeReceive) {
          memset(samples[channel] + i, 0, sizeof(samples[channel][i]));
        }
      }
    }

    float pre_sample = samples[0][0];
    session.processSamples(info, samples, kBufferSize);
    float post_sample = samples[0][0];

    // std::cout << "pre sample " << pre_sample << " post sample " << post_sample << std::endl;

    if (mode == kModeReceive && total_samples == 0 && post_sample == 0) {
      continue;
    }

    total_samples += received;

    if (mode != kModeReceive) {
      continue;
    }

    for (size_t i = 0; i < received; ++i) {
      for (size_t channel = 0; channel < 2; ++channel) {
        size_t ret = fwrite(samples[channel] + i, sizeof(samples[channel][i]), 1, rec);
        assert(ret == 1);
      }
    }
  };

  std::cout << "All sent" << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  fclose(rec);

  std::cout << "Connection closed" << std::endl;

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << std::endl;
  return 1;
}
