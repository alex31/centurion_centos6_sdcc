#include "cpu6.hpp"
#include "mux.hpp"
#include "pacer.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

namespace
{

class ResetLine final : public centurion::Device
{
public:
    explicit ResetLine(centurion::Cpu6& cpu)
        : cpu_(cpu)
    {
    }
    std::uint8_t read(unsigned) override { return 0; }
    void write(unsigned, std::uint8_t) override { }
    void tick(unsigned) override
    {
        if (cpu_.microsteps() == edge)
            cpu_.reset();
    }
    std::uint64_t edge {};

private:
    centurion::Cpu6& cpu_;
};

std::uint64_t positive_integer(std::string_view input)
{
    std::uint64_t number {};
    const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), number);
    if (error != std::errc {} || end != input.data() + input.size() || number == 0)
        throw std::invalid_argument("Expected a positive integer");
    return number;
}

template <class Range> std::uint32_t hash(const Range& bytes)
{
    std::uint32_t result = 2166136261;
    for (const auto byte : bytes)
        result = (result ^ byte) * 16777619u;
    return result;
}

void state(const centurion::Cpu6& cpu, const centurion::Backplane& bus, bool trace)
{
    std::cout << "state " << cpu.microsteps() << ' ' << cpu.pc() << ' ' << cpu.at_instruction_boundary()
              << ' ' << cpu.level() << ' ' << cpu.map() << ' ' << cpu.condition_codes() << ' '
              << cpu.memory_address() << ' ' << cpu.physical_address() << ' ' << hash(cpu.register_ram())
              << ' ' << hash(cpu.page_ram());
    if (!trace)
        std::cout << ' ' << hash(bus.ram());
    std::cout << '\n';
}

// Exercise both single-step and threaded batches, including budget boundaries.
void verify_batches(centurion::Cpu6& cpu, centurion::Backplane& bus, const centurion::Firmware& firmware,
                    std::span<const std::uint8_t> code, std::uint64_t cycles)
{
    centurion::Backplane reference_bus(firmware);
    centurion::Mux mux;
    reference_bus.attach(mux, 0x3f200, 0x100, centurion::Backplane::Irq::Wired);
    centurion::Cpu6 reference(reference_bus, firmware);
    reference.reset();
    while (reference.microsteps() < cpu.microsteps())
        reference.step();
    reference_bus.load_ram(0x2000, code);
    reference.set_pc(0x2000);

    constexpr std::array<unsigned, 8> batches { 0, 1, 2, 3, 17, 100, 257, 4096 };
    std::uint64_t elapsed = 0;
    unsigned batch = 0;
    while (elapsed < cycles) {
        auto count = elapsed < 10'000 ? 1u : batches[batch++ % batches.size()];
        count = std::min<std::uint64_t>(cycles - elapsed, count);
        cpu.run(count);
        for (unsigned i = 0; i < count; ++i)
            reference.step();
        elapsed += count;
        if (cpu.microsteps() != reference.microsteps() || cpu.micro_pc() != reference.micro_pc()
            || cpu.micro_state() != reference.micro_state() || cpu.pc() != reference.pc()
            || cpu.physical_address() != reference.physical_address() || cpu.map() != reference.map()
            || cpu.condition_codes() != reference.condition_codes() || cpu.level() != reference.level()
            || cpu.memory_address() != reference.memory_address()
            || cpu.parity_fault() != reference.parity_fault()
            || cpu.dma_finished() != reference.dma_finished()
            || cpu.register_ram() != reference.register_ram() || cpu.page_ram() != reference.page_ram())
            throw std::runtime_error("Generic and dispatched CPU states differ at microcycle "
                                     + std::to_string(elapsed));
    }
    if (!std::ranges::equal(bus.ram(), reference_bus.ram()))
        throw std::runtime_error("Generic and dispatched RAM differ");

    // A hardware callback may reset the latches on the last edge before a
    // direct tail jump. The next handler must honor the reset micro-address.
    ResetLine line(cpu), reference_line(reference);
    bus.clock_every_microstep(line);
    reference_bus.clock_every_microstep(reference_line);
    for (unsigned phase = 1; phase <= 64; ++phase) {
        line.edge = reference_line.edge = cpu.microsteps() + phase;
        cpu.run(phase + 1);
        for (unsigned i = 0; i <= phase; ++i)
            reference.step();
        if (cpu.micro_pc() != reference.micro_pc() || cpu.micro_state() != reference.micro_state())
            throw std::runtime_error("CPU reset on a peripheral clock edge diverged");
    }
    std::cout << "PASS: microsequencer, ALU, registers, bus state and RAM match the generic core\n";
}

}

