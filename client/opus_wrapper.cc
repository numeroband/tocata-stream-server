// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iterator>
#include <memory>
#include <string>
#include <vector>
#include <iostream>

#include "opus_wrapper.h"

#define LOG(...) std::cout

std::string opus::ErrorToString(int error) {
  switch (error) {
    case OPUS_OK:
      return "OK";
    case OPUS_BAD_ARG:
      return "One or more invalid/out of range arguments.";
    case OPUS_BUFFER_TOO_SMALL:
      return "The mode struct passed is invalid.";
    case OPUS_INTERNAL_ERROR:
      return "An internal error was detected.";
    case OPUS_INVALID_PACKET:
      return "The compressed data passed is corrupted.";
    case OPUS_UNIMPLEMENTED:
      return "Invalid/unsupported request number.";
    case OPUS_INVALID_STATE:
      return "An encoder or decoder structure is invalid or already freed.";
    default:
      return "Unknown error code: " + std::to_string(error);
  }
}

void opus::internal::OpusDestroyer::operator()(OpusEncoder* encoder) const
    noexcept {
  opus_encoder_destroy(encoder);
}

void opus::internal::OpusDestroyer::operator()(OpusDecoder* decoder) const
    noexcept {
  opus_decoder_destroy(decoder);
}

opus::Encoder::Encoder(opus_int32 sample_rate, int num_channels,
                       int application, int expected_loss_percent)
    : num_channels_{num_channels} {
  int error{};
  encoder_.reset(
      opus_encoder_create(sample_rate, num_channels, application, &error));
  valid_ = error == OPUS_OK;
  if (!valid()) {
    LOG(INFO) << "Could not construct encoder. Error: " << ErrorToString(error);
    return;
  }
  if (expected_loss_percent > 0) {
    LOG(INFO) << "Enabling FEC in the encoder.";
    Ctl(OPUS_SET_INBAND_FEC(1));
    Ctl(OPUS_SET_PACKET_LOSS_PERC(expected_loss_percent));
  }
}

bool opus::Encoder::ResetState() {
  valid_ = Ctl(OPUS_RESET_STATE) == OPUS_OK;
  return valid_;
}

bool opus::Encoder::SetBitrate(int bitrate) {
  valid_ = Ctl(OPUS_SET_BITRATE(bitrate)) == OPUS_OK;
  return valid_;
}

bool opus::Encoder::SetVariableBitrate(int vbr) {
  valid_ = Ctl(OPUS_SET_VBR(vbr)) == OPUS_OK;
  return valid_;
}

bool opus::Encoder::SetComplexity(int complexity) {
  valid_ = Ctl(OPUS_SET_COMPLEXITY(complexity)) == OPUS_OK;
  return valid_;
}

int opus::Encoder::GetLookahead() {
  opus_int32 skip{};
  valid_ = Ctl(OPUS_GET_LOOKAHEAD(&skip)) == OPUS_OK;
  return skip;
}

size_t opus::Encoder::Encode(
      const float* pcm, int frame_size, unsigned char* encoded, size_t encoded_size) {
  const auto frame_length = (frame_size * num_channels_ * sizeof(pcm[0]));
  auto num_bytes = opus_encode_float(encoder_.get(), pcm, frame_size,
                               encoded, encoded_size);
  if (num_bytes < 0) {
    LOG(ERROR) << "Encode error: " << opus::ErrorToString(num_bytes);
    return 0;
  }
  return static_cast<size_t>(num_bytes);
}

std::vector<std::vector<unsigned char>> opus::Encoder::Encode(
    const std::vector<float>& pcm, int frame_size) {

      /////////////////////////
  std::vector<std::vector<unsigned char>> result(1);
  result[0].resize(frame_size * 2 * sizeof(pcm[0]));
  memcpy(result[0].data(), pcm.data(), result[0].size());
  // float* res_float = (float*)result[0].data();
  // printf("encoded %f,%f,%f,%f->%f,%f,%f,%f\n", 
  // pcm[0], 
  // pcm[1], 
  // pcm[2], 
  // pcm[3], 
  // res_float[0], 
  // res_float[1], 
  // res_float[2], 
  // res_float[3]);
  return result;
      ///////////

  constexpr auto sample_size = sizeof(pcm[0]);
  const auto frame_length = frame_size * num_channels_ * sample_size;
  auto data_length = pcm.size() * sample_size;
  if (data_length % frame_length != 0u) {
    LOG(WARNING) << "PCM samples contain an incomplete frame. Ignoring the "
                    "incomplete frame.";
    data_length -= (data_length % frame_length);
  }

  std::vector<std::vector<unsigned char>> encoded;
  for (std::size_t i{}; i < data_length; i += frame_length) {
    encoded.push_back(EncodeFrame(pcm.begin() + (i / sample_size), frame_size));
  }
  return encoded;
}

