#include "machine.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{

constexpr unsigned slice = 100'000;

void wait_for(centurion::Machine& machine, const std::string& output, std::string_view marker,
              std::size_t start = 0, unsigned budget = 2400)
{
    for (unsigned i = 0; i < budget; ++i) {
        if (output.find(marker, start) != std::string::npos)
            return;
        machine.run_slice(slice);
    }
    throw std::runtime_error("Missing prompt: " + std::string(marker)
                             + ", PC=" + std::to_string(machine.cpu.pc()));
}

void send(centurion::Machine& machine, std::string_view input)
{
    for (const auto ch : input) {
        for (unsigned i = 0; i < 200 && !machine.mux.can_receive(0); ++i)
            machine.run_slice(slice);
        if (!machine.mux.can_receive(0))
            throw std::runtime_error("MUX input remained busy");
        machine.mux.receive(0, ch);
    }
}

void run_job(centurion::Machine& machine, const std::string& output, std::string_view command,
             std::string_view marker)
{
    const auto start = output.size();
    const auto started = std::chrono::steady_clock::now();
    const auto initial_cycles = machine.cpu.microsteps();
    send(machine, command);
    send(machine, "\r");
    wait_for(machine, output, marker, start, 12000);
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - started;
    std::cout << "\n@@BEGIN " << command << '\n'
              << output.substr(start) << '\n'
              << "@@END " << machine.cpu.microsteps() - initial_cycles << ' ' << elapsed.count() << '\n';
}

}

int main(int argc, char** argv)
try {
    const bool applications = argc == 5 && std::string_view(argv[4]) == "--apps";
    if (argc != 4 && !applications)
        throw std::invalid_argument("Usage: cpp-hawk-boot FIRMWARE SPEC IMAGE [--apps]");
    const auto firmware = centurion::Firmware::load(argv[1], argv[2]);
    std::ifstream disk(argv[3], std::ios::binary);
    if (!disk)
        throw std::runtime_error("Cannot open CENTOS image");

    centurion::Machine machine(firmware);
    machine.hawk.mount(1, centurion::HawkImage(argv[3], { std::istreambuf_iterator<char>(disk), {} }));
    std::string output;
    machine.mux.observe_writes(0, [&](std::uint8_t byte) {
        const char ch = byte & 127;
        output.push_back(ch);
        if (!applications)
            std::cout << ch << std::flush;
    });
    machine.reset();

    wait_for(machine, output, "D=");
    send(machine, "H1");
    wait_for(machine, output, "SYSTEM DISK (S)= 1");
    send(machine, "\r");
    wait_for(machine, output, "ENTER NEW SYSTEM DATE");
    send(machine, "112226\r");
    wait_for(machine, output, "ENTER SYSTEM TIME");
    send(machine, "120000\r");
    wait_for(machine, output, "CRT0 READY");

    if (applications) {
        run_job(machine, output, ".RUN SDCC.XSIEVE", "C_SIEVE;");
        run_job(machine, output, "RUNTESTS", "FOR SYSRDR");
        return 0;
    }
    const auto before = output.size();
    send(machine, ".DIR 1 (BASIC)\r");
    wait_for(machine, output, "AVAILABLE:", before);
    if (output.find("BASIC      A", before) == std::string::npos)
        throw std::runtime_error("Expected BASIC in the CENTOS application image");
    std::cout << "\nPASS: native Hawk boot and CENTOS directory. Microsteps " << machine.cpu.microsteps()
              << '\n';
} catch (const std::exception& error) {
    std::cerr << '\n' << error.what() << '\n';
    return 1;
}
