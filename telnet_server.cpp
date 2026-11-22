// C++ port of the CenRE server's Telnet frontend.
#include "telnet_server.hpp"
#include "images.hpp"
#include "runtime.hpp"
#include "telnet_codec.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>

#include <algorithm>
#include <cctype>
#include <format>
#include <iostream>
#include <optional>
#include <ranges>

namespace centurion
{
namespace
{

using boost::asio::ip::tcp;
using boost::system::error_code;
using namespace std::chrono_literals;
constexpr std::string_view clear = "\x1b[2J\x1b[H";
constexpr std::string_view cursor_on = "\x1b[?25h";
constexpr std::string_view erase_below = "\x1b[0J";
constexpr std::size_t output_limit = 512 * 1024;

enum class Port { Panel, Crt0, Crt1, Disks, Raw };
constexpr std::array ports { Port::Panel, Port::Crt0, Port::Crt1, Port::Disks, Port::Raw };
constexpr std::array port_names { "Control Panel", "CRT0", "CRT1", "Disk Manager", "Raw Serial 1" };
constexpr std::array disk_names { "DSK2 Unit 0", "DSK2 Unit 1", "FINCH Unit 0" };

class Peer : public std::enable_shared_from_this<Peer>
{
public:
    using Input = std::function<void(Peer&, std::span<const std::uint8_t>)>;
    using Closed = std::function<void()>;

    explicit Peer(tcp::socket socket, Port port)
        : port(port)
        , socket_(std::move(socket))
    {
        error_code error;
        socket_.set_option(tcp::no_delay(true), error);
    }

    void start(Input input, Closed closed = {})
    {
        input_ = std::move(input);
        closed_ = std::move(closed);
        read();
    }

    void send(std::string_view text)
    {
        if (text.empty() || ending_ || !socket_.is_open())
            return;
        if (text.size() > output_limit - pending_.size() - sending_.size()) {
            close();
            return;
        }
        pending_.append(text);
        if (sending_.empty())
            write();
    }

    void finish()
    {
        ending_ = true;
        if (sending_.empty())
            close();
    }

    void close()
    {
        if (!socket_.is_open())
            return;
        const auto keep_alive = shared_from_this();
        error_code error;
        socket_.close(error);
        if (closed_)
            std::exchange(closed_, {})();
    }

    [[nodiscard]] bool connected() const noexcept { return socket_.is_open() && !ending_; }

    Port port;
    TelnetCodec decoder;
    CrtRenderer renderer;
    std::string panel_frame;
    std::vector<std::filesystem::path> selected_disk_files;
    bool help {};
    bool show_microcode {};

private:
    void read()
    {
        socket_.async_read_some(boost::asio::buffer(receiving_),
                                [self = shared_from_this()](const error_code& error, std::size_t size) {
                                    if (error) {
                                        self->close();
                                        return;
                                    }
                                    if (!self->connected())
                                        return;
                                    self->input_(*self, std::span(self->receiving_).first(size));
                                    if (self->connected())
                                        self->read();
                                });
    }

    void write()
    {
        sending_.swap(pending_);
        boost::asio::async_write(socket_, boost::asio::buffer(sending_),
                                 [self = shared_from_this()](const error_code& error, std::size_t) {
                                     self->sending_.clear();
                                     if (error) {
                                         self->close();
                                         return;
                                     }
                                     if (!self->pending_.empty())
                                         self->write();
                                     else if (self->ending_)
                                         self->close();
                                 });
    }

    tcp::socket socket_;
    std::array<std::uint8_t, 4096> receiving_ {};
    std::string pending_, sending_;
    Input input_;
    Closed closed_;
    bool ending_ {};
};

std::string extension(const std::filesystem::path& file)
{
    auto name = file.extension().string();
    std::ranges::transform(name, name.begin(), [](unsigned char c) { return std::tolower(c); });
    return name;
}

std::string safe_text(std::string text)
{
    for (auto& c : text) {
        if (static_cast<unsigned char>(c) < 32 || c == 127)
            c = '?';
    }
    return text;
}

}

struct TelnetServer::Impl {
    Impl(boost::asio::io_context& io, Runtime& runtime, const boost::asio::ip::address& address,
         unsigned first_port, std::filesystem::path directory)
        : runtime(runtime)
        , refresh(io)
        , disk_directory(std::move(directory))
    {
        if (first_port < 1 || first_port > 65531)
            throw std::invalid_argument("Invalid first Telnet port");
        for (unsigned index = 0; index < ports.size(); ++index) {
            const tcp::endpoint endpoint(address, first_port + index);
            auto& listener = listeners[index];
            listener.emplace(io);
            listener->open(endpoint.protocol());
            listener->set_option(tcp::acceptor::reuse_address(true));
            listener->bind(endpoint);
            listener->listen();
        }
        for (unsigned index = 0; index < ports.size(); ++index) {
            accept(index);
            std::cout << '[' << port_names[index] << "] " << address.to_string() << ':' << first_port + index
                      << '\n';
        }
        schedule_refresh();
    }