std::vector<unsigned char> opus::Encoder::EncodeFrame(
    const std::vector<float>::const_iterator& frame_start,
    int frame_size) {
  const auto frame_length = (frame_size * num_channels_ * sizeof(*frame_start));
  std::vector<unsigned char> encoded(frame_length);
  auto num_bytes = opus_encode_float(encoder_.get(), &*frame_start, frame_size,
                               encoded.data(), encoded.size());
  if (num_bytes < 0) {
    LOG(ERROR) << "Encode error: " << opus::ErrorToString(num_bytes);
    return {};
  }
  encoded.resize(num_bytes);
  return encoded;
}

opus::Decoder::Decoder(opus_uint32 sample_rate, int num_channels)
    : num_channels_(num_channels) {
  int error{};
  decoder_.reset(opus_decoder_create(sample_rate, num_channels, &error));
  valid_ = error == OPUS_OK;
}

std::vector<float> opus::Decoder::Decode(
    const std::vector<std::vector<unsigned char>>& packets, int frame_size,
    bool decode_fec) {
  std::vector<float> decoded;
  for (const auto& enc : packets) {
    auto just_decoded = Decode(enc, frame_size, decode_fec);
    decoded.insert(std::end(decoded), std::begin(just_decoded),
                   std::end(just_decoded));
  }
  return decoded;
}

std::vector<float> opus::Decoder::Decode(
    const unsigned char* packet, size_t packet_size, int frame_size) {
  const auto frame_length = (frame_size * num_channels_ * sizeof(opus_int16));
  std::vector<float> decoded(frame_length);
  auto num_samples = opus_decode_float(decoder_.get(), packet, packet_size,
                                 decoded.data(), frame_size, false);
  if (num_samples < 0) {
    LOG(ERROR) << "Decode error: " << opus::ErrorToString(num_samples);
    return {};
  }
  decoded.resize(num_samples * num_channels_);
  return decoded;
}


std::vector<float> opus::Decoder::Decode(
    const std::vector<unsigned char>& packet, int frame_size, bool decode_fec) {

      /////////////////////////
  std::vector<float> result(2 * frame_size);
  memcpy(result.data(), packet.data(), 2 * frame_size * sizeof(result[0]));
  // float* src = (float*)packet.data();
  // printf("decoded %f,%f,%f,%f->%f,%f,%f,%f\n", 
  // src[0], 
  // src[1], 
  // src[2], 
  // src[3], 
  // result[0], 
  // result[1], 
  // result[2], 
  // result[3]);
  return result;
      ///////////


  const auto frame_length = (frame_size * num_channels_ * sizeof(opus_int16));
  std::vector<float> decoded(frame_length);
  auto num_samples = opus_decode_float(decoder_.get(), packet.data(), packet.size(),
                                 decoded.data(), frame_size, decode_fec);
  if (num_samples < 0) {
    LOG(ERROR) << "Decode error: " << opus::ErrorToString(num_samples);
    return {};
  }
  decoded.resize(num_samples * num_channels_);
  return decoded;
}

std::vector<float> opus::Decoder::DecodeDummy(int frame_size) {
  const auto frame_length = (frame_size * num_channels_ * sizeof(opus_int16));
  std::vector<float> decoded(frame_length);
  auto num_samples =
      opus_decode_float(decoder_.get(), nullptr, 0, decoded.data(), frame_size, true);
  if (num_samples < 0) {
    LOG(ERROR) << "Decode error: " << opus::ErrorToString(num_samples);
    return {};
  }
  decoded.resize(num_samples * num_channels_);
  return decoded;
}
