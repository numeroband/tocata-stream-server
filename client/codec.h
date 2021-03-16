#pragma once

#include <memory>
#include <string>
#include <vector>

namespace tocata {

class Encoder {
public:
  Encoder(uint32_t sample_rate, int num_channels);

  size_t Encode(
      const float* pcm, size_t frame_size, void* encoded, size_t encoded_size);

 private:
  int num_channels_{};
};

class Decoder {
 public:
  Decoder(uint32_t sample_rate, int num_channels);

  std::vector<float> Decode(
      const void* packet, size_t packet_size, size_t frame_size);

 private:
  int num_channels_{};
};

}  // namespace opus
