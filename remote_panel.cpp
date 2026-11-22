// C++ port of the CenRE server's binary WebSocket control protocol.
#include "remote_panel.hpp"
#include "runtime.hpp"

namespace centurion
{

std::string encode_panel_state(const Cpu6& cpu)
{
    std::string frame("\xa0\x04", 2);
    frame.reserve(400);
    const auto byte = [&](unsigned value) { frame += static_cast<char>(value & 255); };
    const auto word = [&](unsigned value) {
        byte(value >> 8);
        byte(value);
    };
    const auto field = [&](unsigned tag, unsigned value) {
        byte(tag);
        byte(value);
    };
    const auto wide = [&](unsigned tag, unsigned value) {
        byte(tag);
        word(value);
    };
    const auto& regfile = cpu.register_ram();
    const auto micro = cpu.micro_state();

    field(0, cpu.condition_codes() | cpu.map());
    field(1, cpu.level());
    wide(2, cpu.memory_address());
    byte(3);
    byte(cpu.physical_address() >> 16);
    word(cpu.physical_address());
    byte(6);
    for (unsigned bit = 0; bit < 16; ++bit)
        byte(cpu.memory_address() & (1u << bit) ? 255 : 0);
    byte(8);
    for (unsigned bit = 0; bit < 4; ++bit)
        byte(cpu.level() & (1u << bit) ? 255 : 0);
    byte(10);
    for (unsigned bit = 0; bit < 8; ++bit)
        byte((cpu.map() | ((micro.system_control & 0x40) >> 3) | cpu.condition_codes()) & (1u << bit) ? 255
                                                                                                      : 0);

    for (unsigned row = 0; row < regfile.size(); row += 16) {
        byte(12);
        byte(row);
        for (auto value : std::span(regfile).subspan(row, 16))
            byte(value);
        // RemCPU's tag-12 decoder continues into tag 16 without reading a new tag.
        word(micro.work_address);
    }
    wide(16, micro.work_address);
    field(17, micro.result);
    field(18, micro.register_index);
    field(19, micro.register_data);
    field(20, micro.nibble_swap);
    field(21, micro.alu_flags);
    field(22, micro.bus_control);
    field(23, micro.system_control);
    field(24, micro.page_entry);
    wide(25, cpu.micro_pc());
    field(26, 0);
    wide(30, micro.sequence_pointer);
    wide(31, micro.sequence_hold);
    field(32, micro.sequence_stack_position);
    for (auto value : micro.sequence_stack)
        word(value);
    field(33, micro.alu_q);
    byte(34);
    for (auto value : micro.alu_registers)
        byte(value);
    field(35, micro.memory_data_out);
    field(36, micro.memory_data_in);
    return frame;
}

std::string remote_panel_message(Runtime& runtime, std::string_view bytes)
{
    if (bytes.size() < 2)
        return {};
    const auto command = static_cast<unsigned char>(bytes[0]) | (static_cast<unsigned char>(bytes[1]) << 8);
    switch (command) {
    case 0x0020: {
        std::string hello(24, '\0');
        hello[0] = 0x20;
        hello[3] = 3;
        hello.replace(4, 6, "WebCen");
        hello[22] = 4;
        return hello;
    }
    case 0x0040: return { "\x48\0\0\0\x02\0\x03\0", 8 };
    case 0x1800: return { "\x08\x18", 2 };
    case 0x2000: return { "\0\x20", 2 };
    case 0x0009: runtime.step(); break;
    case 0x000b: runtime.preview_step(); break;
    case 0x000a: break;
    case 0x000d:
        if (bytes.size() < 3)
            return {};
        for (unsigned i = 0; i < static_cast<unsigned char>(bytes[2]); ++i)
            runtime.step();
        break;
    default: return {};
    }
    return encode_panel_state(runtime.machine.cpu);
}

}
