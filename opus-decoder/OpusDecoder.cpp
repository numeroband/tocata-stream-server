#include "OpusDecoder.hpp"

#include <iostream>

OpusDecoder::OpusDecoder() {
  int error{};
  _decoder = opus_decoder_create(48000, 2, &error);
  if (error != OPUS_OK) { throw std::runtime_error("Error creating opus decoder"); }
}

OpusDecoder::~OpusDecoder() {
  if (!_decoder) {
    opus_decoder_destroy(_decoder);
  }
}

int OpusDecoder::decode(uint32_t buffer_size)
{
  struct EncodedFrame {
    uint32_t magic;
    uint32_t size;
    uint8_t buffer[];
  };

  const EncodedFrame& frame = *reinterpret_cast<const EncodedFrame*>(_buffer);
  if (buffer_size < sizeof(frame)) {
    std::cerr << "Frame too small: " << buffer_size << std::endl;
    return 0;
  }
  if (frame.magic != 'TcOp') {
    std::cerr << "Invalid frame magic: " << frame.magic << std::endl;
    return 0;
  }
  if (buffer_size < sizeof(frame) + frame.size) {
    std::cerr << "Frame too small: " << buffer_size << " in frame: " << frame.size << std::endl;
    return 0;
  }

  uint32_t decoded_samples = 0;
  float output[4096];
  uint32_t offset = 0;
  while (offset < frame.size) {
    int16_t packet_size;
    memcpy(&packet_size, frame.buffer + offset, sizeof(packet_size));
    offset += sizeof(packet_size);
    if (offset + packet_size > frame.size) {
      std::cerr << "Wrong packet size: " << packet_size << " offset: " << offset << " in frame: " << frame.size << std::endl;
      return -1;
    }
    int samples = opus_decode_float(_decoder, frame.buffer + offset, packet_size, output, 2048, false);
    offset += packet_size;
    if (decoded_samples + samples > kMaxSamples) {
      std::cerr << "Too many samples in frame: " << (decoded_samples + samples) << std::endl;
      return -1;
    }
    for (uint32_t i = 0; i < samples; ++i) {
      _left_samples[decoded_samples + i] = output[i * 2];
      _right_samples[decoded_samples + i] = output[i * 2 + 1];
    }
    decoded_samples += samples;
  }

  return decoded_samples;
}
