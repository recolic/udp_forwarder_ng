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
#include <thread>
#include "Crypto.hpp"
#include <unordered_map>

using std::string;
using namespace std::literals;

template <typename MapType, typename ElementType>
bool MapContains(const MapType &map, ElementType &&element) {
    return (map.find(std::forward<ElementType>(element)) != map.cend());
}

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
    static void epoll_add_fd(fd_t epollFd, fd_t fd) {
        epoll_event event {
            .events = EPOLLIN,
            .data = {
                    .fd = fd,
            }
        };
        auto ret1 = epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event);
        if(ret1 == -1)
            throw std::runtime_error("epoll_ctl failed.");
    }

public:
    [[noreturn]] void run() {
        auto listenFd = rlib::quick_listen(listenAddr, listenPort, true);
        rlib_defer([=]{close(listenFd);});

        auto epollFd = epoll_create1(0);
        if(epollFd == -1)
            throw std::runtime_error("Failed to create epoll fd.");
        epoll_add_fd(epollFd, listenFd);

        constexpr size_t MAX_EVENTS = 16;
        epoll_event events[MAX_EVENTS];

        // DGRAM packet usually smaller than 1400B.
        constexpr size_t DGRAM_BUFFER_SIZE = 20480; // 20KiB
        char buffer[DGRAM_BUFFER_SIZE];
        // WARN: If you want to modify this program to work for both TCP and UDP, PLEASE use rlib::sockIO::recv instead of fixed buffer.

        // Map from serverSession to clientSession.
        // If I see a packet from client, throw it to server.
        // If I see a packet from server, I have to determine which client to throw it.
        // So I have to record the map between client and server, one-to-one.
        struct clientInfo {
            sockaddr_storage addr; socklen_t len;
            bool operator==(const clientInfo &another) const {
                return std::memcmp(this, &another, sizeof(clientInfo)) == 0;
            }
        };
        struct clientInfoHash {std::size_t operator()(const clientInfo &info) const {return *(std::size_t*)&info.addr;}}; // hash basing on port number and part of ip (v4/v6) address.
        std::unordered_map<clientInfo, fd_t, clientInfoHash> client2server;
        std::unordered_map<fd_t, clientInfo> server2client;
        auto connForNewClient = [&, this](const clientInfo &info) {
            auto serverFd = rlib::quick_connect(serverAddr, serverPort, true);
            client2server.insert(std::make_pair(info, serverFd));
            server2client.insert(std::make_pair(serverFd, info));
            epoll_add_fd(epollFd, serverFd);
            return serverFd;
        };

        rlib::println("Forwarding server working...");

        // Main loop!
        while(true) {
            auto nfds = epoll_wait(epollFd, events, MAX_EVENTS, -1);
            if(nfds == -1)
                throw std::runtime_error("epoll_wait failed.");

            for(auto cter = 0; cter < nfds; ++cter) {
                const auto recvFd = events[cter].data.fd;
                const auto recvSideIsClientSide = server2client.find(recvFd) == server2client.end(); // is not server
                const auto &recvSideKey = recvSideIsClientSide ? lKey : rKey;
                const auto &sendSideKey = recvSideIsClientSide ? rKey : lKey;

                try {
                    size_t size;
                    fd_t anotherFd;
                    sockaddr *sendtoAddr = nullptr;
                    socklen_t sendtoAddrLen = 0;
                    // Recv
                    if(recvSideIsClientSide) {
                        // Client to Server packet.
                        clientInfo info;
                        size = recvfrom(recvFd, buffer, DGRAM_BUFFER_SIZE, 0, (sockaddr *)&info.addr, &info.len);
                        if(size == -1)
                            throw std::runtime_error("ERR: recvfrom returns -1. "s + strerror(errno));
                        auto pos = client2server.find(info);
                        if(pos == client2server.end())
                            anotherFd = connForNewClient(info);
                        else
                            anotherFd = pos->second;
                    }
                    else {
                        // Server to Client packet.
                        size = recvfrom(recvFd, buffer, DGRAM_BUFFER_SIZE, 0, nullptr, nullptr);
                        if(size == -1)
                            throw std::runtime_error("ERR: recvfrom returns -1. "s + strerror(errno));
                        clientInfo &info = server2client.at(recvFd);
                        sendtoAddr = (sockaddr *)&info.addr;
                        sendtoAddrLen = info.len;
                        anotherFd = listenFd;
                    }

                    // Encrypt/Decrypt
                    string bufferStr (std::begin(buffer), std::begin(buffer) + size);
                    crypto.convertL2R(bufferStr, recvSideKey, sendSideKey);

                    // Send
                    if(recvSideIsClientSide) {
                        // Client to Server packet.
                        size = send(anotherFd, bufferStr.data(), bufferStr.size(), 0);
                    }
                    else {
                        // Server to Client packet.
                        size = sendto(anotherFd, bufferStr.data(), bufferStr.size(), 0, sendtoAddr, sendtoAddrLen);
                    }
                    if(size == -1) {
                        throw std::runtime_error("ERR: sendto returns -1. "s + strerror(errno));
                    }
                    if(size != bufferStr.size()) {
                        rlib::println("WARN: sendto not sent all data.");
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
