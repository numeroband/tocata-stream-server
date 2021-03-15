#include "session.h"
#include "connection.h"
#include "opus_wrapper.h"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
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
    void connect(const std::string& username, const std::string& password);
    void sendSamples(const AudioInfo& info, float* samples[], size_t num_samples);
    size_t receiveSamples(const std::string& peer, const AudioInfo& info, float* samples[], size_t num_samples);

#ifdef TOCATA_LOCAL
    typedef websocketpp::client<websocketpp::config::asio_client> ws_client;
    static constexpr const char* kHttpServer = "http://localhost:5000";
    static constexpr const char* kWebsocketServer = "ws://localhost:5000";
#else
    typedef websocketpp::client<websocketpp::config::asio_tls_client> ws_client;
    static constexpr const char* kHttpServer = "https://pacific-stream-85481.herokuapp.com";
    static constexpr const char* kWebsocketServer = "wss://pacific-stream-85481.herokuapp.com";
#endif

    static constexpr const char* kUsernameKey = "username";
    static constexpr const char* kPasswordKey = "password";
    static constexpr const char* kTokenKey = "token";
    static constexpr const char* kTypeKey = "type";
    static constexpr const char* kSenderKey = "sender";
    static constexpr const char* kDstKey = "dst";
    static constexpr const char* kDescriptionKey = "description";
    static constexpr const char* kCandidatesKey = "candidates";

    static constexpr const char* kHelloMsg = "Hello";
    static constexpr const char* kByeMsg = "Bye";
    static constexpr const char* kConnectMsg = "Connect";
    static constexpr const char* kCandidatesMsg = "Candidates";

    static Connection::AudioInfo connAudioInfo(const AudioInfo& info, uint32_t seq = 0);

    std::string join(const std::string& username, const std::string& password);

    void sendHello(websocketpp::connection_hdl hdl);
    void sendConnect(websocketpp::connection_hdl hdl, std::string username);
    void sendCandidates(websocketpp::connection_hdl hdl, std::string username, Connection::Candidates candidates);

    void onMessage(websocketpp::connection_hdl hdl, message_ptr msg);    
    void onHello(websocketpp::connection_hdl hdl, const std::string& username);
    void onBye(websocketpp::connection_hdl hdl, const std::string& username);
    void onConnect(websocketpp::connection_hdl hdl, const std::string& username, const std::string& description);
    void onCandidates(websocketpp::connection_hdl hdl, std::string username, Connection::Candidates candidates);

    struct Stream {
      Connection::AudioInfo info;
      std::vector<float> samples;
    };

    ws_client _ws;
    std::string _token;
    std::unordered_map<std::string, Connection> _connections;
    opus::Encoder _encoder{Connection::kSampleRate, Connection::kNumChannels, OPUS_APPLICATION_RESTRICTED_LOWDELAY};
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

void Session::connect(const std::string& username, const std::string& password) {
  _pimpl->connect(username, password);
}

void Session::sendSamples(const AudioInfo& info, float* samples[], size_t num_samples) {
  _pimpl->sendSamples(info, samples, num_samples);
}

size_t Session::receiveSamples(const std::string& peer, const AudioInfo& info, float* samples[], size_t num_samples) {
  return _pimpl->receiveSamples(peer, info, samples, num_samples);
}

std::string Session::Impl::join(const std::string& username, const std::string& password) {
  httplib::Client http{kHttpServer};
  json req = {
    {kUsernameKey, username},
    {kPasswordKey, password},
  };
  auto res = http.Post("/join", req.dump(), "application/json");
  if (!res || res->status != 201) throw std::runtime_error("Cannot join session");
  return res->body;
}

void Session::Impl::connect(const std::string& username, const std::string& password) {
  _token = join(username, password);

  _ws.set_access_channels(websocketpp::log::alevel::none);
  _ws.init_asio();

  _ws.set_open_handler([this](auto h) {
    std::cout << "Connected" << std::endl;
    sendHello(h);
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

size_t Session::Impl::receiveSamples(const std::string& peer, const AudioInfo& info, float* samples[], size_t num_samples)
{
  auto peer_iter = _connections.find(peer);
  if (peer_iter == _connections.end() || !peer_iter->second.connected()) {
    for (uint8_t channel = 0; channel < info.channels; ++channel) {
      memset(samples[channel], 0, num_samples * sizeof(samples[channel][0]));
    }
    return false;
  }
  return peer_iter->second.receive(connAudioInfo(info), samples, num_samples);
}

void Session::Impl::sendHello(websocketpp::connection_hdl hdl) {
  _ws.send(hdl, json{
    {kTypeKey, kHelloMsg}, 
    {kTokenKey, _token},
  }.dump(), websocketpp::frame::opcode::text);
}

void Session::Impl::sendConnect(websocketpp::connection_hdl hdl, std::string username) {
  auto& conn = _connections[username];
  conn.setSendCandidatesCb([this, hdl, username](auto candidates) {
    sendCandidates(hdl, username, candidates);
  });
  conn.setConnectedCb([this, username, &conn]() {
    std::cout << "Connected to " << username << std::endl;
  });
  _ws.send(hdl, json{
    {kTypeKey, kConnectMsg}, 
    {kDstKey, username},
    {kDescriptionKey, conn.description()},
  }.dump(), websocketpp::frame::opcode::text);
}

void Session::Impl::sendCandidates(websocketpp::connection_hdl hdl, std::string username, Connection::Candidates candidates)
{
  _ws.send(hdl, json{
    {kTypeKey, kCandidatesMsg}, 
    {kDstKey, username},
    {kCandidatesKey, candidates},
  }.dump(), websocketpp::frame::opcode::text);
}

void Session::Impl::onMessage(websocketpp::connection_hdl hdl, message_ptr msg) {
  try {
    json obj = json::parse(msg->get_payload());
    std::string type = obj[kTypeKey];
    std::string sender = obj[kSenderKey];

    if (type == kHelloMsg) {
      onHello(hdl, sender);
    } else if (type == kByeMsg) {
      onBye(hdl, sender);
    } else if (type == kConnectMsg) {
      onConnect(hdl, sender, obj[kDescriptionKey]);
    } else if (type == kCandidatesMsg) {
      onCandidates(hdl, sender, obj[kCandidatesKey]);
    } else {
      std::cerr << "Unknown message type " << type << std::endl;
    }
  } catch (std::exception const& ex) {
    std::cerr << "Standard exception raised: " << ex.what() << "\n";
  }
}

void Session::Impl::onHello(websocketpp::connection_hdl hdl, const std::string& username) {
  std::cout << "Hello from " << username << std::endl;
  sendConnect(hdl, username);
}

void Session::Impl::onBye(websocketpp::connection_hdl hdl, const std::string& username) {
  std::cout << "Bye from " << username << std::endl;
  auto con_iter = _connections.find(username);
  if (con_iter == _connections.end()) {
    return;
  }
  con_iter->second.close();
  _connections.erase(con_iter);
}

void Session::Impl::onConnect(websocketpp::connection_hdl hdl, const std::string& username, const std::string& description) {
  std::cout << "Connect from " << username << std::endl;
  if (_connections.find(username) == _connections.end()) {
    sendConnect(hdl, username);
  }
  auto& conn = _connections[username];
  conn.connect(description);
}

void Session::Impl::onCandidates(websocketpp::connection_hdl hdl, std::string username, Connection::Candidates candidates) {
  std::cout << "Candidates from " << username << std::endl;
  auto& conn = _connections[username];
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