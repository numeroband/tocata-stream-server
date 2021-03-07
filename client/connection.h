#pragma once

#include "juice/juice.h"

#include <vector>

namespace tocata {

class Connection {
public: 
  using Candidates = std::vector<std::string>;
  using SendCandidatesCb = std::function<void(Candidates)>;
  using ConnectedCb = std::function<void()>;

  Connection();
  void connect(const std::string& remote);
  void setRemoteCandidates(Candidates candidates);
  void close();
  std::string description();
  bool connected() { return _connected; };
  void setSendCandidatesCb(SendCandidatesCb cb) { _sendCandidatesCb = cb; }
  void setConnectedCb(ConnectedCb cb) { _connectedCb = cb; }
  void send(const void* data, size_t length);

private:
  void onStateChanged(juice_state_t state);
  void onCandidate(const char *sdp);
  void onGatheringDone();
  void onRecv(const char *data, size_t size);

  std::atomic<bool> _connected = false;
  std::unique_ptr<juice_agent_t, decltype(&juice_destroy)> _agent{nullptr, &juice_destroy};
  Candidates _candidates;
  SendCandidatesCb _sendCandidatesCb;
  ConnectedCb _connectedCb;
};

}