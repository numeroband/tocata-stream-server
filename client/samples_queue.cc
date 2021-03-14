#include "samples_queue.h"

#include <cstring>
#include <algorithm>
#include <iostream>

#define SQ_LOG

namespace tocata {

SamplesQueue::SamplesQueue(uint32_t max_size, uint8_t channels, uint32_t sample_rate) 
    : _samples(channels), _max_size(max_size), _sample_period(1e9 / sample_rate), _channels(channels) {
  for (uint8_t channel = 0; channel < channels; ++channel) {
    _samples[channel].resize(max_size);
    memset(_samples[channel].data(), 0xA5, max_size * sizeof(_samples[channel][0]));
  }
}

int64_t SamplesQueue::sampleIndex(uint64_t timestamp) {
  if (_timestamp == 0) {
    return 0;
  }
  // Add half period to go from -0.5 to 0.5 on each sample
  const float delta = static_cast<float>(timestamp - _timestamp);
  return static_cast<int64_t>(0.5 + (delta / _sample_period));
}

void SamplesQueue::setHeadAndSize(size_t num_samples, uint64_t timestamp) {
  _size += num_samples;
  if (_size > _max_size) {
    size_t extra = _size - _max_size;
    _head = (_head + extra) % _max_size;
    _size = _max_size;
    size_t delta = extra * _sample_period; 
    _timestamp += delta;
  }  
}

void SamplesQueue::addSamplesWrapped(const float* interleaved, size_t num_samples, uint64_t timestamp) {
  for (uint8_t channel = 0; channel < _channels; ++channel) {
    for (size_t sample = 0; sample < num_samples; ++sample) {
      _samples[channel][tail() + sample] = interleaved[sample * _channels + channel];
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
  if (_timestamp == 0 || (start - _size) >= _max_size) {
    assert(_timestamp == 0);
    _head = 0;
    _size = 0;
    _timestamp = timestamp;
    return 0;
  }

  if (start < _size) {
    std::cout << "Skipping " << _size - start << " early frames at " << timestamp << std::endl;
    return _size - start;
  }

  if (start > _size) {
    size_t null_samples = start - _size;
    std::cout << "Skipping " << null_samples << " null frames at " << timestamp << std::endl;
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
#ifdef SQ_LOG
  std::cout << "Adding " << num_samples << " samples starting at\t" << (timestamp / 1000);
#endif
  if (skipped) {
    num_samples -= skipped;
    interleaved += skipped * _channels;
    timestamp += skipped * _sample_period;
  }

  size_t wrap = beforeWrap();
  if (num_samples > wrap) {
    addSamplesWrapped(interleaved, wrap, timestamp);
    num_samples -= wrap;
    interleaved += wrap * _channels;
    timestamp += wrap * _sample_period;
  }
  addSamplesWrapped(interleaved, num_samples, timestamp);
#ifdef SQ_LOG
  std::cout << " - " << _size << " elem: " 
    << _timestamp / 1000 << "-" 
    << (_timestamp + (uint64_t)(_sample_period * _size)) / 1000 << std::endl;
#endif
}

void SamplesQueue::readNullSamples(float* samples[], size_t num_samples, uint8_t num_channels, size_t dst_index) {
  for (uint8_t channel = 0; channel < num_channels; ++channel) {
    memset(samples[channel] + dst_index, 0, num_samples * sizeof(samples[channel][0]));
  }
}

void SamplesQueue::readSamples(float* samples[], size_t num_samples, uint8_t num_channels, size_t src_index, size_t dst_index) {
  for (uint8_t channel = 0; channel < num_channels; ++channel) {
    size_t start = (_head + src_index) % _max_size;
    size_t wrap = _max_size - start;
    // Copy channel 0 in any extra dst channels
    uint8_t src_channel = (channel < _channels) ? channel : 0;
    if (num_samples > wrap) {
      memcpy(samples[channel] + dst_index, _samples[src_channel].data() + start, wrap * sizeof(_samples[0][0]));
      num_samples -= wrap;
      start = 0;
      dst_index += wrap;
    } 
    memcpy(samples[channel] + dst_index, _samples[src_channel].data() + start, num_samples * sizeof(_samples[0][0]));
  }
}

size_t SamplesQueue::readSamples(float* samples[], size_t num_samples, uint8_t num_channels, uint64_t timestamp) {
#ifdef SQ_LOG
  std::cout << "Reading " << num_samples << " samples starting at\t" << (timestamp / 1000);
#endif
  size_t dst = 0;
  int64_t start = sampleIndex(timestamp);
  if (start < 0) {
    size_t null_samples = std::min(static_cast<size_t>(-start), num_samples);
    readNullSamples(samples, null_samples, num_channels, dst);
    dst += null_samples;
    num_samples -= null_samples;
    start += null_samples;
  }

  size_t good_samples = 0;
  if (num_samples > 0 && start < _size) {
    good_samples = std::min(static_cast<size_t>(_size - start), num_samples);
    readSamples(samples, good_samples, num_channels, start, dst);
    start += good_samples;
    dst += good_samples;
    num_samples -= good_samples;
  }

  if (num_samples > 0 && start >= _size) {
    readNullSamples(samples, num_samples, num_channels, dst);
  }

#ifdef SQ_LOG
  std::cout << " good " << good_samples << std::endl;
#endif
  return good_samples;
}

}