#include "session.h"
#include "connection.h"

#include <nlohmann/json.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

#include <functional>
#include <unordered_map>

using json = nlohmann::json;

typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

namespace tocata {

struct Session::Impl {
    void connect(const std::string& username, const std::string& password, StatusCb status_cb, PeerCb peer_cb);
    void sendSamples(const AudioInfo& info, float* samples[], size_t num_samples);
    size_t receiveSamples(const std::string& peer_id, const AudioInfo& info, float* samples[], size_t num_samples);

#ifdef TOCATA_LOCAL
    typedef websocketpp::client<websocketpp::config::asio_client> ws_client;
    static constexpr const char* kWebsocketServer = "ws://localhost:5000";
#else
    typedef websocketpp::client<websocketpp::config::asio_tls_client> ws_client;
    static constexpr const char* kWebsocketServer = "wss://pacific-stream-85481.herokuapp.com";
#endif

    static constexpr const char* kUsernameKey = "username";
    static constexpr const char* kPasswordKey = "password";
    static constexpr const char* kStatusKey = "status";
    static constexpr const char* kNameKey = "name";
    static constexpr const char* kTypeKey = "type";
    static constexpr const char* kSenderKey = "sender";
    static constexpr const char* kDstKey = "dst";
    static constexpr const char* kDescriptionKey = "description";
    static constexpr const char* kCandidatesKey = "candidates";

    static constexpr const char* kLoginMsg = "Login";
    static constexpr const char* kHelloMsg = "Hello";
    static constexpr const char* kByeMsg = "Bye";
    static constexpr const char* kConnectMsg = "Connect";
    static constexpr const char* kCandidatesMsg = "Candidates";

    static Connection::AudioInfo connAudioInfo(const AudioInfo& info, uint32_t seq = 0);

    void sendLogin(websocketpp::connection_hdl hdl, const std::string& username, const std::string& password);
    void sendHello(websocketpp::connection_hdl hdl);
    void sendConnect(websocketpp::connection_hdl hdl, std::string peer_id, std::string name);
    void sendCandidates(websocketpp::connection_hdl hdl, std::string peer_id, Connection::Candidates candidates);

    void onMessage(websocketpp::connection_hdl hdl, message_ptr msg);
    void onLogin(websocketpp::connection_hdl hdl, Status status, const std::string& name);
    void onHello(websocketpp::connection_hdl hdl, const std::string& peer_id, const std::string& name);
    void onBye(websocketpp::connection_hdl hdl, const std::string& peer_id);
    void onConnect(websocketpp::connection_hdl hdl, const std::string& peer_id, const std::string& name, const std::string& description);
    void onCandidates(websocketpp::connection_hdl hdl, std::string peer_id, Connection::Candidates candidates);

    struct Stream {
      Connection::AudioInfo info;
      std::vector<float> samples;
    };

    ws_client _ws;
    StatusCb _status_cb;
    PeerCb _peer_cb;
    std::unordered_map<std::string, Connection> _connections;
    Encoder _encoder{Connection::kSampleRate, Connection::kNumChannels};
    uint32_t _seq = 0;
    std::vector<Stream> _streams;
};

Session::Session() : _pimpl(std::make_unique<Impl>()) {
  _pimpl->_streams.resize(kMaxStreams);
  for (auto& stream : _pimpl->_streams) {
    stream.samples.reserve(Connection::kFrameSize * Connection::kNumChannels);
    stream.samples.resize(0);
  }
}

Session::~Session() = default;

void Session::connect(const std::string& username, const std::string& password, StatusCb status_cb, PeerCb peer_cb) {
  _pimpl->connect(username, password, status_cb, peer_cb);
}

void Session::sendSamples(const AudioInfo& info, float* samples[], size_t num_samples) {
  _pimpl->sendSamples(info, samples, num_samples);
}

size_t Session::receiveSamples(const std::string& peer_id, const AudioInfo& info, float* samples[], size_t num_samples) {
  return _pimpl->receiveSamples(peer_id, info, samples, num_samples);
}

void Session::Impl::connect(const std::string& username, const std::string& password, StatusCb status_cb, PeerCb peer_cb) {
  _status_cb = status_cb;
  _peer_cb = peer_cb;

  _ws.set_access_channels(websocketpp::log::alevel::none);
  _ws.init_asio();

  _ws.set_open_handler([this, username, password](auto h) {
    std::cout << "WS connected" << std::endl;
    sendLogin(h, username, password);
  });
  _ws.set_message_handler(bind(&Session::Impl::onMessage, this, ::_1, ::_2));
#ifndef TOCATA_LOCAL
  _ws.set_tls_init_handler([](auto handler) {
    context_ptr ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::sslv23);
    ctx->set_options(asio::ssl::context::default_workarounds |
                      asio::ssl::context::no_sslv2 |
                      asio::ssl::context::no_sslv3 |
                      asio::ssl::context::single_dh_use);
    return ctx;
  });
#endif

  websocketpp::lib::error_code ec;
  ws_client::connection_ptr con = _ws.get_connection(kWebsocketServer, ec);
  if (ec) throw std::runtime_error(ec.message());

  _ws.connect(con);
  _ws.run();  
}

