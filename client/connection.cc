#include "connection.h"

#include <iostream>

namespace tocata {

Connection::Connection() {
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
}

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
}

void Connection::onStateChanged(juice_state_t state) {
  std::cout << "New state " << juice_state_to_string(state) << std::endl;
	if (state == JUICE_STATE_COMPLETED) {
    PingRequest request{};
    request.header.type = kPingRequest;
    std::cout << "Sending ping request" << std::endl;
    send(&request, sizeof(request));
	}

	if (state == JUICE_STATE_DISCONNECTED) {
    _connected = false;
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
  auto now = std::chrono::system_clock::now();
  _pingSent = now;
  std::cout << "Received ping request at " << std::chrono::nanoseconds(now.time_since_epoch()).count() << std::endl;
  PingResponse response{};
  response.header.type = kPingResponse;
  response.host_timestamp = std::chrono::nanoseconds(now.time_since_epoch()).count();
  send(&response, sizeof(response));
}

void Connection::onPingResponse(const void *data, size_t size) {
  const PingResponse& response = *static_cast<const PingResponse*>(data);
  if (size < sizeof(response)) {
    std::cerr << "Message  smaller than ping response" << std::endl;
    return;
  }
  auto delay = (std::chrono::system_clock::now() - _pingSent);
  std::cout << "Received ping response:"
    << " local " << std::chrono::nanoseconds((_pingSent + (delay / 2)).time_since_epoch()).count()
    << " remote " << response.host_timestamp 
    << std::endl;
  _connected = true;
  _connectedCb();
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
  if (_sample_offset == kInvalidOffset) {
    _remote_sample_timestamp = {msg.sample_id, msg.host_timestamp};
    if (_local_sample_timestamp) {
      calculateSampleOffset();
    }
  }
  _samples.addSamples(samples.data(), samples.size() / kNumChannels, msg.sample_id);
}

std::vector<uint8_t> Connection::BuildAudioMessage(const Connection::AudioInfo& info, const float* samples, opus::Encoder& encoder) {
  auto now = std::chrono::system_clock::now().time_since_epoch();
  uint64_t delta = static_cast<uint64_t>(kSamplePeriod * kFrameSize);
  size_t max_frame = kMaxEncodedFrame * info.channels;
  std::vector<uint8_t> result(sizeof(AudioMessage) + max_frame);
  AudioMessage& msg = *reinterpret_cast<AudioMessage*>(result.data());
  msg.header.type = kAudio;
  msg.header.seq = info.seq;
  msg.sample_id = info.sample_id;
  msg.host_timestamp = std::chrono::nanoseconds(now).count() - delta;
  msg.stream_id = info.stream_id;
  msg.channels = info.channels;
  msg.size = static_cast<uint16_t>(encoder.Encode(samples, static_cast<int>(kFrameSize), static_cast<unsigned char*>(msg.bytes), max_frame));
  if (msg.size == 0) {
    return {};
  }
  result.resize(sizeof(AudioMessage) + msg.size);
  return result;
}

void Connection::calculateSampleOffset() {
  _sample_offset = _remote_sample_timestamp.sample_id - _local_sample_timestamp.sample_id;
  int64_t timestamps_delta = _remote_sample_timestamp.timestamp - _local_sample_timestamp.timestamp;
  int64_t samples_delta = timestamps_delta / kSamplePeriod;
  _sample_offset += samples_delta - kFrameSize * 1;
  std::cout << "sample offset " << _sample_offset
    << " local " << _local_sample_timestamp.sample_id 
    << " - " << _local_sample_timestamp.timestamp
    << " remote " << _remote_sample_timestamp.sample_id
    << " - " << _remote_sample_timestamp.timestamp
    << std::endl;
}

size_t Connection::receive(const AudioInfo& info, float* samples[], size_t num_samples) {
  std::lock_guard<std::mutex> lck(_mutex);
  if (_sample_offset == kInvalidOffset) {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    int64_t delta = static_cast<uint64_t>(kSamplePeriod * num_samples);
    uint64_t local_timestamp = static_cast<uint64_t>(std::chrono::nanoseconds(now).count() - delta);
    _local_sample_timestamp = {info.sample_id, local_timestamp};

    if (_remote_sample_timestamp) {
      calculateSampleOffset();
    } else {
      SamplesQueue::readNullSamples(samples, num_samples, info.channels);
      return 0;
    }
  }
  size_t received = _samples.readSamples(samples, num_samples, info.channels, info.sample_id + _sample_offset);
  if (received == 0 && ++_zero_samples == 10) {
    _zero_samples = 0;
    std::cout << "Invalidating sample offset" << std::endl;
    _sample_offset = kInvalidOffset;
    _local_sample_timestamp = {};
    _remote_sample_timestamp = {};
  }
  return received;
}

}