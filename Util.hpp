//
// Created by recolic on 19-6-10.
//

#ifndef UDP_FORWARDER_NG_CONNECTIONTIMEOUTCTL_HPP
#define UDP_FORWARDER_NG_CONNECTIONTIMEOUTCTL_HPP


#include <cstddef>
#include <rlib/sys/fd.hpp>
#include <ctime>
#include <rlib/log.hpp>

extern rlib::logger rlog;

inline size_t getWallTime() {
    auto seconds = static_cast<size_t>(std::time(nullptr));
    return seconds / SERVER_ENCRYPT_CONNECTION_TIMEOUT_SECONDS;
}


inline char nibbleToHex(int nibble)
{
    const int ascii_zero = 48;
    const int ascii_a = 65;

    if((nibble >= 0) && (nibble <= 9))
    {
        return (char) (nibble + ascii_zero);
    }
    if((nibble >= 10) && (nibble <= 15))
    {
        return (char) (nibble - 10 + ascii_a);
    }
    return '?';
}
inline string char2str(char byteVal)
{
    int upp = (byteVal & 0xF0) >> 4;
    int low = (byteVal & 0x0F);
    return std::string() + nibbleToHex(upp) + nibbleToHex(low);
}

template <typename T>
inline string printBinaryObj(const T &obj) {
    string res = "[";
    const char *p = (const char *)&obj;
    for(auto i = 0; i < sizeof(T); ++i) {
        res += char2str(p[i]) + ' ';
    }
    return res + "]";
}

struct clientInfo {
    sockaddr_storage addr; socklen_t len;
    bool operator==(const clientInfo &another) const {
        return do_compare(another, true);
    }
    bool matchWildcard(const clientInfo &another) const {
        return do_compare(another, false);
    }
    bool do_compare(const clientInfo &another, bool need_same_addr = true) const {
        const auto *me = (const sockaddr_in*)&this->addr;
        const auto *an = (const sockaddr_in*)&another.addr;

        if(me->sin_family != an->sin_family)
            return false;
        if(me->sin_family == AF_INET) {
            auto same_addr = std::memcmp(&me->sin_addr, &an->sin_addr, sizeof(me->sin_addr)) == 0;
            auto same_port = me->sin_port == an->sin_port;
            return need_same_addr ? same_addr && same_port : same_port;
        }
        else if(me->sin_family == AF_INET6) {
            const auto *me = (const sockaddr_in6*)&this->addr;
            const auto *an = (const sockaddr_in6*)&another.addr;
            auto same_addr = std::memcmp(&me->sin6_addr, &an->sin6_addr, sizeof(me->sin6_addr)) == 0;
            auto same_port = me->sin6_port == an->sin6_port;
            return need_same_addr ? same_addr && same_port : same_port;
        }
        else
            throw std::runtime_error("Invalid client info: sin_family is not AF_INET or AF_INET6");
    }
    bool isNull() const {
        for(auto cter = 0; cter < sizeof(addr); ++cter) {
            if(cter[(char *)&addr] != 0)
                return false;
        }
        return true;
    }
    uint16_t getPortNum() const {
        const auto *me = (const sockaddr_in*)&this->addr;
        if(me->sin_family == AF_INET) {
            return me->sin_port;

        }
        else if(me->sin_family == AF_INET6) {
            const auto *me = (const sockaddr_in6*)&this->addr;
            return me->sin6_port;
        }
        else
            throw std::runtime_error("Invalid client info: sin_family is not AF_INET or AF_INET6: got " + std::to_string(me->sin_family));
    }
    void setPortNum(uint16_t port) {
        auto *me = (sockaddr_in*)&this->addr;
        if(me->sin_family == AF_INET) {
            me->sin_port = port;
        }
        else if(me->sin_family == AF_INET6) {
            auto *me = (sockaddr_in6*)&this->addr;
            me->sin6_port = port;
        }
        else
            throw std::runtime_error("Invalid client info: sin_family is not AF_INET or AF_INET6: got " + std::to_string(me->sin_family));
    }
};
struct clientInfoHash {std::size_t operator()(const clientInfo &info) const {return *(std::size_t*)&info.addr;}}; // hash basing on port number and part of ip (v4/v6) address.

//// Change connection to encrypted server in every 1 minute to avoid GFW deep-packet-inspect.
//class ConnectionTimeoutCtl {
//public:
//    ConnectionTimeoutCtl(size_t timeoutSeconds, bool serverSideIsEncrypted)
//    : timeoutSeconds(timeoutSeconds), serverSideIsEncrypted(serverSideIsEncrypted) {
//        // If server side is unencrypted, set timeout to +inf.
//    }
//
//    bool encryptedMessageIsControlMessage() {
//        // Client side operation.
//        // 1. Return true if the message is control message (nonce=0)
//        // 2. if the message is control msg, deal with it.
//    }
//
//    fd_t shouldChangeConnection() {
//        // Server side operation.
//        // 1. Return new fd ONLY if the connection should time out AND the new connection is ready.
//        //      else return -1.
//        // 2. This function should send the control message, and update the client2server map.
//    }
//
//



//private:
//    size_t timeoutSeconds;
//    bool serverSideIsEncrypted;
//};


#endif //UDP_FORWARDER_NG_CONNECTIONTIMEOUTCTL_HPP
