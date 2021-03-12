#pragma once

#include "opus_wrapper.h"

namespace tocata {

class OpusRtpEncoder {
public:
  OpusRtpEncoder();
  bool encode(float* samples, size_t num_samples);

private:
};

class OpusRtpDecoder {

};

}