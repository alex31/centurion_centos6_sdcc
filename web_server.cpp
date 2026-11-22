// C++ port of the CenRE server's browser frontend.
#include "web_server.hpp"
#include "remote_panel.hpp"
#include "runtime.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/json.hpp>

#include <algorithm>
#include <charconv>
#include <deque>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>

namespace centurion
{
namespace
{

namespace beast = boost::beast;
namespace http = beast::http;
namespace ws = beast::websocket;
namespace json = boost::json;
using boost::asio::ip::tcp;
using boost::system::error_code;
using namespace std::chrono_literals;

constexpr std::size_t max_hawk_size = 6'651'904;
using Request = http::request<http::vector_body<std::uint8_t>>;
using Response = http::response<http::vector_body<std::uint8_t>>;

std::string decode(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        const auto c = value[i];
        if (c == '%' && i + 2 < value.size()) {
            const auto first = value.data() + i + 1;
            unsigned byte {};
            const auto [end, error] = std::from_chars(first, first + 2, byte, 16);
            if (error == std::errc {} && end == first + 2) {
                result += static_cast<char>(byte);
                i += 2;
                continue;
            }
        }
        result += c == '+' ? ' ' : c;
    }
    return result;
}

struct Target {
    std::string_view route;
    std::map<std::string, std::string, std::less<>> parameters;

    explicit Target(std::string_view value)
    {
        const auto query = value.find('?');
        route = value.substr(0, query);
        if (query == value.npos)
            return;
        value.remove_prefix(query + 1);
        while (!value.empty()) {
            const auto end = value.find('&');
            const auto parameter = value.substr(0, end);
            const auto equal = parameter.find('=');
            parameters.try_emplace(decode(parameter.substr(0, equal)),
                                   equal == parameter.npos ? "" : decode(parameter.substr(equal + 1)));
            if (end == value.npos)
                break;
            value.remove_prefix(end + 1);
        }
    }

    [[nodiscard]] std::string_view param(std::string_view key) const
    {
        const auto it = parameters.find(key);
        return it == parameters.end() ? std::string_view {} : std::string_view(it->second);
    }
};

std::string_view string_field(const json::object& value, std::string_view key)
{
    const auto* item = value.if_contains(key);
    return item && item->is_string() ? std::string_view(item->as_string()) : std::string_view {};
}

std::string_view speed_name(Runtime::Speed speed)
{
    switch (speed) {
    case Runtime::Speed::Maximum: return "maximum";
    case Runtime::Speed::Realtime: return "realtime";
    case Runtime::Speed::Custom: return "custom";
    }
    throw std::logic_error("Unknown CPU speed");
}

std::string make_snapshot(const Runtime& runtime)
{
    const auto& cpu = runtime.machine.cpu;
    json::array registers;
    for (unsigned index = 0; index < 8; ++index)
        registers.emplace_back(cpu.reg(index));
    json::array disks;
    for (unsigned unit = 0; unit < 2; ++unit) {
        const auto* disk = runtime.machine.hawk.image(unit);
        disks.emplace_back(json::object {
            { "label", "DSK2 Unit " + std::to_string(unit) },
            { "filename", disk ? json::value(disk->name) : json::value(nullptr) },
            { "stride", disk ? json::value(disk->stride) : json::value(nullptr) },
        });
    }
    const auto* finch = runtime.machine.finch.image();
    disks.emplace_back(json::object {
        { "label", "FINCH Unit 0" },
        { "filename", finch ? json::value(finch->name) : json::value(nullptr) },
        { "stride", finch ? json::value(FinchImage::track_size) : json::value(nullptr) },
    });
    json::array terminals;
    for (const auto& terminal : runtime.terminals) {
        const auto& crt = terminal.crt();
        json::array cells;
        cells.reserve(crt.cells().size());
        for (const auto cell : crt.cells())
            cells.emplace_back(cell);
        terminals.emplace_back(json::object {
            { "cells", std::move(cells) },
            { "x", crt.x() },
            { "y", crt.y() },
            { "available", terminal.available() },
            { "bootReady", terminal.boot_ready() },
            { "bootStatus", terminal.boot_status() },
        });
    }
    const auto factor = runtime.speed_factor();
    return json::serialize(json::object {
        { "type", "state" },
        { "running", runtime.running() },
        { "rate", runtime.rate() },
        { "speed", speed_name(runtime.speed()) },
        { "speedFactor", factor ? json::value(*factor) : json::value(nullptr) },
        { "pc", cpu.pc() },
        { "microsteps", cpu.microsteps() },
        { "level", cpu.level() },
        { "map", cpu.map() },
        { "cc", cpu.condition_codes() },
        { "registers", std::move(registers) },
        { "disks", std::move(disks) },
        { "canSave", runtime.machine.hawk.image(1) != nullptr },
        { "terminals", std::move(terminals) },
    });
}

std::string safe_disk_name(std::string_view input)
{
    const auto slash = input.find_last_of("/\\");
    if (slash != input.npos)
        input.remove_prefix(slash + 1);
    std::string name(input.substr(0, 128));
    for (auto& c : name) {
        const auto byte = static_cast<unsigned char>(c);
        if (byte < 32 || byte >= 127)
            c = '_';
    }
    return name.empty() ? "uploaded.img" : name;
}

std::string download_name(std::string_view filename)
{
    if (filename.size() >= 4 && beast::iequals(filename.substr(filename.size() - 4), ".img"))
        filename.remove_suffix(4);
    std::string name(filename);
    for (auto& c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-'
              || c == '_'))
            c = '_';
    }
    return (name.empty() ? "hawk" : name) + "-snapshot.img";
}

