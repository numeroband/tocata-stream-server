#include "samples_queue.h"

#include <cstring>

namespace tocata {

SamplesQueue::SamplesQueue(uint32_t max_size, uint8_t channels, uint32_t sample_rate) 
    : _samples(channels), _max_size(max_size), _sample_period(1e9 / sample_rate), _channels(channels) {
  for (uint8_t channel = 0; channel < channels; ++channel) {
    _samples[channel].resize(max_size);
  }
}

int64_t SamplesQueue::sampleIndex(uint64_t timestamp) {
  if (_timestamp == 0) {
    return 0;
  }
  return static_cast<int64_t>(timestamp - _timestamp) / _sample_period;
}

void SamplesQueue::setHeadAndSize(size_t num_samples, uint64_t timestamp) {
  _size += num_samples;
  if (_size > _max_size) {
    size_t extra = _size - _max_size;
    _head = (_head + extra) % _max_size;
    _size -= extra;
    _timestamp = timestamp + (extra * _sample_period);
  }  
}

void SamplesQueue::addSamplesWrapped(const float* interleaved, size_t num_samples, uint64_t timestamp) {
  for (uint8_t channel = 0; channel < _channels; ++channel) {
    for (size_t sample = 0; sample < num_samples; ++sample) {
      _samples[channel][sample] = interleaved[sample * _channels + channel];
    }
  }
  setHeadAndSize(num_samples, timestamp);
}

void SamplesQueue::addNullSamples(size_t num_samples, uint64_t timestamp) {
  for (uint8_t channel = 0; channel < _channels; ++channel) {
    memset(_samples[channel].data() + tail(), 0, num_samples);
  }
  setHeadAndSize(num_samples, timestamp);
}

size_t SamplesQueue::skipSamples(uint64_t timestamp) {
  int64_t start = sampleIndex(timestamp);
  if (_timestamp == 0 || start >= _max_size) {
    _head = 0;
    _size = 0;
    _timestamp = timestamp;
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
      addNullSamples(wrap, timestamp);
      remaining -= wrap;
      timestamp += wrap * _sample_period;
    }
    addNullSamples(remaining, timestamp);
    return null_samples;
  }

  return 0;
}

void SamplesQueue::addSamples(const float* interleaved, size_t num_samples, uint64_t timestamp) {
    size_t skipped = skipSamples(timestamp);
    num_samples -= skipped;
    interleaved += skipped * _channels;
    timestamp += skipped * _sample_period;

    size_t wrap = beforeWrap();
    if (num_samples > wrap) {
      addSamplesWrapped(interleaved, wrap, timestamp);
      num_samples -= wrap;
      interleaved += wrap * _channels;
      timestamp += wrap * _sample_period;
    }
    addSamplesWrapped(interleaved, num_samples, timestamp);
}

void SamplesQueue::readSamples(float* samples[], size_t num_samples, uint8_t num_channels, uint64_t timestamp) {
    for (uint8_t channel = 0; channel < num_channels; ++channel) {
      memset(samples[channel], 0, num_samples * sizeof(samples[channel][0]));
    }
}

}