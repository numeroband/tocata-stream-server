cmake_minimum_required(VERSION 3.5)
project(opus-decoder CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(POST_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/post.js)
set(OPUS_STACK_PROTECTOR OFF)
set(OPUS_HARDENING OFF)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF)
set(OPUS_FORTIFY_SOURCE OFF)

add_subdirectory(opus)

add_executable(opus-decoder)

target_sources(opus-decoder PRIVATE OpusDecoder.cpp)

target_link_libraries(opus-decoder PRIVATE 
    Opus::opus
    )

set_target_properties(opus-decoder PROPERTIES LINK_FLAGS 
    "-s WASM=1 \
    -s NO_DYNAMIC_EXECUTION=1 \
    -s NO_FILESYSTEM=1 \
    -s DEMANGLE_SUPPORT=1 \
    -s MODULARIZE=1 \
    -s EXPORT_NAME=libopusdecoder \
    --bind \
    --post-js '${POST_SCRIPT}'")