using Frame = std::shared_ptr<const std::string>;

class WebClient : public std::enable_shared_from_this<WebClient>
{
public:
    using Handler = std::function<void(WebClient&, std::string_view)>;

    WebClient(beast::tcp_stream stream, Handler handler, bool binary)
        : socket_(std::move(stream))
        , input_(std::move(handler))
    {
        beast::get_lowest_layer(socket_).expires_never();
        socket_.set_option(ws::stream_base::timeout::suggested(beast::role_type::server));
        socket_.read_message_max(32768);
        socket_.binary(binary);
    }

    void accept(Request request, Frame initial)
    {
        auto message = std::make_shared<Request>(std::move(request));
        socket_.async_accept(*message,
                             [self = shared_from_this(), message, initial](const error_code& error) {
                                 if (!error) {
                                     self->ready_ = true;
                                     self->send_state(initial);
                                     self->read();
                                 }
                             });
    }

    void send_state(Frame frame)
    {
        if (!ready_ || !frame)
            return;
        pending_state_ = std::move(frame);
        flush();
    }

    void send_error(std::string_view text)
    {
        send_reply(json::serialize(json::object { { "type", "error" }, { "message", text } }));
    }

    void send_reply(std::string reply)
    {
        if (!ready_ || reply.empty())
            return;
        if (pending_replies_.size() >= 32) {
            close();
            return;
        }
        pending_replies_.emplace_back(std::make_shared<const std::string>(std::move(reply)));
        flush();
    }

private:
    void flush()
    {
        if (sending_ || !ready_)
            return;
        if (pending_replies_.empty()) {
            sending_ = std::exchange(pending_state_, {});
        } else {
            sending_ = std::move(pending_replies_.front());
            pending_replies_.pop_front();
        }
        if (!sending_)
            return;
        socket_.async_write(boost::asio::buffer(*sending_),
                            [self = shared_from_this()](const error_code& error, std::size_t) {
                                self->sending_.reset();
                                if (error)
                                    self->close();
                                else
                                    self->flush();
                            });
    }

    void read()
    {
        socket_.async_read(message_, [self = shared_from_this()](const error_code& error, std::size_t) {
            if (error) {
                self->close();
                return;
            }
            const auto data = self->message_.data();
            self->input_(*self, { static_cast<const char*>(data.data()), data.size() });
            self->message_.consume(data.size());
            if (self->ready_)
                self->read();
        });
    }

    void close()
    {
        ready_ = false;
        error_code ignored;
        beast::get_lowest_layer(socket_).socket().close(ignored);
    }

    ws::stream<beast::tcp_stream> socket_;
    beast::flat_buffer message_;
    Handler input_;
    Frame sending_, pending_state_;
    std::deque<Frame> pending_replies_;
    bool ready_ {};
};