int main(int argc, char** argv)
try {
    if (argc != 6)
        throw std::invalid_argument(
            "Usage: cpu6-probe FIRMWARE SPEC PROGRAM CYCLES trace|bench|realtime|verify");
    const auto firmware = centurion::Firmware::load(argv[1], argv[2]);
    const auto cycles = positive_integer(argv[4]);
    const std::string_view mode(argv[5]);
    if (mode != "trace" && mode != "bench" && mode != "realtime" && mode != "verify")
        throw std::invalid_argument("Expected trace, bench, realtime or verify");
    const bool trace = mode == "trace";
    std::ifstream program(argv[3], std::ios::binary);
    if (!program)
        throw std::runtime_error("Cannot open CPU6 program");
    const std::vector<std::uint8_t> code { std::istreambuf_iterator<char>(program), {} };

    centurion::Backplane bus(firmware);
    centurion::Mux mux;
    bus.attach(mux, 0x3f200, 0x100, centurion::Backplane::Irq::Wired);
    centurion::Cpu6 cpu(bus, firmware);
    std::cout << "engine " << (cpu.uses_compiled_microcode() ? "compiled" : "generic") << '\n';
    cpu.reset();
    for (unsigned i = 0; i < 10000 && !cpu.at_instruction_boundary(); ++i) {
        cpu.run(1);
        if (trace)
            state(cpu, bus, true);
    }
    if (!cpu.at_instruction_boundary())
        throw std::runtime_error("Reset did not reach an instruction boundary");
    bus.load_ram(0x2000, code);
    cpu.set_pc(0x2000);

    if (mode == "verify") {
        verify_batches(cpu, bus, firmware, code, cycles);
    } else if (trace) {
        for (std::uint64_t cycle = 0; cycle < cycles; ++cycle) {
            cpu.run(1);
            state(cpu, bus, true);
        }
    } else if (mode == "bench") {
        const auto begin = std::chrono::steady_clock::now();
        cpu.run(cycles);
        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - begin;
        std::cout << "seconds " << elapsed.count() << '\n';
        state(cpu, bus, false);
    } else {
        using Clock = centurion::Pacer::Clock;
        const auto begin = Clock::now();
        centurion::Pacer pacer(begin);
        const auto cpu_start = std::clock();
        std::vector<double> wake_latency;
        auto remaining = cycles;
        while (remaining) {
            std::this_thread::sleep_until(pacer.deadline());
            const auto now = Clock::now();
            wake_latency.push_back(std::chrono::duration<double, std::milli>(now - pacer.deadline()).count());
            const auto count = std::min<std::uint64_t>(pacer.cycles_due(now), remaining);
            cpu.run(count);
            remaining -= count;
        }
        const std::chrono::duration<double> elapsed = Clock::now() - begin;
        const double cpu_seconds = static_cast<double>(std::clock() - cpu_start) / CLOCKS_PER_SEC;
        std::ranges::sort(wake_latency);
        const auto p95 = wake_latency[(wake_latency.size() * 95 + 99) / 100 - 1];
        std::cout << "realtime " << elapsed.count() << ' ' << cpu_seconds << ' ' << p95 << '\n';
        state(cpu, bus, false);
    }
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
