#include <iostream>
#include <rlib/stdio.hpp>
#include <rlib/opt.hpp>
#include "Forwarder.hpp"

rlib::logger rlog(std::cerr);

using namespace rlib::literals;

int main(int argc, char **argv) {
    rlib::opt_parser args(argc, argv);
    if(args.getBoolArg("--help", "-h")) {
        rlib::println("Usage: {} -l listenAddr -p listenPort -s serverAddr -P serverPort [-lp LPassword] [-rp R(emote)Password] [--log=error/info/verbose/debug]"_rs.format(args.getSelf()));
        rlib::println("Leave LPassword for empty if listenAddr is other UDP application.");
        rlib::println("Leave RPassword for empty if serverAddr is other UDP application.");
        return 0;
    }
    auto listenAddr = args.getValueArg("-l");
    auto listenPort = args.getValueArg("-p").as<uint16_t>();
    auto serverAddr = args.getValueArg("-s");
    auto serverPort = args.getValueArg("-P").as<uint16_t>();
    auto lPassword = args.getValueArg("-lp", false, "");
    auto rPassword = args.getValueArg("-rp", false, "");

    auto log_level = args.getValueArg("--log", false, "info");
    if(log_level == "error")
        rlog.set_log_level(rlib::log_level_t::ERROR);
    else if(log_level == "info")
        rlog.set_log_level(rlib::log_level_t::INFO);
    else if(log_level == "verbose")
        rlog.set_log_level(rlib::log_level_t::VERBOSE);
    else if(log_level == "debug")
        rlog.set_log_level(rlib::log_level_t::DEBUG);
    else
        throw std::runtime_error("Unknown log level: " + log_level);

    Forwarder fwd(listenAddr, listenPort, serverAddr, serverPort, lPassword, rPassword);
    fwd.run();

    return 0;
}