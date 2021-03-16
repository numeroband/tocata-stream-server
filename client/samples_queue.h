#pragma once

#include <vector>
#include <cstdint>

namespace tocata {

class SamplesQueue {
public:
  static void readNullSamples(float* samples[], size_t num_samples, uint8_t num_channels, size_t dst_index = 0);
  
  SamplesQueue(uint32_t max_size, uint8_t channels);
  void reset();
  void addSamples(const float* interleaved, size_t num_samples, int64_t sample_id);
  size_t readSamples(float* samples[], size_t num_samples, uint8_t num_channels, int64_t sample_id);
  int64_t firstSampleId() { return _sample_id; }
  size_t size() { return _size; }

private:
  static constexpr uint64_t kInvalidSampleId = UINT64_MAX;

  size_t skipSamples(int64_t sample_id);
  size_t tail() { return (_head + _size) % _max_size; }
  size_t beforeWrap() { return _max_size - tail(); };
  void addNullSamples(size_t num_samples);
  void addSamplesWrapped(const float* interleaved, size_t num_samples);
  void setHeadAndSize(size_t num_samples);
  void readSamples(float* samples[], size_t num_samples, uint8_t num_channels, size_t src_index, size_t dst_index);

  std::vector<std::vector<float>> _samples;
  size_t _head = 0;
  size_t _size = 0;
  int64_t _sample_id = kInvalidSampleId;
  uint32_t _max_size;
  uint8_t _channels;
};

}