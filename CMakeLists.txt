cmake_minimum_required(VERSION 3.0)
project(udp_forwarder_ng)
find_package(rlib)
find_package(Threads)

set(CMAKE_CXX_STANDARD 14)

include_directories(./lib)
include_directories(.)

add_executable(udp_forwarder_ng main.cc Forwarder.hpp Crypto.hpp Dictionary.hpp)
target_link_libraries(udp_forwarder_ng r)
target_link_libraries(udp_forwarder_ng Threads::Threads)
add_executable(crypto_test test/TestCrypto.cc Crypto.hpp Dictionary.hpp)
target_link_libraries(crypto_test r)