    void accept(unsigned index)
    {
        listeners[index]->async_accept([this, index](const error_code& error, tcp::socket socket) {
            if (!error)
                attach(std::move(socket), ports[index]);
            if (error != boost::asio::error::operation_aborted)
                accept(index);
        });
    }

    void attach(tcp::socket socket, Port port)
    {
        auto peer = std::make_shared<Peer>(std::move(socket), port);
        if (port == Port::Raw) {
            if (runtime.raw_serial_in_use() || any_peer(Port::Crt1)) {
                peer->close();
                return;
            }
            runtime.claim_raw_serial([weak = std::weak_ptr(peer)](std::uint8_t byte) {
                if (const auto peer = weak.lock()) {
                    const char character = byte;
                    peer->send({ &character, 1 });
                }
            });
            peer->start(
                [this](Peer& channel, std::span<const std::uint8_t> data) {
                    if (!runtime.queue_raw_serial(data))
                        channel.close();
                },
                [this] { runtime.release_raw_serial(); });
            peers.emplace_back(peer);
            return;
        }
        if (port == Port::Crt1 && runtime.raw_serial_in_use()) {
            peer->close();
            return;
        }
        peer->send(TelnetCodec::greeting());
        peer->start([this](Peer& channel, std::span<const std::uint8_t> input) {
            channel.decoder.consume(
                input, [this, &channel](auto key) { handle_key(channel, key); },
                [&channel](auto reply) { channel.send(reply); });
        });
        peers.emplace_back(peer);
        if (port == Port::Disks) {
            disk_menu(*peer);
        } else if (port == Port::Panel) {
            panel(*peer);
        } else {
            peer->send(cursor_on);
            terminal(*peer);
        }
    }

    bool any_peer(Port port)
    {
        return std::ranges::any_of(peers, [port](const auto& weak) {
            const auto peer = weak.lock();
            return peer && peer->connected() && peer->port == port;
        });
    }

    void schedule_refresh()
    {
        refresh.expires_after(20ms);
        refresh.async_wait([this](const error_code& error) {
            if (error)
                return;
            const bool panel_due = ++refresh_count % 5 == 0;
            std::erase_if(peers, [](const auto& weak) { return weak.expired(); });
            for (const auto& weak : peers) {
                if (const auto peer = weak.lock(); peer && peer->connected()) {
                    if (peer->port == Port::Crt0 || peer->port == Port::Crt1)
                        terminal(*peer);
                    else if (peer->port == Port::Panel && panel_due)
                        panel(*peer);
                }
            }
            schedule_refresh();
        });
    }

    void terminal(Peer& peer)
    {
        const auto port = peer.port == Port::Crt0 ? 0 : 1;
        peer.send(peer.renderer.diff(runtime.terminals[port].crt()));
    }

    void handle_key(Peer& peer, std::uint8_t key)
    {
        switch (peer.port) {
        case Port::Crt0:
        case Port::Crt1: runtime.terminals[peer.port == Port::Crt0 ? 0 : 1].telnet_byte(key); break;
        case Port::Panel: panel_key(peer, key); break;
        case Port::Disks: try { disk_key(peer, key);
            } catch (const std::exception& error) {
                peer.send("Error: " + safe_text(error.what()) + "\r\n");
            }
            break;
        case Port::Raw: break;
        }
    }