struct HttpPeer : std::enable_shared_from_this<HttpPeer> {
    explicit HttpPeer(tcp::socket socket)
        : stream(std::move(socket))
    {
        // Let one socket completion carry a disk-sized chunk of up to 64 KiB.
        // The default 512-byte reads would alternate with thousands of CPU slices.
        buffer.reserve(64 * 1024);
        parser.body_limit(max_hawk_size);
        stream.expires_after(30s);
    }

    beast::tcp_stream stream;
    beast::flat_buffer buffer;
    http::request_parser<http::vector_body<std::uint8_t>> parser;
};

struct Asset {
    std::string_view type;
    std::vector<std::uint8_t> body;
};

Asset load_asset(const std::filesystem::path& path, std::string_view type)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot open " + path.string());
    return { type, { std::istreambuf_iterator<char>(input), {} } };
}

}

struct WebServer::Impl {
    Impl(boost::asio::io_context& io, Runtime& runtime, const boost::asio::ip::address& bind, unsigned port,
         const std::filesystem::path& root)
        : io(io)
        , runtime(runtime)
        , address(bind)
        , listener(io)
        , refresh(io)
        , assets { { "/", load_asset(root / "index.html", "text/html; charset=utf-8") },
                   { "/console.css", load_asset(root / "console.css", "text/css; charset=utf-8") },
                   { "/console.js", load_asset(root / "console.js", "text/javascript; charset=utf-8") } }
    {
        const tcp::endpoint endpoint(address, port);
        listener.open(endpoint.protocol());
        listener.set_option(tcp::acceptor::reuse_address(true));
        listener.bind(endpoint);
        listener.listen();
        accept();
        tick();
        const auto host = address.is_v6() ? '[' + address.to_string() + ']' : address.to_string();
        std::cout << "[Web console] http://" << host << ':' << port << "/\n";
    }

    void accept()
    {
        listener.async_accept([this](const error_code& error, tcp::socket socket) {
            if (!error) {
                auto peer = std::make_shared<HttpPeer>(std::move(socket));
                http::async_read(peer->stream, peer->buffer, peer->parser,
                                 [this, peer](const error_code& failure, std::size_t) {
                                     if (failure == http::error::body_limit) {
                                         send(peer,
                                              text(peer->parser.get().version(), http::status::bad_request,
                                                   "Disk upload too large"));
                                     } else if (!failure) {
                                         handle(peer, peer->parser.release());
                                     }
                                 });
            }
            if (error != boost::asio::error::operation_aborted)
                accept();
        });
    }

    void tick()
    {
        refresh.expires_after(200ms);
        refresh.async_wait([this](const error_code& error) {
            if (error)
                return;
            broadcast();
            if (runtime.running())
                broadcast_panel();
            tick();
        });
    }

    void broadcast()
    {
        std::erase_if(web_clients, [](const auto& client) { return client.expired(); });
        if (web_clients.empty())
            return;
        const auto frame = std::make_shared<const std::string>(make_snapshot(runtime));
        for (const auto& client : web_clients) {
            if (const auto active = client.lock())
                active->send_state(frame);
        }
    }

    void broadcast_panel()
    {
        std::erase_if(panel_clients, [](const auto& client) { return client.expired(); });
        if (panel_clients.empty())
            return;
        const auto frame = std::make_shared<const std::string>(encode_panel_state(runtime.machine.cpu));
        for (const auto& client : panel_clients) {
            if (const auto active = client.lock())
                active->send_state(frame);
        }
    }

    void message(WebClient& client, std::string_view raw)
    {
        try {
            const auto parsed = json::parse(raw).as_object();
            const auto port = parsed.at("port").as_int64();
            if (port != 0 && port != 1)
                throw std::invalid_argument("Unknown terminal");
            auto& terminal = runtime.terminals[port];
            const auto type = string_field(parsed, "type");
            if (type == "key" && parsed.at("key").is_string()) {
                terminal.key(string_field(parsed, "key"));
            } else if (type == "paste" && parsed.at("text").is_string()) {
                terminal.paste(string_field(parsed, "text"));
            } else if (type == "boot") {
                if (!runtime.machine.hawk.image(1))
                    throw std::runtime_error("Load a CENTOS disk image first");
                terminal.boot(string_field(parsed, "date"), string_field(parsed, "time"));
                runtime.start();
                broadcast();
            } else {
                throw std::invalid_argument("Unknown terminal command");
            }
        } catch (const std::exception& error) {
            client.send_error(error.what());
        }
    }

