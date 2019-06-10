//
// Created by recolic on 19-6-9.
//

#ifndef UDP_FORWARDER_NG_FORWARDER_HPP
#define UDP_FORWARDER_NG_FORWARDER_HPP

#include <string>
#include <picosha2.h>
#include <rlib/sys/sio.hpp>
#include <sys/epoll.h>
#include <rlib/stdio.hpp>
#include "Crypto.hpp"

using std::string;
using namespace std::literals;

class Forwarder {
public:
    Forwarder(string listenAddr, uint16_t listenPort, string serverAddr, uint16_t serverPort, string lPassword,
              string rPassword)
            : listenAddr(listenAddr), listenPort(listenPort), serverAddr(serverAddr), serverPort(serverPort),
            lKey(picosha2::k_digest_size, '\0'), rKey(picosha2::k_digest_size, '\0') {
        picosha2::hash256(lPassword.begin(), lPassword.end(), lKey.begin(), lKey.end());
        picosha2::hash256(rPassword.begin(), rPassword.end(), rKey.begin(), rKey.end());
        if(lPassword.empty())
            lKey = "";
        if(rPassword.empty())
            rKey = "";
    }

private:
    auto setup_epoll(fd_t listenFd, fd_t serverFd) {
        auto epollFd = epoll_create1(0);
        if(epollFd == -1)
            throw std::runtime_error("Failed to create epoll fd.");

        // setup epoll.
        epoll_event eventL {
            .events = EPOLLIN,
            .data = {
                    .fd = listenFd,
            }
        }, eventS {
            .events = EPOLLIN,
            .data = {
                    .fd = serverFd,
            }
        };
        auto ret1 = epoll_ctl(epollFd, EPOLL_CTL_ADD, listenFd, &eventL);
        auto ret2 = epoll_ctl(epollFd, EPOLL_CTL_ADD, serverFd, &eventS);
        if(ret1 == -1 or ret2 == -1)
            throw std::runtime_error("epoll_ctl failed.");
        return epollFd;
    }

public:
    [[noreturn]] void run() {
        // setup connections.
        auto listenFd = rlib::quick_listen(listenAddr, listenPort, true);
        auto serverFd = rlib::quick_connect(serverAddr, serverPort, true);
        auto epollFd = setup_epoll(listenFd, serverFd);

        constexpr size_t MAX_EVENTS = 16;
        epoll_event events[MAX_EVENTS];

        // DGRAM packet usually smaller than 1400B.
        constexpr size_t DGRAM_BUFFER_SIZE = 20480; // 20KiB
        char buffer[DGRAM_BUFFER_SIZE];

        // deal with sockaddr.
        sockaddr_storage listen_sockaddr, server_sockaddr;
        socklen_t listen_socklen = sizeof(listen_sockaddr), server_socklen = sizeof(server_sockaddr);
        auto ret = getsockname(listenFd, (sockaddr *)&listen_sockaddr, &listen_socklen) +
                getsockname(serverFd, (sockaddr *)&server_sockaddr, &server_socklen);
        if(ret != 0)
            throw std::runtime_error("getsockname failed.");

        // Main loop!
        while(true) {
            auto nfds = epoll_wait(epollFd, events, MAX_EVENTS, -1);
            if(nfds == -1)
                throw std::runtime_error("epoll_wait failed.");

            for(auto cter = 0; cter < nfds; ++cter) {
                const auto recvFd = events[cter].data.fd;
                const auto recvSideIsListenSide = recvFd == listenFd;
                const auto anotherFd = recvSideIsListenSide ? serverFd : listenFd;
                const auto &recvSideKey = recvSideIsListenSide ? lKey : rKey;
                const auto &sendSideKey = recvSideIsListenSide ? rKey : lKey;

                try {
                    auto size = recvfrom(recvFd, buffer, DGRAM_BUFFER_SIZE, 0, nullptr, nullptr);
                    if(size == -1) {
                        throw std::runtime_error("ERR: recvfrom returns -1. "s + strerror(errno));
                    }

                    string bufferStr (std::begin(buffer), std::begin(buffer) + size);
                    crypto.convertL2R(bufferStr, recvSideKey, sendSideKey);

                    size = sendto(anotherFd, bufferStr.data(), bufferStr.size(), 0,
                            (sockaddr *)(recvSideIsListenSide ? &server_sockaddr : &listen_sockaddr),
                            recvSideIsListenSide ? server_socklen : listen_socklen);
                    if(size == -1) {
                        throw std::runtime_error("ERR: sendto returns -1. "s + strerror(errno));
                    }
                    if(size != bufferStr.size()) {
                        rlib::println("ERR: sendto not sent all data.");
                    }
                }
                catch(std::exception &e) {
                    rlib::println(e.what());
                }
            }
        }
    }


private:
    string listenAddr;
    uint16_t listenPort;
    string serverAddr;
    uint16_t serverPort;
    string lKey;
    string rKey;
    Crypto crypto;
};


#endif //UDP_FORWARDER_NG_FORWARDER_HPP
