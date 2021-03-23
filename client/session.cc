#include "session.h"
#include "connection.h"

#include <nlohmann/json.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

#include <functional>
#include <array>

using json = nlohmann::json;

typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

namespace tocata {

struct Session::Impl {
    void connect(const std::string& username, const std::string& password, StatusCb status_cb, PeerCb peer_cb);
    void processSamples(const AudioInfo& info, float* samples[], size_t num_samples);
    void sendSamples(const AudioInfo& info, float* samples[], size_t num_samples);

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

    static constexpr const size_t kMaxConnections = 5;

    static Connection::AudioInfo connAudioInfo(const AudioInfo& info, uint32_t seq = 0);

    Connection& getConnection(uint64_t peer_id);
    void initConnection(websocketpp::connection_hdl hdl, Connection& conn, uint64_t peer_id, std::string name);

    void sendLogin(websocketpp::connection_hdl hdl, const std::string& username, const std::string& password);
    void sendHello(websocketpp::connection_hdl hdl);
    void sendConnect(websocketpp::connection_hdl hdl, uint64_t peer_id, const std::string& description);
    void sendCandidates(websocketpp::connection_hdl hdl, uint64_t peer_id, const std::string& description, Connection::Candidates candidates);

    void onMessage(websocketpp::connection_hdl hdl, message_ptr msg);
    void onLogin(websocketpp::connection_hdl hdl, Status status, uint64_t peer_id = Connection::kInvalidPeerId, const std::string& name = "");
    void onHello(websocketpp::connection_hdl hdl, uint64_t peer_id, const std::string& name);
    void onBye(websocketpp::connection_hdl hdl, uint64_t peer_id);
    void onConnect(websocketpp::connection_hdl hdl, uint64_t peer_id, const std::string& name, const std::string& description);
    void onCandidates(websocketpp::connection_hdl hdl, uint64_t peer_id, const std::string& description, Connection::Candidates candidates);

    struct Stream {
      Connection::AudioInfo info;
      std::vector<float> samples;
    };

    ws_client _ws;
    StatusCb _status_cb;
    PeerCb _peer_cb;
    std::array<Connection, kMaxConnections> _connections{};
    Encoder _encoder{Connection::kSampleRate, Connection::kNumChannels};
    uint32_t _seq = 0;
    std::vector<Stream> _streams;
    std::mutex _mutex{};
    float _gain = 0.0;
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

void Session::processSamples(const AudioInfo& info, float* samples[], size_t num_samples) {
  _pimpl->processSamples(info, samples, num_samples);
}

void Session::Impl::connect(const std::string& username, const std::string& password, StatusCb status_cb, PeerCb peer_cb) {
  _status_cb = status_cb;
  _peer_cb = peer_cb;
  _gain = 0.0;

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
          if (conn.connected()) {
            conn.send(packet.data(), packet.size());
          }
        }
      }
      stream.samples.resize(0);
    }
  }
}

