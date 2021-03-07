#pragma once

#include "connection.h"

#define ASIO_STANDALONE
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

#define TOCATA_LOCAL

#include <memory>
#include <functional>
#include <unordered_map>

namespace tocata {

class Session
{
public:
    void connect(const std::string& username, const std::string& password);

private:
    typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

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

    std::string join(const std::string& username, const std::string& password);

    void sendHello(websocketpp::connection_hdl hdl);
    void sendConnect(websocketpp::connection_hdl hdl, std::string username);
    void sendCandidates(websocketpp::connection_hdl hdl, std::string username, Connection::Candidates candidates);

    void onMessage(websocketpp::connection_hdl hdl, message_ptr msg);    
    void onHello(websocketpp::connection_hdl hdl, const std::string& username);
    void onBye(websocketpp::connection_hdl hdl, const std::string& username);
    void onConnect(websocketpp::connection_hdl hdl, const std::string& username, const std::string& description);
    void onCandidates(websocketpp::connection_hdl hdl, std::string username, Connection::Candidates candidates);

    ws_client _ws;
    std::string _token;
    std::unordered_map<std::string, Connection> _connections;
};

}