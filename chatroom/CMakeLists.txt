cmake_minimum_required(VERSION 3.16)
project(chatroom LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(chat_core
  src/common/log.cpp
  src/common/nonblock.cpp
  src/protocol/kv_message.cpp
  src/net/buffer.cpp
  src/net/codec.cpp
  src/base/thread_pool.cpp
  src/app/chat_service.cpp
  src/net/reactor_server.cpp
)

target_include_directories(chat_core PUBLIC src)
target_link_libraries(chat_core PUBLIC pthread)

add_executable(chat_server src/server/main.cpp)
target_link_libraries(chat_server PRIVATE chat_core)

add_executable(chat_client src/client/main.cpp)
target_link_libraries(chat_client PRIVATE chat_core)

add_executable(chat_bench src/bench/chat_bench.cpp)
target_link_libraries(chat_bench PRIVATE chat_core)

enable_testing()

add_executable(codec_test tests/codec_test.cpp)
target_link_libraries(codec_test PRIVATE chat_core)
add_test(NAME codec_test COMMAND codec_test)

add_executable(protocol_test tests/protocol_test.cpp)
target_link_libraries(protocol_test PRIVATE chat_core)
add_test(NAME protocol_test COMMAND protocol_test)

