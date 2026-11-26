#include "machine.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace
{
void check_sequencer()
{
    // Independent oracle: resolve the three original four-bit selectors and
    // propagate carry from the low slice, including bit 11 of the internal PC.
    for (unsigned selectors = 0; selectors < 64; ++selectors) {
        for (unsigned word = 0; word < 4096; ++word) {
            centurion::MicroSequencer sequence;
            sequence.pc = word;
            sequence.hold = (word ^ 0x59) & 255;
            sequence.sp = (word >> 2) & 3;
            for (unsigned i = 0; i < 4; ++i)
                sequence.stack[i] = (word + 0xf7 * (i + 1)) & 0xfff;
            const unsigned direct = (word ^ 0x5a7) & 0x7ff, branch = (word >> 4) & 15;
            const unsigned control = word & 3;
            const std::array<unsigned, 4> sources { sequence.pc, sequence.hold, sequence.stack[sequence.sp],
                                                    direct };
            unsigned output = 0, next = 0, carry = 1;
            for (unsigned i = 0; i < 3; ++i) {
                const auto select = (selectors >> (2 * i)) & 3;
                const auto nibble = ((sources[select] >> (4 * i)) & 15) | (i == 0 ? branch : 0);
                output |= nibble << (4 * i);
                const auto sum = nibble + carry;
                next |= (sum & 15) << (4 * i);
                carry = sum >> 4;
            }
            auto stack = sequence.stack;
            auto sp = sequence.sp;
            if (control == 0)
                sp = (sp + 1) & 3;
            if (control == 2) {
                sp = (sp - 1) & 3;
                stack[sp] = sequence.pc;
            }
            const auto actual = sequence.clock(selectors & 3, (selectors >> 2) & 3, selectors >> 4, direct,
                                               branch, control);
            if (actual != output || sequence.pc != next || sequence.sp != sp || sequence.stack != stack)
                throw std::runtime_error("Microsequencer disagrees with the three four-bit slices");
        }
    }
}

void check_bus(const centurion::Firmware& firmware)
{
    centurion::Backplane bus(firmware);
    bool fault {};
    for (unsigned i = 0; i < 256; ++i) {
        bus.write(0x2000 + i, i, true);
        if (bus.read(0x2000 + i, fault) != i || !fault)
            throw std::runtime_error("Inverted RAM parity was lost");
    }
    const std::array<std::uint8_t, 127> zeros {};
    bus.load_ram(0x2001, zeros);
    for (unsigned i = 0; i < 256; ++i) {
        bus.read(0x2000 + i, fault);
        if (fault != (i == 0 || i >= 128))
            throw std::runtime_error("Wrong parity after a partial reload");
        bus.write(0x2000 + i, i);
        if (bus.read(0x2000 + i, fault) != i || fault)
            throw std::runtime_error("RAM write failed to repair parity");
    }

    centurion::Mux first, second;
    bus.attach(first, 0x3f200, 0x10, centurion::Backplane::Irq::Wired);
    bus.attach(second, 0x3f210, 0x10, centurion::Backplane::Irq::Wired);
    first.write(10, 3);
    second.write(10, 7);
    for (auto* device : { &first, &second }) {
        device->write(12, 1);
        device->write(14, 0);
    }
    if (!bus.interrupt_pending(6) || bus.interrupt_pending(7) || bus.acknowledge_interrupt(0) != 3)
        throw std::runtime_error("IRQ priority or daisy-chain order changed");
    second.write(10, 2);
    if (!bus.interrupt_pending(1) || bus.interrupt_pending(2))
        throw std::runtime_error("Pending IRQ level did not change");
    second.write(13, 0);
    if (bus.interrupt_pending(0))
        throw std::runtime_error("Disabled IRQ is still pending");

    struct Polled : centurion::Device {
        unsigned priority {};
        unsigned interrupt_level() const override { return priority; }
        std::uint8_t read(unsigned) override { return 0; }
        void write(unsigned, std::uint8_t) override { }
    } polled;
    bus.attach(polled, 0x3f220, 1, centurion::Backplane::Irq::Wired);
    polled.priority = 9;
    if (!bus.interrupt_pending(8) || bus.acknowledge_interrupt(8) != 9)
        throw std::runtime_error("A polled IRQ was not sampled");
    polled.priority = 0;
    if (bus.interrupt_pending(0))
        throw std::runtime_error("A polled IRQ remained pending");
}

void check_mux_causes()
{
    centurion::Mux mux;
    mux.write(0, 0xe5);
    mux.write(2, 0xe5);
    mux.write(4, 0xe5);
    mux.write(10, 6);
    mux.write(14, 0);
    mux.advance_transmit();
    mux.write(1, 'A');
    if (mux.interrupt_level() != 6) throw std::runtime_error("Missing transmit IRQ");
    mux.acknowledge();
    mux.write(1, 'B');
    if (mux.read(15) != 1 || mux.interrupt_level() || mux.read(15) != 255)
        throw std::runtime_error("Coalesced TX completions generated a spurious receive IRQ");

    mux.write(12, 12); // Request TX service on ports 2 and 3.
    mux.receive(1, 'C');
    mux.receive(2, 'D');
    for (unsigned cause : {2, 4, 5, 7}) {
        if (mux.interrupt_level() != 6) throw std::runtime_error("Lost pending MUX IRQ");
        mux.acknowledge();
        if (mux.read(15) != cause) throw std::runtime_error("Wrong pending MUX cause");
        if (cause == 2 && mux.read(3) != 'C') throw std::runtime_error("Lost RX1 data");
        if (cause == 4 && mux.read(5) != 'D') throw std::runtime_error("Lost RX2 data");
    }
    if (mux.interrupt_level() || mux.read(15) != 255) throw std::runtime_error("MUX did not become idle");
}

void check_cmd_divider(centurion::Machine& machine)
{
    bool fault {};
    for (unsigned delay : { 3, 41, 86, 1027 }) {
        machine.run_slice(delay);
        machine.reset();
        const auto remaining = 41 - machine.cpu.microsteps() % 41;
        machine.bus.write(0x3f808, 0);
        for (unsigned i = 0; i < remaining; ++i) {
            if (machine.bus.read(0x3f809, fault) != 8)
                throw std::runtime_error("CMD delay completed early");
            machine.step();
        }
        if (machine.bus.read(0x3f809, fault) != 1 || machine.bus.read(0x3f808, fault) != 0)
            throw std::runtime_error("CMD delay divider lost its phase");
    }
}
}

