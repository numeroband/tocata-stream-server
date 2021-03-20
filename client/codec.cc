#include <iterator>
#include <memory>
#include <string>
#include <vector>
#include <iostream>

#include "codec.h"

namespace tocata {

Encoder::Encoder(uint32_t sample_rate, int num_channels)
    : num_channels_{num_channels} {
}

size_t Encoder::Encode(
      const float* pcm, size_t frame_size, void* encoded, size_t encoded_size) {
  uint16_t* pcm16 = static_cast<uint16_t*>(encoded);
  for (size_t sample = 0; sample < frame_size; ++sample) {
    for (uint8_t channel = 0; channel < num_channels_; ++channel) {
      size_t index = num_channels_ * sample + channel;
      pcm16[index] = fromFloat(pcm[index]);
    }
  }
  return frame_size * num_channels_ * sizeof(pcm16[0]);
}

Decoder::Decoder(uint32_t sample_rate, int num_channels)
    : num_channels_(num_channels) {
}

std::vector<float> Decoder::Decode(
    const void* packet, size_t packet_size, size_t frame_size) {
  const auto frame_length = (frame_size * num_channels_ * sizeof(float));
  std::vector<float> decoded(frame_size * num_channels_);
  const uint16_t* pcm16 = static_cast<const uint16_t*>(packet);
  for (size_t sample = 0; sample < frame_size; ++sample) {
    for (uint8_t channel = 0; channel < num_channels_; ++channel) {
      size_t index = num_channels_ * sample + channel;
      decoded[index] = toFloat(pcm16[index]);
    }
  }
  return decoded;
}

}