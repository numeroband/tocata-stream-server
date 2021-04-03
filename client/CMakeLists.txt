cmake_minimum_required(VERSION 3.5)
project(tocata-stream CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# find_package(OpenSSL REQUIRED)

set(NO_SERVER ON CACHE INTERNAL "")
set(NO_TESTS ON CACHE INTERNAL "")
set(OPENSSL_DIR "/Users/lorenzo/openssl/x86_install")
# set(OPENSSL_USE_STATIC_LIBS TRUE CACHE INTERNAL "")

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")

add_subdirectory(libjuice)
add_subdirectory(opus)

# Once the pubsub_client package is found, define new targets.
add_library(tocata-stream INTERFACE)

target_sources(tocata-stream INTERFACE
    ${PROJECT_SOURCE_DIR}/session.cc
    ${PROJECT_SOURCE_DIR}/connection.cc
    ${PROJECT_SOURCE_DIR}/samples_queue.cc
    ${PROJECT_SOURCE_DIR}/codec.cc
    ${PROJECT_SOURCE_DIR}/session.h
    )
  
target_compile_definitions(tocata-stream INTERFACE 
    # TOCATA_LOCAL
    ASIO_STANDALONE
    )

target_include_directories(tocata-stream INTERFACE 
    ${OPENSSL_DIR}/include
    asio/asio/include
    websocketpp
    json/include
    )

target_link_libraries(tocata-stream INTERFACE 
    ${OPENSSL_DIR}/lib/libcrypto.a
    ${OPENSSL_DIR}/lib/libssl.a
    LibJuice::LibJuiceStatic
    Opus::opus
    )

add_library(tocata-stream-framework SHARED)

set_target_properties(tocata-stream-framework PROPERTIES
    FRAMEWORK TRUE
    MACOSX_FRAMEWORK_IDENTIFIER com.tocata.TocataStream
    PUBLIC_HEADER session.h
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "macOS Developer"
    )

target_link_libraries(tocata-stream-framework PRIVATE 
    tocata-stream
    )

add_executable(tocata-stream-test)

target_sources(tocata-stream-test PRIVATE main.cc)

target_link_libraries(tocata-stream-test PRIVATE tocata-stream)

