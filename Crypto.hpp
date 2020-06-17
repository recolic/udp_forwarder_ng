//
// Created by recolic on 19-6-9.
//

#ifndef UDP_FORWARDER_NG_CRYPTO_HPP
#define UDP_FORWARDER_NG_CRYPTO_HPP

#include <string>
#include <numeric>
#include <limits>
#include "Config.hpp"

#if defined(__linux__)
#  include <endian.h>
#include <rlib/stdio.hpp>

#elif defined(__FreeBSD__) || defined(__NetBSD__)
#  include <sys/endian.h>
#elif defined(__OpenBSD__)
#  include <sys/types.h>
#  define be16toh(x) betoh16(x)
#  define be32toh(x) betoh32(x)
#  define be64toh(x) betoh64(x)
#endif

using std::string;

// WARNING: should be thread-safe.
class Crypto {
public:
    Crypto() = default;
    void convertL2R(string &data, const string &lKey, const string &rKey) {
        // If data is empty, maybe it's a bare control msg: a exception maybe thrown.
        // If lKey is not null, decrypt the data.
        // If rKey is not null, encrypt the data.
        if(!lKey.empty()) {
            decrypt(data, lKey);
        }
        if(!rKey.empty()) {
            encrypt(data, rKey);
        }
    }

private:
    static constexpr inline float sq(float a) {return a*a;}
    string uint64ToBinStr(uint64_t n) {
        n = htobe64(n); // to network byte order
        string s (sizeof(uint64_t) / sizeof(char), '\0');
        *(uint64_t *)s.data() = n;
        return s;
    }
    uint64_t uint64FromBinStr(const string &s) {
        auto n = *(uint64_t *)s.data();
        return be64toh(n);
    }
    void encrypt(string &data, const string &key) {
        auto message_len = data.size();
        auto postfix_len = (size_t)(message_len * (sq((float)xorshf_rand.get() / std::numeric_limits<uint64_t>::max())));
        if(postfix_len > crypto_dictionary.size()) {
            postfix_len = 0;
        }
        if(postfix_len + dict_current_index >= crypto_dictionary.size()) {
            dict_current_index = 0;
        }

        string toEncrypt = "r" + uint64ToBinStr(message_len) + data + crypto_dictionary.substr(dict_current_index, postfix_len);

        string nonce = uint64ToBinStr(xorshf_rand.get());
        block_encrypt(toEncrypt, key, nonce);

        data = nonce + toEncrypt;

        // PACKET:
        //   nonce  'r'  msgLen  payload      junk
        //  0     8    9       17       17+len    end
    }
    void block_encrypt(string &data, const string &key, const string &nonce) {
        // nonce is unique for every packet.
        // key is shared by all packets.
        for(auto cter = 0; cter < data.size(); ++cter) {
            const auto key_index = cter % key.size();
            const auto nonce_index = cter % nonce.size();
            data[cter] ^= key[key_index] ^ nonce[nonce_index];
        }
    }
    void decrypt(string &data, const string &key) {
        if(data.size() < 8)
            throw std::runtime_error("Decrypt: Data length less than 8. ");
        string nonce = data.substr(0, 8);
        if(uint64FromBinStr(nonce) == 0)
            throw std::runtime_error("Bad nonce: nonce is zero: ctl msg not fucked.");

        string toDecrypt = data.substr(8);
        block_decrypt(toDecrypt, key, nonce);

        if(toDecrypt.size() < 9)
            throw std::runtime_error("Decrypt: decrypted data length less than 9. ");
        if(toDecrypt[0] != 'r')
            throw std::runtime_error("Decrypt: decrypted magic number incorrect. ");

        uint64_t msgLen = uint64FromBinStr(toDecrypt.substr(1, 9));
        if(toDecrypt.size() < 9+msgLen)
            throw std::runtime_error("Decrypt: decrypted data length < 9+payloadLength");

        data = toDecrypt.substr(9, msgLen);
    }
    void block_decrypt(string &data, const string &key, const string &nonce) {
        return block_encrypt(data, key, nonce);
    }
    size_t dict_current_index = 0;

    struct {
        // This rand is just QUICK and good enough. Maybe still good enough for multi-threading.
        uint64_t x=123456789, y=362436069, z=521288629;
        uint64_t get() {
            uint64_t t;
            x ^= x << 16;
            x ^= x >> 5;
            x ^= x << 1;

            t = x;
            x = y;
            y = z;
            z = t ^ x ^ y;

            if(z != 0) // nonce can not be zero. zero nonce means control message.
                return z;
            else
                return get();
        }
    } xorshf_rand;

};


#endif //UDP_FORWARDER_NG_CRYPTO_HPP
