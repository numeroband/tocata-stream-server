#include "connection.h"

#include <iostream>

namespace tocata {

void Connection::init(SendCandidatesCb sendCandidatesCb, ConnectedCb connectedCb) { 
	juice_config_t config;
	memset(&config, 0, sizeof(config));

	config.stun_server_host = "stun.stunprotocol.org";
	config.stun_server_port = 3478;
	config.cb_state_changed = [](auto agent, auto state, auto user_ptr) {
    static_cast<Connection*>(user_ptr)->onStateChanged(state);
  };
	config.cb_candidate = [](auto agent, auto sdp, auto user_ptr) {
    static_cast<Connection*>(user_ptr)->onCandidate(sdp);
  };
	config.cb_gathering_done = [](auto agent, auto user_ptr) {
    static_cast<Connection*>(user_ptr)->onGatheringDone();
  };
	config.cb_recv = [](auto agent, auto data, auto size, auto user_ptr) {
    static_cast<Connection*>(user_ptr)->onRecv(data, size);
  };
	config.user_ptr = this;

	_agent.reset(juice_create(&config));
  _sendCandidatesCb = sendCandidatesCb;
  _connectedCb = connectedCb;
};

void Connection::connect(const std::string& remote) {
  juice_set_remote_description(_agent.get(), remote.c_str());
  juice_gather_candidates(_agent.get());
}

void Connection::send(const void* data, size_t length) {
  auto ret = juice_send(_agent.get(), static_cast<const char*>(data), length);
  assert(ret == JUICE_ERR_SUCCESS);
}

void Connection::setRemoteCandidates(Candidates candidates) {
  for (auto& candidate : candidates) {
    juice_add_remote_candidate(_agent.get(), candidate.c_str());
  }
  juice_set_remote_gathering_done(_agent.get());
}

std::string Connection::description() {
	char sdp[JUICE_MAX_SDP_STRING_LEN];
	juice_get_local_description(_agent.get(), sdp, JUICE_MAX_SDP_STRING_LEN);
  return sdp;
}

void Connection::close() {
  std::cout << "Closing peer connection" << std::endl;
  _agent.reset();
}

void Connection::onStateChanged(juice_state_t state) {
  std::cout << "New state " << juice_state_to_string(state) << std::endl;
	if (state == JUICE_STATE_COMPLETED) {
    PingRequest request{};
    request.header.type = kPingRequest;
    std::cout << "Sending ping request" << std::endl;
    _pingSent = std::chrono::steady_clock::now();
    send(&request, sizeof(request));
	}

	if (state == JUICE_STATE_DISCONNECTED) {
    _connected = false;
    _connectedCb(false);
  }
}

void Connection::onCandidate(const char *sdp) {
  _candidates.push_back(sdp);
}

void Connection::onGatheringDone() {
  _sendCandidatesCb(_candidates);
}

void Connection::onRecv(const void *data, size_t size) {
  const MessageHeader& header = *static_cast<const MessageHeader*>(data);
  if (size < sizeof(header)) {
    std::cerr << "Message smaller than header" << std::endl;
    return;
  }

  if (header.seq != 0 && _lastSeq != 0 && header.seq != (_lastSeq + 1)) {
    std::cerr << "Packet loss " << _lastSeq << "->" << header.seq << std::endl;
  }
  _lastSeq = header.seq;
  if (header.seq % 100 == 0) {
    std::cerr << "Received " << header.seq << " frames" << std::endl;
  }

  switch (header.type) {
    case kPingRequest:
      onPingRequest(data, size);
      break;
    case kPingResponse:
      onPingResponse(data, size);
      break;
    case kAudio:
      onAudio(data, size);
      break;
    default:
      std::cerr << "Invalid message type " << header.type << std::endl;
      break;
  }
}

void Connection::onPingRequest(const void *data, size_t size) {
  std::cout << "Received ping request" << std::endl;
  PingResponse response{};
  response.header.type = kPingResponse;
  send(&response, sizeof(response));
}

void Connection::onPingResponse(const void *data, size_t size) {
  const PingResponse& response = *static_cast<const PingResponse*>(data);
  if (size < sizeof(response)) {
    std::cerr << "Message  smaller than ping response" << std::endl;
    return;
  }
  auto delay = (std::chrono::steady_clock::now() - _pingSent);
  std::cout << "Received ping response: " 
    << std::chrono::duration_cast<std::chrono::microseconds>(delay).count() << "us"
    << std::endl;
  _connected = true;
  _connectedCb(true);
}

void Connection::onAudio(const void *data, size_t size) {
  const AudioMessage& msg = *static_cast<const AudioMessage*>(data);
  if (size < sizeof(msg)) {
    std::cerr << "Message smaller than audio header" << std::endl;
    return;
  }

  if (size < (sizeof(msg) + msg.size)) {
    std::cerr << "Truncated audio message with size " << msg.size << std::endl;
    return;
  }

  std::vector<float> samples = _decoder.Decode(msg.bytes, msg.size, kFrameSize);
  std::lock_guard<std::mutex> lck(_mutex);
  _samples.addSamples(samples.data(), samples.size() / kNumChannels, msg.sample_id);
}

std::vector<uint8_t> Connection::BuildAudioMessage(const Connection::AudioInfo& info, const float* samples, Encoder& encoder) {
  size_t max_frame = kMaxEncodedFrame * info.channels;
  std::vector<uint8_t> result(sizeof(AudioMessage) + max_frame);
  AudioMessage& msg = *reinterpret_cast<AudioMessage*>(result.data());
  msg.header.type = kAudio;
  msg.header.seq = info.seq;
  msg.sample_id = info.sample_id;
  msg.stream_id = info.stream_id;
  msg.channels = info.channels;
  msg.size = static_cast<uint16_t>(encoder.Encode(samples, static_cast<int>(kFrameSize), static_cast<unsigned char*>(msg.bytes), max_frame));
  if (msg.size == 0) {
    return {};
  }
  result.resize(sizeof(AudioMessage) + msg.size);
  return result;
}

size_t Connection::receive(const AudioInfo& info, float* samples[], size_t num_samples) {
  std::lock_guard<std::mutex> lck(_mutex);
  if (_sample_offset == kInvalidOffset) {
    size_t headroom = num_samples + 4 * kFrameSize;
    if (_samples.size() < headroom) {
      SamplesQueue::readNullSamples(samples, num_samples, info.channels);
      return 0;
    }
    int64_t dst_sample_id = _samples.firstSampleId() + (_samples.size() - headroom);
    _sample_offset = dst_sample_id - info.sample_id;
    std::cout << "sample_offset " << _sample_offset
      << " num_samples " << num_samples
      << " dst_sample_id " << dst_sample_id
      << " (" << _samples.firstSampleId() 
      << "-" << _samples.firstSampleId() + _samples.size() - 1
      << ")" << std::endl;
  }
  size_t received = _samples.readSamples(samples, num_samples, info.channels, info.sample_id + _sample_offset);
  if (received == 0 && ++_zero_samples >= kMaxZeroSamples) {
    _zero_samples = 0;
    std::cerr << "Invalidating sample offset" << std::endl;
    _sample_offset = kInvalidOffset;
    _samples.reset();
  }
  return received;
}

}