void Session::Impl::sendSamples(const AudioInfo& info, float* samples[], size_t num_samples)
{
  auto& stream = _streams[info.stream_id];
  for (size_t i = 0; i < num_samples; ++i) {
    if (stream.samples.size() == 0) {
      stream.info = connAudioInfo(info);
      stream.info.sample_id += i;
    }
    stream.samples.push_back(samples[0][i]);
    stream.samples.push_back(samples[info.channels > 0 ? 1 : 0][i]);
  
    if (stream.samples.size() == Connection::kNumChannels * Connection::kFrameSize) {
      stream.info.seq = ++_seq;
      auto packet = Connection::BuildAudioMessage(stream.info, stream.samples.data(), _encoder);
      if (!packet.empty()) {
        for (auto& conn : _connections) {
          if (conn.second.connected()) {
            conn.second.send(packet.data(), packet.size());
          }
        }
      }
      stream.samples.resize(0);
    }
  }
}

size_t Session::Impl::receiveSamples(const std::string& peer_id, const AudioInfo& info, float* samples[], size_t num_samples)
{
  auto peer_iter = _connections.find(peer_id);
  if (peer_iter == _connections.end() || !peer_iter->second.connected()) {
    for (uint8_t channel = 0; channel < info.channels; ++channel) {
      memset(samples[channel], 0, num_samples * sizeof(samples[channel][0]));
    }
    return false;
  }
  return peer_iter->second.receive(connAudioInfo(info), samples, num_samples);
}

void Session::Impl::sendLogin(websocketpp::connection_hdl hdl, const std::string& username, const std::string& password) {
  _ws.send(hdl, json{
    {kTypeKey, kLoginMsg}, 
    {kUsernameKey, username},
    {kPasswordKey, password},
  }.dump(), websocketpp::frame::opcode::text);
}

void Session::Impl::sendHello(websocketpp::connection_hdl hdl) {
  _ws.send(hdl, json{
    {kTypeKey, kHelloMsg}, 
  }.dump(), websocketpp::frame::opcode::text);
}

void Session::Impl::sendConnect(websocketpp::connection_hdl hdl, std::string peer_id, std::string name) {
  auto& conn = _connections[peer_id];
  conn.init([this, hdl, peer_id](auto candidates) {
    sendCandidates(hdl, peer_id, candidates);
  }, [this, peer_id, name, &conn](bool connected) {    
    std::cout << (connected ? "Connected to " : "Disconnected from ") << name << " - " << peer_id << std::endl;
    _peer_cb(peer_id, name, connected);
  });
  _ws.send(hdl, json{
    {kTypeKey, kConnectMsg}, 
    {kDstKey, peer_id},
    {kDescriptionKey, conn.description()},
  }.dump(), websocketpp::frame::opcode::text);
}

void Session::Impl::sendCandidates(websocketpp::connection_hdl hdl, std::string peer_id, Connection::Candidates candidates)
{
  _ws.send(hdl, json{
    {kTypeKey, kCandidatesMsg}, 
    {kDstKey, peer_id},
    {kCandidatesKey, candidates},
  }.dump(), websocketpp::frame::opcode::text);
}

void Session::Impl::onMessage(websocketpp::connection_hdl hdl, message_ptr msg) {
  try {
    json obj = json::parse(msg->get_payload());
    std::string type = obj[kTypeKey];

    if (type == kLoginMsg) {
      Status status = obj[kStatusKey];
      onLogin(hdl, status, status == kConnected ? obj[kNameKey] : "");
      return;
    }

    std::string sender = obj[kSenderKey];
    if (type == kHelloMsg) {
      onHello(hdl, sender, obj[kNameKey]);
    } else if (type == kByeMsg) {
      onBye(hdl, sender);
    } else if (type == kConnectMsg) {
      onConnect(hdl, sender, obj[kNameKey], obj[kDescriptionKey]);
    } else if (type == kCandidatesMsg) {
      onCandidates(hdl, sender, obj[kCandidatesKey]);
    } else {
      std::cerr << "Unknown message type " << type << std::endl;
    }
  } catch (std::exception const& ex) {
    std::cerr << "Standard exception raised: " << ex.what() << "\n";
  }
}

void Session::Impl::onLogin(websocketpp::connection_hdl hdl, Status status, const std::string& name) {
  std::cout << "Login status " << status << " name " << name << std::endl;
  _status_cb(status, name);
  if (status == kConnected) {
      sendHello(hdl);
  } else {
    // disconnect socket
  }
}

void Session::Impl::onHello(websocketpp::connection_hdl hdl, const std::string& peer_id, const std::string& name) {
  std::cout << "Hello from " << name << " - " << peer_id << std::endl;
  sendConnect(hdl, peer_id, name);
}

void Session::Impl::onBye(websocketpp::connection_hdl hdl, const std::string& peer_id) {
  std::cout << "Bye from " << peer_id << std::endl;
  auto con_iter = _connections.find(peer_id);
  if (con_iter != _connections.end()) {
    con_iter->second.close();
  }
}

void Session::Impl::onConnect(websocketpp::connection_hdl hdl, const std::string& peer_id, const std::string& name, const std::string& description) {
  std::cout << "Connect from " << name << " - " << peer_id << std::endl;
  if (_connections.find(peer_id) == _connections.end()) {
    sendConnect(hdl, peer_id, name);
  }
  auto& conn = _connections[peer_id];
  conn.connect(description);
}

void Session::Impl::onCandidates(websocketpp::connection_hdl hdl, std::string peer_id, Connection::Candidates candidates) {
  std::cout << "Candidates from " << peer_id << std::endl;
  auto& conn = _connections[peer_id];
  conn.setRemoteCandidates(candidates);
}

Connection::AudioInfo Session::Impl::connAudioInfo(const AudioInfo& info, uint32_t seq) { 
  return {
    seq,
    info.sample_id,
    info.stream_id,
    info.channels,
  };
}

}