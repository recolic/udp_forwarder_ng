#include <iostream>
#include <rlib/stdio.hpp>
#include <rlib/opt.hpp>
using namespace rlib::literals;

int main(int argc, char **argv) {
    rlib::opt_parser args(argc, argv);
    if(args.getBoolArg("--help", "-h")) {
        rlib::println("Usage: {} -l listenAddr -p listenPort -s serverAddr -P serverPort -lp LPassword -rp R(emote)Password"_rs.format(args.getSelf()));
    }
    auto listenAddr = args.getValueArg("-l");
    auto listenPort = args.getValueArg("-p").as<uint16_t>();
    auto serverAddr = args.getValueArg("-s");
    auto serverPort = args.getValueArg("-P").as<uint16_t>();
    auto lPassword = args.getValueArg("-lp");
    auto rPassword = args.getValueArg("-rp");

    return 0;
}