int main(int argc, char** argv)
try {
    if (argc != 4)
        throw std::invalid_argument("Usage: cpp-peripherals-probe FIRMWARE SPEC PROGRAM");
    const auto firmware = centurion::Firmware::load(argv[1], argv[2]);
    std::ifstream input(argv[3], std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot open CPU6 program");
    const std::vector<std::uint8_t> program { std::istreambuf_iterator<char>(input), {} };

    check_sequencer();
    check_bus(firmware);
    check_mux_causes();
    centurion::Machine machine(firmware);
    check_cmd_divider(machine);
    std::string output;
    machine.mux.observe_writes(0,
                               [&output](std::uint8_t value) { output += static_cast<char>(value & 127); });
    machine.reset();
    for (unsigned i = 0; i < 10'000 && !machine.cpu.at_instruction_boundary(); ++i)
        machine.step();
    if (!machine.cpu.at_instruction_boundary())
        throw std::runtime_error("Reset did not reach an instruction boundary");
    machine.bus.load_ram(0x2000, program);
    machine.cpu.set_pc(0x2000);
    for (unsigned slice = 0; slice < 2000 && !output.contains("CMD"); ++slice)
        machine.run_slice(100'000);
    if (!output.contains("PASS CMD"))
        throw std::runtime_error("Guest CMD test failed: " + output);
    if (machine.printer.transcript() != "P\xe2\x87\x8a\n" || machine.diagnostic.display() != 0x56
        || machine.diagnostic.points() != 2 || !machine.diagnostic.blank())
        throw std::runtime_error("Printer or diagnostic card state differs");

    // Abort between the I/O command and the first CPU/DMA clock edge.
    bool parity_fault = false;
    machine.bus.write(0x3f808, 0x43);
    machine.bus.write(0x3f809, 0);
    if (!(machine.bus.read(0x3f809, parity_fault) & 1) || machine.bus.read(0x3f808, parity_fault) != 0x23)
        throw std::runtime_error("CMD DMA abort did not return the lag status");

    machine.bus.insert_diagnostic_roms(true);
    machine.reset();
    machine.cpu.set_sense_switches(1);
    bool entered_diagnostics = false;
    for (unsigned tick = 0; tick < 100'000; ++tick) {
        machine.step();
        if (machine.cpu.at_instruction_boundary() && machine.cpu.pc() >= 0x8000
            && machine.cpu.pc() < 0xa000) {
            entered_diagnostics = true;
            break;
        }
    }
    if (!entered_diagnostics)
        throw std::runtime_error("Bootstrap did not enter the diagnostic ROM");
    if (machine.bus.read(0x8000, parity_fault) != firmware.table("f1").front())
        throw std::runtime_error("Diagnostic ROM is not mapped");
    std::cout << "PASS: CMD DMA round trip, printer, tape and diagnostic ROM boot\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
