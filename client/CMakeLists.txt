cmake_minimum_required(VERSION 3.5)
project(tocata-stream CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

set(NO_SERVER ON CACHE INTERNAL "")
set(NO_TESTS ON CACHE INTERNAL "")
set(OPENSSL_USE_STATIC_LIBS TRUE CACHE INTERNAL "")

add_subdirectory(libjuice)
add_subdirectory(opus)

# Once the pubsub_client package is found, define new targets.
add_library(tocata-stream SHARED
    session.cc
    connection.cc
    samples_queue.cc
    opus_rtp.cc
    opus_wrapper.cc
    session.h
    )

set_target_properties(tocata-stream PROPERTIES
    FRAMEWORK TRUE
    MACOSX_FRAMEWORK_IDENTIFIER com.tocata.TocataStream
    PUBLIC_HEADER session.h
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "macOS Developer"
  )
  

target_compile_definitions(tocata-stream PRIVATE 
    TOCATA_LOCAL
    ASIO_STANDALONE
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
    Opus::opus
    )

add_executable(tocata-stream-test main.cc)

target_link_libraries(tocata-stream-test PRIVATE 
    tocata-stream
    )

