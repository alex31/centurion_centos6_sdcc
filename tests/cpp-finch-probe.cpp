#include "machine.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

int main(int argc, char** argv)
try {
    const bool late_mount = argc == 6 && std::string_view(argv[5]) == "--late-mount";
    if (argc != 4 && argc != 5 && !late_mount)
        throw std::invalid_argument("Usage: cpp-finch-probe FIRMWARE SPEC PROGRAM [DUMP [--late-mount]]");
    const auto firmware = centurion::Firmware::load(argv[1], argv[2]);
    std::ifstream input(argv[3], std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot open CPU6 program");
    const std::vector<std::uint8_t> program { std::istreambuf_iterator<char>(input), {} };
    if (program.size() <= 0x1a || program[0x17] != 0x79 || program[0x1a] != 0)
        throw std::runtime_error("Expected the CPU6 bare-metal startup before main");

    centurion::Machine machine(firmware);
    using Image = centurion::FinchImage;
    if (late_mount) {
        machine.reset();
        for (unsigned i = 0; i < 107'593; ++i)
            machine.step();
    }
    machine.finch.mount(Image("scratch.ffi", std::vector<std::uint8_t>(2 * Image::platter_size)));
    machine.reset();
    for (unsigned i = 0; i < 10'000 && !machine.cpu.at_instruction_boundary(); ++i)
        machine.step();
    if (!machine.cpu.at_instruction_boundary())
        throw std::runtime_error("Reset did not reach an instruction boundary");

    constexpr auto origin = 0x2000;
    constexpr auto return_address = origin + 0x1a;
    machine.bus.load_ram(origin, program);
    machine.cpu.set_pc(origin);
    for (unsigned i = 0; i < 20'000'000; ++i) {
        if (machine.cpu.at_instruction_boundary() && machine.cpu.pc() == return_address)
            break;
        machine.step();
    }
    if (machine.cpu.pc() != return_address || !machine.cpu.at_instruction_boundary())
        throw std::runtime_error("Finch test did not return");
    if (const auto code = machine.cpu.reg(1); code != 0) {
        std::cerr << "Finch test returned " << std::hex << code << '\n';
        return 1;
    }

    const auto disk = std::span(machine.finch.image()->data);
    constexpr auto track_offset = Image::platter_size + Image::track_size;
    if (std::ranges::find(disk.subspan(track_offset, Image::track_size), 0xa5)
            == disk.subspan(track_offset, Image::track_size).end()
        || std::ranges::any_of(disk.first(track_offset), std::identity {})
        || std::ranges::any_of(disk.subspan(track_offset + Image::track_size), std::identity {}))
        throw std::runtime_error("Finch track markers or neighboring data differ");
    if (argc >= 5) {
        std::ofstream dump(argv[4], std::ios::binary);
        dump.write(reinterpret_cast<const char*>(disk.data()), disk.size());
        if (!dump)
            throw std::runtime_error("Cannot write disk dump");
    }
    std::cout << "PASS: Finch track format, sector read/write, and unchanged neighboring data. Microsteps "
              << machine.cpu.microsteps() << '\n';
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
