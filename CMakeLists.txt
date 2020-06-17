cmake_minimum_required(VERSION 3.0)
project(udp_forwarder_ng)
find_package(rlib)
find_package(Threads)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_DEBUG "-g -DMALLOC_CHECK_=2")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

include_directories(./lib)
include_directories(.)

add_executable(udp_forwarder_ng main.cc Forwarder.hpp Crypto.hpp Config.hpp Util.hpp)
target_link_libraries(udp_forwarder_ng r)
target_link_libraries(udp_forwarder_ng Threads::Threads)
add_executable(crypto_test test/TestCrypto.cc Crypto.hpp Config.hpp)
target_link_libraries(crypto_test r)

install(TARGETS udp_forwarder_ng DESTINATION /usr/bin)
