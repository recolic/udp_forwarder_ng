//
// Created by recolic on 19-6-9.
//

#ifndef UDP_FORWARDER_NG_FORWARDER_HPP
#define UDP_FORWARDER_NG_FORWARDER_HPP

#include <string>
#include <picosha2.h>
#include <rlib/sys/sio.hpp>
#include <sys/epoll.h>
#include <thread>
#include <Crypto.hpp>
#include <unordered_map>
#include "Config.hpp"
#include "Util.hpp"

using std::string;
using namespace std::literals;

inline void epoll_add_fd(fd_t epollFd, fd_t fd) {
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
inline void epoll_del_fd(fd_t epollFd, fd_t fd) {
    epoll_event event {
        .events = EPOLLIN,
        .data = {
                .fd = fd,
        }
    };
    auto ret1 = epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, &event); // Can be nullptr since linux 2.6.9
    if(ret1 == -1)
        throw std::runtime_error("epoll_ctl failed.");
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

public:
    [[noreturn]] void run() {
        auto listenFd = rlib::quick_listen(listenAddr, listenPort, true);
        rlib_defer([=]{close(listenFd);});

        auto epollFd = epoll_create1(0);
        if(epollFd == -1)
            throw std::runtime_error("Failed to create epoll fd.");
        epoll_add_fd(epollFd, listenFd);

        epoll_event events[MAX_EVENTS];

        char buffer[DGRAM_BUFFER_SIZE];
        // WARN: If you want to modify this program to work for both TCP and UDP, PLEASE use rlib::sockIO::recv instead of fixed buffer.

        // Map from serverSession to clientSession.
        // If I see a packet from client, throw it to server.
        // If I see a packet from server, I have to determine which client to throw it.
        // So I have to record the map between client and server, one-to-one.

        std::unordered_map<clientInfo, fd_t, clientInfoHash> client2server;
        std::unordered_map<fd_t, clientInfo> server2client;
        std::unordered_map<fd_t, size_t> server2wallTime;
        // If connection creation time is less than walltime, the connection timed out.

        auto connForNewClient = [&, this](const clientInfo &info) {
            if(info.isNull()) throw std::runtime_error("Invalid client info");
            auto serverFd = rlib::quick_connect(serverAddr, serverPort, true);
            rlog.verbose("creating new connection... {}", serverFd);
            client2server[info] = serverFd; // May overwrite existing element on server timing out.
            server2client.insert(std::make_pair(serverFd, info));
            server2wallTime.insert(std::make_pair(serverFd, getWallTime()));
            epoll_add_fd(epollFd, serverFd);
            return serverFd;
        };
        auto eraseServerConn = [&](fd_t fd) {
            server2client.erase(fd);
            server2wallTime.erase(fd);
            epoll_del_fd(epollFd, fd);
            close(fd);
        };

        rlog.info("Forwarding server working...");
        rlog.info("Listening {}:{}, with upstream server {}:{}.", listenAddr, listenPort, serverAddr, serverPort);

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
                rlog.debug("woke up fd {}, isCLient={}", recvFd, recvSideIsClientSide);

                try {
                    size_t size;
                    fd_t anotherFd;
                    sockaddr *sendtoAddr = nullptr;
                    socklen_t sendtoAddrLen = 0;
                    clientInfo clientSideInfo;
                    // Recv /////////////////////////////////////////////////////////////////////////////////////
                    if(recvSideIsClientSide) {
                        // Client to Server packet.
                        auto &info = clientSideInfo;
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
                        clientInfo &info = server2client.at(recvFd); // If server not found, drop the msg. (The server may just timed out)
                        sendtoAddr = (sockaddr *)&info.addr;
                        sendtoAddrLen = info.len;
                        anotherFd = listenFd;
                        clientSideInfo = info;
                    }

                    // received raw data.
                    string bufferStr (std::begin(buffer), std::begin(buffer) + size);

                    // Addon: ConnTimeout ///////////////////////////////////////////////////////////////////////////
                    // Recolic: The GFW use deep-packet-inspection to fuck my OpenVPN connection in about 10 minutes.
                    //   What if I change a new connection in every 1 minute?
                    //   Try it.

                    if(bufferStr.size() >= sizeof(uint64_t)) {
                        // Check control msg. Its nonce is zero.
                        if(*(uint64_t*)bufferStr.data() == 0) {
                            if(recvSideIsClientSide) {
                                // ctl msg from client. (conn change req)
                                uint16_t previous_port, new_port;
                                if(bufferStr.size() < sizeof(uint64_t) + 2*sizeof(previous_port))
                                    throw std::runtime_error("ctl msg from client too short.");
                                std::memcpy(&previous_port, bufferStr.data()+sizeof(uint64_t), sizeof(previous_port));
                                std::memcpy(&new_port, bufferStr.data()+sizeof(uint64_t)+sizeof(previous_port), sizeof(previous_port));
                                previous_port = be16toh(previous_port);
                                new_port = be16toh(new_port);

                                // getsockname on UDP ONLY works for the local port. Ignore the addr!
                                auto iter = std::find_if(client2server.begin(), client2server.end(), [previous_port](const auto &kv){
                                    // Known bug 1632: If there's two udp client with different addr and same port. it booms.
                                    return kv.first.getPortNum() == previous_port;
                                });
                                if(iter == client2server.end())
                                    throw std::runtime_error("ctl msg from client: change conn: prev conn not exist.");

                                auto clientSideInfoBackup = clientSideInfo;
                                clientSideInfo.setPortNum(new_port);
                                auto serverFd = iter->second;
                                rlog.debug("Client requested to change conn:", previous_port, new_port);
                                server2client[serverFd].setPortNum(new_port);
                                client2server[clientSideInfo] = serverFd; // Old record is not erased now.

                                // send ACK to client(recvFd). Should use previous port.
                                //   The client will see the ACK from OLD connection and erase it.
                                string ackStr (sizeof(uint64_t), '\0');
                                auto ret = sendto(recvFd, ackStr.data(), ackStr.size(), 0, (sockaddr*)&clientSideInfoBackup.addr, clientSideInfoBackup.len);
                                if(ret == -1)
                                    throw std::runtime_error("Failed to send CONN CHANGE ACK");

                                // remove the ctl prefix
                                bufferStr = bufferStr.substr(sizeof(uint64_t) + 2*sizeof(previous_port));
                            }
                            else {
                                // ACK ctl msg from server (conn change ack)
                                if(bufferStr.size() != sizeof(uint64_t))
                                    throw std::runtime_error("wrong ack ctl from server");
                                rlog.verbose("REMOVEING FD", recvFd);
                                eraseServerConn(recvFd);
                                continue; // nothing todo with bare ACK.
                            }
                        }
                    }

                    // Encrypt/Decrypt ///////////////////////////////////////////////////////////////////////////////
                    crypto.convertL2R(bufferStr, recvSideKey, sendSideKey);
                    // Encrypt/Decrypt End. Continue ConnTimeout Addon ///////////////////////////////////////////////

                    auto prepareConnChangeReq = [&](fd_t prevFd, fd_t newFd) {
                        clientInfo previous, newOne;
                        newOne.len = previous.len = sizeof(previous.addr);
                        auto ret = getsockname(prevFd, (sockaddr*)&previous.addr, &previous.len) +
                                getsockname(newFd, (sockaddr*)&newOne.addr, &newOne.len);
                        if(ret != 0)
                            throw std::runtime_error("getsockname failed.");
                        auto previous_port = htobe16(previous.getPortNum()),
                            new_port = htobe16(newOne.getPortNum());
                        rlog.debug("COnnChangeReq (port num in BIG ENDIAN):", previous_port, new_port);

                        // Add control header.
                        bufferStr = string(sizeof(uint64_t) + sizeof(previous_port)*2, '\0') + bufferStr;
                        std::memcpy((char*)bufferStr.data()+sizeof(uint64_t), &previous_port, sizeof(previous_port));
                        std::memcpy((char*)bufferStr.data()+sizeof(uint64_t)+sizeof(previous_port), &new_port, sizeof(new_port));
                    };

                    if(recvSideIsClientSide && !sendSideKey.empty()) {
                        // Check server connection timeout.
                        // Only timeout the connection if server-side is encrypted. Or OpenVPN server will confuse.
                        // If the connection is timeout:
                        //   1. Create the new connection, reset timeout, update client2server and insert server2client.
                        //   2. Attach the control header onto the origin data packet, send it on the new connection.
                        // When received message from server, if server2client doesn't match client2server, meaning
                        //   that this connection is already timed out.
                        //   If the message is ACK control message, remove the old entry in server2client.
                        //   Otherwise, resend the bare control header in previous step 2.
                        if(server2wallTime.at(anotherFd) < getWallTime()) {
                            // This connection timed out.
                            rlog.verbose("A Connection timed out, creating new conn...");
                            auto newConnFd = connForNewClient(clientSideInfo);
                            prepareConnChangeReq(anotherFd, newConnFd);
                        }
                    }

                    if(!recvSideIsClientSide) {
                        // server to client: I said to change conn but server still using old conn.
                        //   Maybe my req lost. resend.
                        auto clientOwnerFd = client2server.at(server2client.at(recvFd));
                        if(clientOwnerFd != recvFd) {
                            // Client2server already modified, but not receiving ACK.
                            prepareConnChangeReq(recvFd, clientOwnerFd);
                        }
                    }

                    // Send /////////////////////////////////////////////////////////////////////////////////////
                    rlog.debug("sending on fd", anotherFd);
                    if(recvSideIsClientSide) {
                        // Client to Server packet.
                        size = send(anotherFd, bufferStr.data(), bufferStr.size(), 0);
                    }
                    else {
                        // Server to Client packet.
                        size = sendto(anotherFd, bufferStr.data(), bufferStr.size(), 0, sendtoAddr, sendtoAddrLen);
                    }
                    if(size == -1) {
                        throw std::runtime_error("sendto returns -1. "s + strerror(errno));
                    }
                    if(size != bufferStr.size()) {
                        rlog.warning("sendto not sent all data.");
                    }
                    // Done /////////////////////////////////////////////////////////////////////////////////////
                }
                catch(std::exception &e) {
                    rlog.error(e.what());
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
