#include "samples_queue.h"

#include <cstring>
#include <algorithm>
#include <iostream>

// #define SQ_LOG

namespace tocata {

SamplesQueue::SamplesQueue(uint32_t max_size, uint8_t channels) 
    : _samples(channels), _max_size(max_size), _channels(channels) {
  for (uint8_t channel = 0; channel < channels; ++channel) {
    _samples[channel].resize(max_size);
    memset(_samples[channel].data(), 0xA5, max_size * sizeof(_samples[channel][0]));
  }
}

void SamplesQueue::reset() {
  _head = 0;
  _size = 0;
  _sample_id = kInvalidSampleId;
}

void SamplesQueue::setHeadAndSize(size_t num_samples) {
  _size += num_samples;
  if (_size > _max_size) {
    size_t extra = _size - _max_size;
    _head = (_head + extra) % _max_size;
    _size = _max_size;
    _sample_id += extra;
  }  
}

void SamplesQueue::addSamplesWrapped(const float* interleaved, size_t num_samples) {
  for (uint8_t channel = 0; channel < _channels; ++channel) {
    for (size_t sample = 0; sample < num_samples; ++sample) {
      _samples[channel][tail() + sample] = interleaved[sample * _channels + channel];
    }
  }
  setHeadAndSize(num_samples);
}

void SamplesQueue::addNullSamples(size_t num_samples) {
  for (uint8_t channel = 0; channel < _channels; ++channel) {
    memset(_samples[channel].data() + tail(), 0, num_samples);
  }
  setHeadAndSize(num_samples);
}

size_t SamplesQueue::skipSamples(int64_t sample_id) {
  int64_t start = sample_id - _sample_id;
  if (_sample_id == kInvalidSampleId || (start - _size) >= _max_size) {
    _head = 0;
    _size = 0;
    _sample_id = sample_id;
    return 0;
  }

  if (start < _size) {
    return _size - start;
  }

  if (start > _size) {
    size_t null_samples = start - _size;
    size_t remaining = null_samples;
    size_t wrap = beforeWrap();
    if (null_samples > wrap) {
      addNullSamples(wrap);
      remaining -= wrap;
      sample_id += wrap;
    }
    addNullSamples(remaining);
    return null_samples;
  }

  return 0;
}

void SamplesQueue::addSamples(const float* interleaved, size_t num_samples, int64_t sample_id) {
  size_t skipped = skipSamples(sample_id);
#ifdef SQ_LOG
  std::cout << "Adding " << num_samples << " samples starting at\t" << sample_id;
#endif
  num_samples -= skipped;
  interleaved += skipped * _channels;
  sample_id += skipped;

  size_t wrap = beforeWrap();
  if (num_samples > wrap) {
    addSamplesWrapped(interleaved, wrap);
    num_samples -= wrap;
    interleaved += wrap * _channels;
  }
  addSamplesWrapped(interleaved, num_samples);
#ifdef SQ_LOG
  std::cout << " - " << _size << " elem: " 
    << _sample_id << "-" 
    << (_sample_id + _size - 1) << std::endl;
#endif
}

void SamplesQueue::readSamples(float* samples[], size_t num_samples, uint8_t num_channels, size_t src_index, size_t dst_index, float gain) {
  size_t start = (_head + src_index) % _max_size;
  size_t wrap = _max_size - start;
  // Copy channel 0 in any extra dst channels
  if (num_samples > wrap) {
    for (size_t i = 0; i < wrap; ++i) {
      for (uint8_t channel = 0; channel < num_channels; ++channel) {
        uint8_t src_channel = (channel < _channels) ? channel : 0;
        float pre_sample = samples[channel][dst_index + i];
        samples[channel][dst_index + i] += gain * _samples[src_channel][start + i];
        float post_sample = samples[channel][dst_index + i];
        // std::cout << "wrap pre sample " << pre_sample << " post sample " << post_sample << std::endl;
      }
    }
    num_samples -= wrap;
    start = 0;
    dst_index += wrap;
  } 
  for (size_t i = 0; i < num_samples; ++i) {
    for (uint8_t channel = 0; channel < num_channels; ++channel) {
      uint8_t src_channel = (channel < _channels) ? channel : 0;
      float pre_sample = samples[channel][dst_index + i];
      samples[channel][dst_index + i] += gain * _samples[src_channel][start + i];
      float post_sample = samples[channel][dst_index + i];
      // std::cout << "read pre sample " << pre_sample << " post sample " << post_sample << std::endl;
    }
  }
}

size_t SamplesQueue::readSamples(float* samples[], size_t num_samples, uint8_t num_channels, int64_t sample_id, float gain) {
#ifdef SQ_LOG
  std::cout << "Reading " << num_samples << " samples starting at\t" << sample_id;
#endif
  size_t dst = 0;
  int64_t start = (_sample_id == kInvalidSampleId) ? 0 : (sample_id - _sample_id);
  if (start < 0) {
    size_t null_samples = std::min(static_cast<size_t>(-start), num_samples);
    dst += null_samples;
    num_samples -= null_samples;
    start += null_samples;
  }

  size_t good_samples = 0;
  if (num_samples > 0 && start < _size) {
    good_samples = std::min(static_cast<size_t>(_size - start), num_samples);
    readSamples(samples, good_samples, num_channels, start, dst, gain);
    start += good_samples;
    dst += good_samples;
    num_samples -= good_samples;
  }

#ifdef SQ_LOG
  std::cout << " good " << good_samples << std::endl;
#endif
  return good_samples;
}

}