//
// Created by recolic on 19-6-10.
//

#include <Crypto.hpp>
#include <rlib/stdio.hpp>
#include <vector>

Crypto crypto;

void test(std::string c, std::string l, std::string r) {
    auto backup = c;
    crypto.convertL2R(c, "", l);
    rlib::println("ORIGIN len", backup.size(), ", 1ORDER-ENC len", c.size());
    crypto.convertL2R(c, l, r);
    crypto.convertL2R(c, r, l);
    crypto.convertL2R(c, l, "");
    if(backup != c) {
        throw std::runtime_error("Failed testcase:" + backup + ":" + l + ":" + r + ":GOT:" + c);
    }
}

int main() {
    test("v99x0cv0zxcv", "1", "");
    test("v99x0cv0zxcv", "1", "");
    test("v99x0cv0zxcv", "1", "");
    test("v99x0cv0zxcv", "1", "");
    test("v99x0cv0zxcv", "1", "");
    test("v99x0cv0zxcv", "1", "");


    std::string k1 = "1", k2 = "", k3 = "12989d8vh9xuncv'sd;", k4 = std::string("sdvpn0923\0t;,ew',;ds") + '\0' + '2';
    std::vector<std::string> keys {k1, k2, k3, k4};

    std::vector<std::string> tests {"v99x0cv0zxcv", "", "1", std::string("1290urj3j90nxv09snd\0goi\0oweinfge") + '\0' + '2', crypto_dictionary};

    for(auto t : tests) {
        for(auto l : keys) {
            for(auto r : keys) {
                test(t, l, r);
            }
        }
    }


}