    void panel_key(Peer& peer, std::uint8_t key)
    {
        auto& cpu = runtime.machine.cpu;
        auto& board = runtime.machine;
        if (key >= '1' && key <= '8') {
            constexpr std::array rates { 1u, 10u, 100u, 1'000u, 10'000u, 50'000u, 100'000u, 150'000u };
            runtime.set_rate(rates[key - '1']);
        }
        const auto command = std::tolower(key);
        peer.help = command == 'h' || command == '?';
        switch (command) {
        case 'r': runtime.running() ? runtime.stop() : runtime.start(); break;
        case 's': runtime.step(); break;
        case 'x':
            runtime.stop();
            runtime.reset();
            break;
        case 'o': cpu.set_sense_switches(cpu.sense_switches() ^ 1); break;
        case 'l':
            cpu.set_sense_switches(cpu.sense_switches() ^ 2);
            if (cpu.sense_switches() & 2)
                runtime.reset();
            break;
        case 'd':
        case 'p': cpu.set_sense_switches(cpu.sense_switches() ^ 4); break;
        case 'f': cpu.set_sense_switches(cpu.sense_switches() ^ 8); break;
        case 'm': peer.show_microcode = !peer.show_microcode; break;
        case 'g': board.bus.insert_diagnostic_roms(!board.bus.diagnostic_roms_installed()); break;
        case '+': board.diagnostic.set_switches(board.diagnostic.switches() + 1); break;
        case '-': board.diagnostic.set_switches(board.diagnostic.switches() - 1); break;
        case 'q':
            peer.send("\x1b[?25hDisconnecting...\r\n");
            peer.finish();
            return;
        }
        panel(peer);
    }

    void panel(Peer& peer)
    {
        const auto& cpu = runtime.machine.cpu;
        std::string screen = "\x1b[H  CENTURION CPU-6  |  Control Panel\r\n\r\n";
        if (peer.help) {
            screen += "R run/halt   S single step   X reset   1-8 set rate   Q disconnect\r\n"
                      "O sense 1   L load/OpSys   D sense 3   F ROM/function\r\n"
                      "M microcode   G diagnostic ROMs   +/- diagnostic switches\r\n";
            screen += std::format("Terminal {} ({} columns by {} rows)\r\n",
                                  peer.decoder.type().empty() ? "unknown" : peer.decoder.type(),
                                  peer.decoder.columns(), peer.decoder.rows());
        } else {
            screen += std::format("  {:7}  PC {:04X}  uPC {:03X}  LEVEL {:X}  MAP {:X}  CC {:02X}\r\n",
                                  runtime.running() ? "Running" : "Halted", cpu.pc(), cpu.micro_pc(),
                                  cpu.level(), cpu.map(), cpu.condition_codes());
            screen += std::format("  ADDR {:05X}  SENSE {:04b}  CYCLES {}  RATE {}\r\n\r\n",
                                  cpu.physical_address(), cpu.sense_switches(), cpu.microsteps(),
                                  runtime.rate());
            constexpr std::string_view registers = "ABXYZSCP";
            for (unsigned index = 0; index < registers.size(); ++index)
                screen += std::format(" {} {:04X}", registers[index], cpu.reg(index));
            screen += "\r\n\r\n";
            if (peer.show_microcode)
                screen += microcode_panel();
            else
                screen += register_panel();
            screen += erase_below;
            screen += std::format("\x1b[{};1H [R]un/halt [S]tep [X]reset [M]icrocode [1-8]rate [?]help [Q]uit",
                                  peer.decoder.rows());
        }
        screen += erase_below;
        if (screen != peer.panel_frame) {
            peer.send(screen);
            peer.panel_frame = std::move(screen);
        }
    }

    std::string register_panel() const
    {
        std::string screen = "  REGISTERS BY INTERRUPT LEVEL\r\n";
        const auto& ram = runtime.machine.cpu.register_ram();
        for (unsigned level = 0; level < 16; ++level) {
            screen += std::format("  {:X} ", level);
            for (unsigned index = 0; index < 16; index += 2)
                screen
                    += std::format(" {:04X}", (ram[level * 16 + index] << 8) | ram[level * 16 + index + 1]);
            screen += "\r\n";
        }
        return screen;
    }

    std::string microcode_panel() const
    {
        const auto& machine = runtime.machine;
        const auto& cpu = machine.cpu;
        const auto state = cpu.micro_state();
        std::string screen = "  MICROCODE STATE\r\n";
        screen += std::format("  SEQ P:{:03X} O:{:03X} H:{:03X}  SP:{:X}  STK ", state.sequence_pointer,
                              cpu.micro_pc(), state.sequence_hold, state.sequence_stack_position);
        for (const auto address : state.sequence_stack)
            screen += std::format(" {:03X}", address);
        screen += "\r\n  MICROINSTRUCTION ";
        for (const auto byte : cpu.microinstruction() | std::views::reverse)
            screen += std::format("{:02X} ", byte);
        screen += std::format("\r\n  ALU Q:{:02X} FLAGS:{:02X}  RAM ", state.alu_q, state.alu_flags);
        for (const auto byte : state.alu_registers)
            screen += std::format("{:02X} ", byte);
        screen += std::format("\r\n  WORK {:04X}  RES {:02X}  RIR {:02X}  RDR {:02X}  SWAP {:02X}\r\n",
                              state.work_address, state.result, state.register_index, state.register_data,
                              state.nibble_swap);
        screen += std::format("  BUS {:02X}  SYS {:02X}  PAGE {:02X}  DIN {:02X}  DOUT {:02X}{}\r\n",
                              state.bus_control, state.system_control, state.page_entry, state.memory_data_in,
                              state.memory_data_out, cpu.parity_fault() ? "  PARITY FAULT" : "");
        const auto& diag = machine.diagnostic;
        screen += std::format("  DIAG ROMs {}  DIP {:X}  LED {:02X}{}  POINTS {:X}\r\n",
                              machine.bus.diagnostic_roms_installed() ? "ON" : "OFF", diag.switches(),
                              diag.display(), diag.blank() ? " (blank)" : "", diag.points());
        return screen;
    }

    void disk_menu(Peer& peer, bool detailed = false)
    {
        peer.selected_disk_files.clear();
        std::string text(clear);
        text += "  CENTURION CPU-6  |  Disk Management\r\n\r\n";
        const auto describe = [&](unsigned disk, const auto* image, unsigned stride) {
            text += std::format("  [{}] {:12}  ", disk, disk_names[disk]);
            if (image) {
                text += safe_text(image->name);
                if (detailed)
                    text += std::format("  {} bytes  {} blocks  {} stride  {}",
                                        image->data.size(), image->data.size() / stride, stride,
                                        image->write_protected ? "WP" : "RW");
            } else {
                text += "(empty)";
            }
            text += "\r\n";
        };
        for (unsigned unit = 0; unit < 2; ++unit) {
            const auto* image = runtime.machine.hawk.image(unit);
            describe(unit, image, image ? image->stride : 0);
        }
        describe(2, runtime.machine.finch.image(), FinchImage::track_size);
        text += "\r\n  [L]oad  [N]ew  [I]nfo  [U]nmount  [M]enu  [Q]uit\r\n";
        peer.send(text);
    }

    void disk_files(Peer& peer)
    {
        std::filesystem::create_directories(disk_directory);
        peer.selected_disk_files.clear();
        for (const auto& entry : std::filesystem::directory_iterator(disk_directory)) {
            if (entry.is_regular_file()
                && (extension(entry.path()) == ".img" || extension(entry.path()) == ".ffi"))
                peer.selected_disk_files.push_back(entry.path());
        }
        std::ranges::sort(peer.selected_disk_files);
    }

    void mount_disk(Peer& peer, unsigned index)
    {
        if (peer.selected_disk_files.empty())
            disk_files(peer);
        if (index >= peer.selected_disk_files.size())
            throw std::out_of_range("Invalid disk index");
        const auto& file = peer.selected_disk_files[index];
        const auto name = file.filename().string();
        unsigned slot;
        if (extension(file) == ".ffi") {
            slot = 2;
            if (runtime.machine.finch.image())
                throw std::runtime_error("No compatible empty drive available");
            runtime.machine.finch.mount(FinchImage(name, read_disk(file)));
        } else {
            auto& hawk = runtime.machine.hawk;
            slot = hawk.image(1) ? 0 : 1;
            if (hawk.image(slot))
                throw std::runtime_error("No compatible empty drive available");
            hawk.mount(slot, HawkImage(name, read_disk(file)));
        }
        peer.send(std::format("Mounted {} to {}\r\n", safe_text(name), disk_names[slot]));
        peer.selected_disk_files.clear();
    }

    void disk_key(Peer& peer, std::uint8_t key)
    {
        if (key >= '0' && key <= '7') {
            mount_disk(peer, key - '0');
            return;
        }
        switch (std::tolower(key)) {
        case 'm': disk_menu(peer); break;
        case 'i': disk_menu(peer, true); break;
        case 'l': {
            disk_files(peer);
            std::string text(clear);
            text += "  LOAD DISK IMAGE\r\n\r\n";
            for (unsigned i = 0; i < std::min<std::size_t>(8, peer.selected_disk_files.size()); ++i)
                text += std::format("  [{}] {}\r\n", i,
                                    safe_text(peer.selected_disk_files[i].filename().string()));
            if (peer.selected_disk_files.empty())
                text += "  (no disk images found)\r\n";
            peer.send(text + "\r\n  [M] Back to menu\r\n");
            break;
        }
        case 'n':
            peer.send("  Hawk 400- and 512-byte stride, Finch 1–4 platters.\r\n"
                      "  Create a disk image in the disks directory and select [L]oad.\r\n");
            break;
        case 'u':
            for (unsigned unit = 0; unit < 2; ++unit)
                runtime.machine.hawk.eject(unit);
            runtime.machine.finch.eject();
            disk_menu(peer);
            break;
        case 'q':
            peer.send("\x1b[?25hDisconnecting...\r\n");
            peer.finish();
            break;
        }
    }

    Runtime& runtime;
    boost::asio::steady_timer refresh;
    std::filesystem::path disk_directory;
    std::array<std::optional<tcp::acceptor>, ports.size()> listeners;
    std::vector<std::weak_ptr<Peer>> peers;
    unsigned refresh_count {};
};

TelnetServer::TelnetServer(boost::asio::io_context& io, Runtime& runtime,
                           const boost::asio::ip::address& address, unsigned first_port,
                           std::filesystem::path disk_directory)
    : impl_(std::make_unique<Impl>(io, runtime, address, first_port, std::move(disk_directory)))
{
}

TelnetServer::~TelnetServer() = default;

}
