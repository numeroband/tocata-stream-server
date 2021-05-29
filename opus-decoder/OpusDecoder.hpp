#pragma once

#include <emscripten/html5.h>
#include <emscripten/bind.h>

#include <opus.h>

namespace e = emscripten;

class OpusDecoder {
public:
  OpusDecoder();
  ~OpusDecoder();
  int decode(uint32_t buffer_size);
  uintptr_t buffer() { return reinterpret_cast<uintptr_t>(_buffer); }
  uintptr_t leftSamples() { return reinterpret_cast<uintptr_t>(_left_samples); }
  uintptr_t rightSamples() { return reinterpret_cast<uintptr_t>(_right_samples); }
  
private:
  static constexpr uint32_t kMaxInput = 16 * 1024;
  static constexpr uint32_t kMaxSamples = 64 * 1024;

  OpusDecoder* _decoder = nullptr;
  uint8_t _buffer[kMaxInput];
  float _left_samples[kMaxSamples];
  float _right_samples[kMaxSamples];
};

EMSCRIPTEN_BINDINGS(EMTest) {
  e::class_<OpusDecoder>("OpusDecoderPriv")
    .constructor()
      .function("decode", &OpusDecoder::decode)
      .function("buffer", &OpusDecoder::buffer)
      .function("leftSamples", &OpusDecoder::leftSamples)
      .function("rightSamples", &OpusDecoder::rightSamples);
}