    bool trusted_host(std::string_view header, const boost::asio::ip::address& local) const
    {
        if (header.empty()
            || header.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.:-[]")
                != header.npos)
            return false;
        std::string_view host;
        std::string_view port;
        if (header.starts_with('[')) {
            const auto bracket = header.find(']');
            if (bracket == header.npos || (bracket + 1 != header.size() && header[bracket + 1] != ':'))
                return false;
            host = header.substr(1, bracket - 1);
            if (bracket + 1 < header.size())
                port = header.substr(bracket + 2);
        } else {
            const auto separator = header.find(':');
            host = header.substr(0, separator);
            if (separator != header.npos)
                port = header.substr(separator + 1);
        }
        if (!port.empty()) {
            unsigned number {};
            const auto [end, error] = std::from_chars(port.data(), port.data() + port.size(), number);
            if (error != std::errc {} || end != port.data() + port.size() || number > 65535)
                return false;
        }
        if (beast::iequals(host, "localhost") || host == "127.0.0.1" || host == "::1")
            return true;
        error_code error;
        const auto parsed = boost::asio::ip::make_address(host, error);
        return !error && (parsed == address || (address.is_unspecified() && parsed == local));
    }

    static bool same_origin(const Request& request)
    {
        const auto host = request[http::field::host];
        const auto origin = request[http::field::origin];
        return origin.starts_with("http://") && origin.substr(7) == host;
    }

    static Response response(unsigned version, http::status status, std::string_view type)
    {
        Response result(status, version);
        result.set(http::field::cache_control, "no-store");
        result.set("X-Content-Type-Options", "nosniff");
        result.set(http::field::content_type, type);
        result.keep_alive(false);
        return result;
    }

    static Response text(unsigned version, http::status status, std::string_view body)
    {
        auto result = response(version, status, "text/plain; charset=utf-8");
        result.body().assign(body.begin(), body.end());
        return result;
    }

    void send(const std::shared_ptr<HttpPeer>& peer, Response response, bool stop = false)
    {
        response.prepare_payload();
        auto message = std::make_shared<Response>(std::move(response));
        http::async_write(peer->stream, *message,
                          [this, peer, message, stop](const error_code&, std::size_t) {
                              error_code ignored;
                              peer->stream.socket().shutdown(tcp::socket::shutdown_send, ignored);
                              if (stop)
                                  io.stop();
                          });
    }

    void handle(const std::shared_ptr<HttpPeer>& peer, Request request)
    {
        const auto version = request.version();
        try {
            const auto host = request[http::field::host];
            error_code endpoint_error;
            const auto local = peer->stream.socket().local_endpoint(endpoint_error);
            if (endpoint_error || !trusted_host(host, local.address())) {
                send(peer, text(version, http::status::forbidden, "Invalid Host"));
                return;
            }
            const Target target(request.target());
            if (ws::is_upgrade(request)) {
                const bool dashboard = target.route == "/console";
                if ((dashboard || !request[http::field::origin].empty()) && !same_origin(request)) {
                    send(peer, text(version, http::status::forbidden, "Invalid Origin"));
                    return;
                }
                Frame initial;
                if (dashboard)
                    initial = std::make_shared<const std::string>(make_snapshot(runtime));
                auto client = std::make_shared<WebClient>(
                    std::move(peer->stream),
                    [this, dashboard](WebClient& client, std::string_view data) {
                        if (dashboard)
                            message(client, data);
                        else
                            client.send_reply(remote_panel_message(runtime, data));
                    },
                    !dashboard);
                (dashboard ? web_clients : panel_clients).emplace_back(client);
                client->accept(std::move(request), std::move(initial));
                return;
            }
            const auto method = request.method();
            if (method == http::verb::get) {
                if (const auto asset = assets.find(target.route); asset != assets.end()) {
                    auto result = response(version, http::status::ok, asset->second.type);
                    result.set("Content-Security-Policy",
                               "default-src 'self'; frame-ancestors 'none'; base-uri 'none'; connect-src "
                               "'self' ws://"
                                   + std::string(host));
                    result.body() = asset->second.body;
                    send(peer, std::move(result));
                    return;
                }
                if (target.route == "/api/disk") {
                    send(peer, download(request));
                    return;
                }
            }
            if (method == http::verb::post && target.route.starts_with("/api/")) {
                if (!same_origin(request)) {
                    send(peer,
                         text(version, http::status::forbidden,
                              "Use the local console page to control the server"));
                    return;
                }
                if (target.route == "/api/disk") {
                    send(peer, upload(std::move(request), target));
                    return;
                }
                if (target.route == "/api/control") {
                    control(peer, version, target);
                    return;
                }
            }
            send(peer, text(version, http::status::not_found, "Not found"));
        } catch (const std::invalid_argument& error) {
            send(peer, text(version, http::status::bad_request, error.what()));
        } catch (const std::exception& error) {
            std::cerr << "[Web console] " << error.what() << '\n';
            send(peer, text(version, http::status::internal_server_error, "Console request failed"));
        }
    }

