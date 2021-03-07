cmake_minimum_required(VERSION 3.5)
project(tocata-stream CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

set(NO_SERVER ON CACHE INTERNAL "")
set(NO_TESTS ON CACHE INTERNAL "")
set(OPENSSL_USE_STATIC_LIBS TRUE CACHE INTERNAL "")

add_subdirectory(libjuice)

# Once the pubsub_client package is found, define new targets.
add_executable(tocata-stream 
    main.cc
    session.cc
    connection.cc
    )

target_include_directories(tocata-stream PRIVATE 
    asio/asio/include
    websocketpp
    cpp-httplib
    json/include
    )

target_link_libraries(tocata-stream PRIVATE 
    OpenSSL::SSL
    OpenSSL::Crypto
    LibJuice::LibJuiceStatic
    )
