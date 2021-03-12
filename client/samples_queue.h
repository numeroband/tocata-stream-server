#pragma once

#include <vector>
#include <cstdint>

namespace tocata {

class SamplesQueue {
public:
  SamplesQueue(uint32_t max_size, uint8_t channels, uint32_t sample_rate);
  size_t skipSamples(uint64_t timestamp);
  void addSamples(const float* interleaved, size_t num_samples, uint64_t timestamp);
  bool readSamples(float* samples[], size_t num_samples, uint8_t num_channels, uint64_t timestamp);

private:
  size_t tail() { return (_head + _size) % _max_size; }
  size_t beforeWrap() { return _max_size - tail(); };
  int64_t sampleIndex(uint64_t timestamp);
  void addNullSamples(size_t num_samples, uint64_t timestamp);
  void addSamplesWrapped(const float* interleaved, size_t num_samples, uint64_t timestamp);
  void setHeadAndSize(size_t num_samples, uint64_t timestamp);
  void readNullSamples(float* samples[], size_t num_samples, uint8_t num_channels, size_t dst_index);
  void readSamples(float* samples[], size_t num_samples, uint8_t num_channels, size_t src_index, size_t dst_index);
  std::vector<std::vector<float>> _samples;
  size_t _head = 0;
  size_t _size = 0;
  uint64_t _timestamp = 0;
  uint32_t _max_size;
  float _sample_period;
  uint8_t _channels;
};

}