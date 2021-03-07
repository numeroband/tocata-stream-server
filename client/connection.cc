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
  if (_connected) {
		juice_send(_agent.get(), static_cast<const char*>(data), length);
  }
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
	if (state == JUICE_STATE_CONNECTED) {
    _connected = true;
    _connectedCb();
	}
}

void Connection::onCandidate(const char *sdp) {
  _candidates.push_back(sdp);
}

void Connection::onGatheringDone() {
  _sendCandidatesCb(_candidates);
}

void Connection::onRecv(const char *data, size_t size) {
  constexpr size_t BUFFER_SIZE = 128;
	char buffer[BUFFER_SIZE];
	if (size > BUFFER_SIZE - 1)
		size = BUFFER_SIZE - 1;
	memcpy(buffer, data, size);
	buffer[size] = '\0';
	std::cout << "Received: " << buffer << std::endl;
}

}