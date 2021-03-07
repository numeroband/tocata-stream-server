#include "session.h"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

namespace tocata {

std::string Session::join(const std::string& username, const std::string& password) {
  httplib::Client http{kHttpServer};
  json req = {
    {kUsernameKey, username},
    {kPasswordKey, password},
  };
  auto res = http.Post("/join", req.dump(), "application/json");
  if (!res || res->status != 201) throw std::runtime_error("Cannot join session");
  return res->body;
}

void Session::connect(const std::string& username, const std::string& password) {
  _token = join(username, password);

  _ws.set_access_channels(websocketpp::log::alevel::none);
  _ws.init_asio();

  _ws.set_open_handler([this](auto h) {
    std::cout << "Connected" << std::endl;
    sendHello(h);
  });
  _ws.set_message_handler(bind(&Session::onMessage, this, ::_1, ::_2));
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

void Session::sendHello(websocketpp::connection_hdl hdl) {
  _ws.send(hdl, json{
    {kTypeKey, kHelloMsg}, 
    {kTokenKey, _token},
  }.dump(), websocketpp::frame::opcode::text);
}

void Session::sendConnect(websocketpp::connection_hdl hdl, std::string username) {
  auto& conn = _connections[username];
  conn.setSendCandidatesCb([this, hdl, username](auto candidates) {
    sendCandidates(hdl, username, candidates);
  });
  conn.setConnectedCb([this, username, &conn]() {
    std::cout << "Connected to " << username << std::endl;
    std::string msg = "Audio from " + _token;
    conn.send(msg.c_str(), msg.length() + 1);
  });
  _ws.send(hdl, json{
    {kTypeKey, kConnectMsg}, 
    {kDstKey, username},
    {kDescriptionKey, conn.description()},
  }.dump(), websocketpp::frame::opcode::text);
}

void Session::sendCandidates(websocketpp::connection_hdl hdl, std::string username, Connection::Candidates candidates)
{
  _ws.send(hdl, json{
    {kTypeKey, kCandidatesMsg}, 
    {kDstKey, username},
    {kCandidatesKey, candidates},
  }.dump(), websocketpp::frame::opcode::text);
}

void Session::onMessage(websocketpp::connection_hdl hdl, message_ptr msg) {
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

void Session::onHello(websocketpp::connection_hdl hdl, const std::string& username) {
  std::cout << "Hello from " << username << std::endl;
  sendConnect(hdl, username);
}

void Session::onBye(websocketpp::connection_hdl hdl, const std::string& username) {
  std::cout << "Bye from " << username << std::endl;
  auto con_iter = _connections.find(username);
  if (con_iter == _connections.end()) {
    return;
  }
  con_iter->second.close();
  _connections.erase(con_iter);
}

void Session::onConnect(websocketpp::connection_hdl hdl, const std::string& username, const std::string& description) {
  std::cout << "Connect from " << username << std::endl;
  if (_connections.find(username) == _connections.end()) {
    sendConnect(hdl, username);
  }
  auto& conn = _connections[username];
  conn.connect(description);
}

void Session::onCandidates(websocketpp::connection_hdl hdl, std::string username, Connection::Candidates candidates) {
  std::cout << "Candidates from " << username << std::endl;
  auto& conn = _connections[username];
  conn.setRemoteCandidates(candidates);
}

}