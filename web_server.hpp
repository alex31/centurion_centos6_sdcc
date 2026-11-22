// C++ port of the CenRE server's browser frontend.
#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>

#include <filesystem>
#include <memory>

namespace centurion
{

class Runtime;

class WebServer
{
public:
    WebServer(boost::asio::io_context& io, Runtime& runtime, const boost::asio::ip::address& address,
              unsigned port, const std::filesystem::path& web_root);
    ~WebServer();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
