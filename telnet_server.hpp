// C++ port of the CenRE server's Telnet frontend.
#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>

#include <filesystem>
#include <memory>

namespace centurion
{

class Runtime;

class TelnetServer
{
public:
    TelnetServer(boost::asio::io_context& io, Runtime& runtime, const boost::asio::ip::address& address,
                 unsigned first_port, std::filesystem::path disk_directory);
    ~TelnetServer();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
