#pragma once

#include <memory>
#include <string>
#include <vector>

namespace tocata {

class Encoder {
public:
  Encoder(uint32_t sample_rate, int num_channels);

  static inline uint16_t fromFloat(float sample) {
      const float sample_float = ((sample + 1) / 2) * 65535 + 0.5;
      return static_cast<uint16_t>(sample_float);
  }

  size_t Encode(
      const float* pcm, size_t frame_size, void* encoded, size_t encoded_size);

 private:
  int num_channels_{};
};

class Decoder {
 public:
  Decoder(uint32_t sample_rate, int num_channels);

  static inline float toFloat(uint16_t sample) {
    return (2 * (static_cast<float>(sample) / 65535)) - 1;
  }

  std::vector<float> Decode(
      const void* packet, size_t packet_size, size_t frame_size);

 private:
  int num_channels_{};
};

}  // namespace opus