void Session::Impl::processSamples(const AudioInfo& info, float* samples[], size_t num_samples)
{
  float gain = _gain;
  // Send original samples
  sendSamples(info, samples, num_samples);

  // Apply gain to local audio
  for (uint8_t channel = 0; channel < info.channels; ++channel) {
    for (size_t sample = 0; sample < num_samples; ++sample) {
      samples[channel][sample] *= gain;
    }
  }

  // Received remote audio (each connection will aply its own gain)
  auto conn_info = connAudioInfo(info);
  for (auto& conn : _connections) {
    if (conn.connected()) {
      conn.receive(conn_info, samples, num_samples);
    }
  }  
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

Connection& Session::Impl::getConnection(uint64_t peer_id) {
  std::lock_guard<std::mutex> lk{_mutex};
  Connection* empty = nullptr;
  for (auto& conn : _connections) {
    if (conn.peerId() == peer_id) {
      return conn;
    }

    if (!empty && conn.peerId() == Connection::kInvalidPeerId) {
      empty = &conn;
    }
  }
  assert(empty);
  return *empty;
}

void Session::Impl::initConnection(websocketpp::connection_hdl hdl, Connection& conn, uint64_t peer_id, std::string name) {
  conn.init(peer_id, [this, hdl, &conn](auto candidates) {
    sendCandidates(hdl, conn.peerId(), conn.description(), candidates);
  }, [this, peer_id, name](bool connected, float* gain) {    
    std::cout << (connected ? "Connected to " : "Disconnected from ") << name << " - " << peer_id << std::endl;
    _peer_cb(peer_id, name, connected, gain);
  });
}

void Session::Impl::sendConnect(websocketpp::connection_hdl hdl, uint64_t peer_id, const std::string& description) {
  _ws.send(hdl, json{
    {kTypeKey, kConnectMsg}, 
    {kDstKey, peer_id},
    {kDescriptionKey, description},
  }.dump(), websocketpp::frame::opcode::text);
}

void Session::Impl::sendCandidates(websocketpp::connection_hdl hdl, uint64_t peer_id, const std::string& description, Connection::Candidates candidates)
{
  _ws.send(hdl, json{
    {kTypeKey, kCandidatesMsg}, 
    {kDstKey, peer_id},
    {kDescriptionKey, description},
    {kCandidatesKey, candidates},
  }.dump(), websocketpp::frame::opcode::text);
}

void Session::Impl::onMessage(websocketpp::connection_hdl hdl, message_ptr msg) {
  try {
    json obj = json::parse(msg->get_payload());
    std::string type = obj[kTypeKey];

    if (type == kLoginMsg) {
      Status status = obj[kStatusKey];
      if (status == kConnected) {
        onLogin(hdl, status, obj[kSenderKey], obj[kNameKey]);
      } else {
        onLogin(hdl, status);
      }

      return;
    }

    uint64_t sender = obj[kSenderKey];
    if (type == kHelloMsg) {
      onHello(hdl, sender, obj[kNameKey]);
    } else if (type == kByeMsg) {
      onBye(hdl, sender);
    } else if (type == kConnectMsg) {
      onConnect(hdl, sender, obj[kNameKey], obj[kDescriptionKey]);
    } else if (type == kCandidatesMsg) {
      onCandidates(hdl, sender, obj[kDescriptionKey], obj[kCandidatesKey]);
    } else {
      std::cerr << "Unknown message type " << type << std::endl;
    }
  } catch (std::exception const& ex) {
    std::cerr << "Standard exception raised: " << ex.what() << "\n";
  }
}

void Session::Impl::onLogin(websocketpp::connection_hdl hdl, Status status, uint64_t peer_id, const std::string& name) {
  std::cout << "Login status " << status << " name " << name << std::endl;
  _gain = 1.0;
  _status_cb(status, peer_id, name, &_gain);
  if (status == kConnected) {
      sendHello(hdl);
  } else {
    // disconnect socket
  }
}

void Session::Impl::onHello(websocketpp::connection_hdl hdl, uint64_t peer_id, const std::string& name) {
  std::cout << "Hello from " << name << " - " << peer_id << std::endl;
  auto& conn = getConnection(peer_id);
  if (!conn.invalid()) {
    return;
  }
  initConnection(hdl, conn, peer_id, name);
  sendConnect(hdl, peer_id, conn.description());
}

void Session::Impl::onBye(websocketpp::connection_hdl hdl, uint64_t peer_id) {
  std::cout << "Bye from " << peer_id << std::endl;
  auto& conn = getConnection(peer_id);
  if (conn.invalid()) {
    return;
  }
  conn.close();
}

void Session::Impl::onConnect(websocketpp::connection_hdl hdl, uint64_t peer_id, const std::string& name, const std::string& description) {
  std::cout << "Connect from " << name << " - " << peer_id << std::endl;
  auto& conn = getConnection(peer_id);
  if (!conn.invalid()) {
    return;
  }
  initConnection(hdl, conn, peer_id, name);
  conn.connect(description);
}

void Session::Impl::onCandidates(websocketpp::connection_hdl hdl, uint64_t peer_id, const std::string& description, Connection::Candidates candidates) {
  std::cout << "Candidates from " << peer_id << std::endl;
  auto& conn = getConnection(peer_id);
  if (conn.invalid()) {
    return;
  }
  conn.setRemoteCandidates(candidates);
  if (!conn.connecting()) {
    conn.connect(description);
  }
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