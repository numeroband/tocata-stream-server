#include "session.h"

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
static constexpr size_t kSampleRate = 48000;
static constexpr size_t kMaxSamples = 241920; // 5 seconds aligned to 256 and 480

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
  std::string const username = argc > 2 ? argv[2] : (sending ? "javier.madueno" : "lorenzo.soto");
  std::string const remote = argc > 3 ? argv[3] : (sending ? "lorenzo.soto" : "javier.madueno");
  std::string const filename = argc > 4 ? argv[4] : (sending ? "/tmp/recording.bin" : "/tmp/received.bin");

  FILE* rec;
  if (mode != kModeEcho) {
    rec = fopen(filename.c_str(), sending ? "rb" : "wb");
    std::cout << "Opened " << filename << std::endl;
  } 

  if (mode == kModeInit) {
    for (float i = 0; i < (2 * kMaxSamples); ++i) {
      size_t ret = fwrite(&i, sizeof(i), 1, rec);
      assert(ret == 1);
    }
    fclose(rec);
    return 0;
  }

  tocata::Session::AudioInfo info{
    0,
    0,
    kSampleRate,
    0,
    2,
  };

  tocata::Session session{};
  std::thread thread{[&]{
    session.connect(username, "password");
  }};

  // while(true) {
  //   play(filename, session);
  // }

  if (sending) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Assuming connected" << std::endl;
  }

  static float stereo_samples[2 * kBufferSize];
  float* samples[] = {stereo_samples, stereo_samples + kBufferSize};

  auto period = std::chrono::nanoseconds(kBufferSize * 1000 * 1000 * 1000 / kSampleRate);
  auto next = std::chrono::steady_clock::now();
  size_t file_samples = 0;
  while (mode == kModeEcho || file_samples < kMaxSamples) {
    next += period;
    std::this_thread::sleep_until(std::chrono::steady_clock::time_point(next));

    size_t received = kBufferSize;
    if (receiving) {
      session.receiveSamples(remote, info, samples, kBufferSize);
      if (mode != kModeEcho) {
        assert(received == kBufferSize);
      }
    }
    if (mode == kModeReceive) {
      for (size_t i = 0; i < received; ++i) {
        for (size_t channel = 0; channel < 2; ++channel) {
          size_t ret = fwrite(samples[channel] + i, sizeof(samples[channel][i]), 1, rec);
          assert(ret == 1);
        }
      }
    }
    if (mode == kModeSend) {
      for (size_t i = 0; i < received; ++i) {
        for (size_t channel = 0; channel < 2; ++channel) {
          size_t ret = fread(samples[channel] + i, sizeof(samples[channel][i]), 1, rec);
          assert(ret == 1);
        }
      }
    }
    if (sending) {
      session.sendSamples(info, samples, received);
    }
    file_samples += kBufferSize;
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
