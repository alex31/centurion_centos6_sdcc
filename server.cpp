// C++ port of the CenRE emulator server.
#include "images.hpp"
#include "runtime.hpp"
#include "telnet_server.hpp"
#include "web_server.hpp"

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/signal_set.hpp>

#include <charconv>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <format>
#include <iostream>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

namespace
{

using boost::asio::ip::tcp;
using boost::system::error_code;
using centurion::Runtime;

std::string_view setting(const char* name, std::string_view fallback)
{
    const auto* value = std::getenv(name);
    return value && *value ? std::string_view(value) : fallback;
}

unsigned port_number(std::string_view value)
{
    unsigned port {};
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), port);
    if (error != std::errc {} || end != value.data() + value.size() || port == 0 || port > 65535)
        throw std::invalid_argument("Invalid TCP port " + std::string(value));
    return port;
}

boost::asio::ip::address bind_address(boost::asio::io_context& io, std::string_view name)
{
    error_code error;
    const auto address = boost::asio::ip::make_address(name, error);
    if (!error)
        return address;
    const auto resolved = tcp::resolver(io).resolve(name, "0");
    if (resolved.empty())
        throw std::runtime_error("Could not resolve bind address");
    return resolved.begin()->endpoint().address();
}

std::string_view trim(std::string_view value)
{
    constexpr std::string_view space = " \t\r\n";
    const auto first = value.find_first_not_of(space);
    return first == value.npos ? std::string_view {}
                               : value.substr(first, value.find_last_not_of(space) - first + 1);
}

class Console
{
public:
    Console(boost::asio::io_context& io, Runtime& runtime)
        : io_(io)
        , runtime_(runtime)
        , input_(io)
    {
        struct stat metadata {
        };
        if (::fstat(STDIN_FILENO, &metadata) != 0 || (!S_ISFIFO(metadata.st_mode) && !::isatty(STDIN_FILENO)))
            return;
        const int fd = ::dup(STDIN_FILENO);
        if (fd < 0)
            return;
        error_code error;
        input_.assign(fd, error);
        if (error) {
            ::close(fd);
            return;
        }
        read();
    }

private:
    void read()
    {
        boost::asio::async_read_until(input_, boost::asio::dynamic_buffer(buffer_, 8192), '\n',
                                      [this](const error_code& error, std::size_t size) {
                                          if (error)
                                              return;
                                          const auto command
                                              = trim(std::string_view(buffer_).substr(0, size));
                                          try {
                                              execute(command);
                                          } catch (const std::exception& failure) {
                                              std::cerr << failure.what() << '\n';
                                          }
                                          buffer_.erase(0, size);
                                          if (!io_.stopped())
                                              read();
                                      });
    }

    void execute(std::string_view line)
    {
        const auto separator = line.find_first_of(" \t");
        std::string command(line.substr(0, separator));
        for (auto& letter : command)
            letter = std::tolower(static_cast<unsigned char>(letter));
        const auto argument = separator == line.npos ? std::string_view {} : trim(line.substr(separator));
        if (command == "s" || command == "step") {
            runtime_.step();
            std::cout << std::format("  PC: {:04X}\n", runtime_.machine.cpu.pc());
        } else if (command == "r" || command == "run") {
            runtime_.start();
            std::cout << "  Emulation running...\n";
        } else if (command == "h" || command == "halt") {
            runtime_.stop();
            std::cout << "  Emulation halted.\n";
        } else if (command == "reset") {
            runtime_.reset();
            std::cout << "  CPU reset.\n";
        } else if (command == "save" && !argument.empty()) {
            const auto* disk = runtime_.machine.hawk.image(1);
            if (!disk)
                throw std::runtime_error("No disk in DSK2 unit 1");
            centurion::save_disk(argument, disk->data);
            std::cout << "  Disk image saved to " << argument << '\n';
        } else if (command == "q" || command == "quit" || command == "exit") {
            io_.stop();
        } else if (command == "help" || command == "?") {
            std::cout << "  s(tep)  r(un)  h(alt)  reset  save FILE  q(uit)\n";
        } else if (!command.empty()) {
            std::cout << "  Unknown command: " << command << '\n';
        }
        std::cout << std::flush;
    }

    boost::asio::io_context& io_;
    Runtime& runtime_;
    boost::asio::posix::stream_descriptor input_;
    std::string buffer_;
};

}

int main(int argc, char** argv)
try {
    if (argc < 4 || argc > 5)
        throw std::invalid_argument("Usage: centurion-emulator FIRMWARE SPEC WEB-ROOT [HAWK-IMAGE]");
    const auto firmware = centurion::Firmware::load(argv[1], argv[2]);
    boost::asio::io_context io;
    Runtime runtime(io, firmware);
    if (argc == 5) {
        const std::filesystem::path path(argv[4]);
        runtime.machine.hawk.mount(
            1, centurion::HawkImage(path.filename().string(), centurion::read_disk(path)));
    }
    const auto address = bind_address(io, setting("CENTURION_BIND", "127.0.0.1"));
    const auto first_port = port_number(setting("CENTURION_TELNET_BASE", "2323"));
    centurion::TelnetServer telnet(io, runtime, address, first_port, "disks");
    centurion::WebServer web(io, runtime, address, port_number(setting("CENTURION_WEB_PORT", "42646")),
                             argv[3]);

    boost::asio::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait([&](const error_code& error, int) {
        if (!error)
            io.stop();
    });
    Console console(io, runtime);
    std::cout << "Centurion CPU-6 emulator (C++26)\n" << std::flush;
    runtime.start();
    io.run();
} catch (const std::exception& failure) {
    std::cerr << failure.what() << '\n';
    return 1;
}