    Response download(const Request& request)
    {
        const auto version = request.version();
        if (request["sec-fetch-site"] == "cross-site")
            return text(version, http::status::forbidden, "Open the local console to save the image");
        const auto* disk = runtime.machine.hawk.image(1);
        if (!disk)
            return text(version, http::status::not_found, "No Hawk image mounted on DSK2 unit 1");
        auto result = response(version, http::status::ok, "application/octet-stream");
        result.set(http::field::content_disposition,
                   "attachment; filename=\"" + download_name(disk->name) + '"');
        result.body() = disk->data;
        return result;
    }

    Response upload(Request request, const Target& target)
    {
        const auto version = request.version();
        const auto size = request.body().size();
        if (request.chunked() || (size != max_hawk_size && size != 5'196'800))
            return text(version, http::status::bad_request,
                        "Select a 5,196,800 or 6,651,904 byte Hawk image");
        Terminal* boot = nullptr;
        if (target.parameters.contains("boot")) {
            const auto port = target.param("boot");
            if (port != "0" && port != "1")
                return text(version, http::status::bad_request, "Unknown boot terminal");
            Terminal::validate_clock(target.param("date"), target.param("time"));
            boot = &runtime.terminals[port[0] - '0'];
        }
        runtime.machine.hawk.mount(
            1, HawkImage(safe_disk_name(target.param("name")), std::move(request.body())));
        runtime.reset();
        if (boot) {
            boot->boot(target.param("date"), target.param("time"), true);
            runtime.start();
        }
        broadcast();
        return text(version, http::status::ok,
                    boot ? "Image mounted on DSK2 unit 1. Starting CENTOS…" : "Image mounted on DSK2 unit 1");
    }

    void control(const std::shared_ptr<HttpPeer>& peer, unsigned version, const Target& target)
    {
        const auto action = target.param("action");
        if (action == "run") {
            runtime.start();
        } else if (action == "pause") {
            runtime.stop();
        } else if (action == "step") {
            if (runtime.running()) {
                send(peer, text(version, http::status::conflict, "Pause the CPU before stepping"));
                return;
            }
            runtime.step();
        } else if (action == "reset") {
            runtime.reset();
        } else if (action == "speed") {
            const auto mode = target.param("mode");
            if (mode == "realtime")
                runtime.set_speed(Runtime::Speed::Realtime);
            else if (mode == "maximum")
                runtime.set_speed(Runtime::Speed::Maximum);
            else
                throw std::invalid_argument("Unknown speed mode");
        } else if (action == "shutdown") {
            send(peer, text(version, http::status::ok, "Server stopped"), true);
            return;
        } else {
            throw std::invalid_argument("Unknown control command");
        }
        broadcast();
        broadcast_panel();
        send(peer, text(version, http::status::ok, "OK"));
    }

    boost::asio::io_context& io;
    Runtime& runtime;
    boost::asio::ip::address address;
    tcp::acceptor listener;
    boost::asio::steady_timer refresh;
    std::map<std::string_view, Asset> assets;
    std::vector<std::weak_ptr<WebClient>> web_clients, panel_clients;
};

WebServer::WebServer(boost::asio::io_context& io, Runtime& runtime, const boost::asio::ip::address& address,
                     unsigned port, const std::filesystem::path& web_root)
    : impl_(std::make_unique<Impl>(io, runtime, address, port, web_root))
{
}

WebServer::~WebServer() = default;

}
