// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#include "images.hpp"
#include "machine.hpp"
#include "terminal.hpp"

#include <node_api.h>

#include <memory>
#include <stdexcept>

namespace
{

void check(napi_status status)
{
    if (status != napi_ok)
        throw std::runtime_error("Headless CPU6 binding failed");
}

std::string string(napi_env env, napi_value value)
{
    std::size_t length;
    check(napi_get_value_string_utf8(env, value, nullptr, 0, &length));
    std::string result(length + 1, '\0');
    check(napi_get_value_string_utf8(env, value, result.data(), result.size(), &length));
    result.resize(length);
    return result;
}

unsigned number(napi_env env, napi_value value)
{
    std::uint32_t result;
    check(napi_get_value_uint32(env, value, &result));
    return result;
}

struct Headless {
    centurion::Machine machine;
    centurion::Terminal terminal{machine.mux, 0};
    std::string output;
    std::vector<double> line_cycles;

    Headless(const centurion::Firmware& firmware, const std::filesystem::path& disk)
        : machine(firmware)
    {
        machine.hawk.mount(1, centurion::HawkImage(disk.filename().string(), centurion::read_disk(disk)));
        machine.mux.observe_writes(0, [this](std::uint8_t byte) {
            const auto character = byte & 127;
            output.push_back(static_cast<char>(character));
            if (character == '\n')
                line_cycles.push_back(machine.cpu.microsteps());
            terminal.receive(character);
        });
    }
};

template <std::size_t Count>
std::array<napi_value, Count> arguments(napi_env env, napi_callback_info info)
{
    std::array<napi_value, Count> values{};
    std::size_t count = Count;
    check(napi_get_cb_info(env, info, &count, values.data(), nullptr, nullptr));
    if (count != Count)
        throw std::invalid_argument("Invalid headless CPU6 arguments");
    return values;
}

Headless& session(napi_env env, napi_value handle)
{
    void* pointer{};
    check(napi_get_value_external(env, handle, &pointer));
    if (!pointer)
        throw std::invalid_argument("Missing headless CPU6 session");
    return *static_cast<Headless*>(pointer);
}

template <typename Function>
napi_value invoke(napi_env env, Function function)
{
    try {
        return function();
    } catch (const std::exception& error) {
        napi_throw_error(env, nullptr, error.what());
        return nullptr;
    }
}

napi_value open(napi_env env, napi_callback_info info)
{
    return invoke(env, [&] {
        const auto [firmware, spec, disk] = arguments<3>(env, info);
        auto emulator = std::make_unique<Headless>(
            centurion::Firmware::load(string(env, firmware), string(env, spec)), string(env, disk));
        napi_value result;
        check(napi_create_external(env, emulator.get(), [](napi_env, void* pointer, void*) {
            delete static_cast<Headless*>(pointer);
        }, nullptr, &result));
        emulator.release();
        return result;
    });
}

napi_value advance(napi_env env, napi_callback_info info)
{
    return invoke(env, [&] {
        const auto [handle, count] = arguments<2>(env, info);
        auto& emulator = session(env, handle);
        const auto slices = number(env, count);
        if (slices > 100)
            throw std::invalid_argument("CPU6 batch exceeds 100 slices");
        for (unsigned slice = 0; slice < slices; ++slice) {
            emulator.terminal.tick();
            emulator.machine.run_slice(100'000);
        }
        napi_value result, output, cycles;
        check(napi_create_array_with_length(env, 2, &result));
        check(napi_create_string_latin1(env, emulator.output.data(), emulator.output.size(), &output));
        check(napi_create_array_with_length(env, emulator.line_cycles.size(), &cycles));
        for (unsigned index = 0; index < emulator.line_cycles.size(); ++index) {
            napi_value cycle;
            check(napi_create_double(env, emulator.line_cycles[index], &cycle));
            check(napi_set_element(env, cycles, index, cycle));
        }
        check(napi_set_element(env, result, 0, output));
        check(napi_set_element(env, result, 1, cycles));
        emulator.output.clear();
        emulator.line_cycles.clear();
        return result;
    });
}

napi_value reset(napi_env env, napi_callback_info info)
{
    return invoke(env, [&] {
        const auto [handle] = arguments<1>(env, info);
        auto& emulator = session(env, handle);
        emulator.machine.reset();
        emulator.terminal.reset();
        emulator.output.clear();
        emulator.line_cycles.clear();
        napi_value result;
        check(napi_get_undefined(env, &result));
        return result;
    });
}

napi_value input(napi_env env, napi_callback_info info)
{
    return invoke(env, [&] {
        const auto [handle, code] = arguments<2>(env, info);
        auto& mux = session(env, handle).machine.mux;
        const auto value = number(env, code);
        const bool accepted = mux.can_receive(0) && (value == 256 || value == 3 || mux.rts(0));
        if (accepted && value < 256)
            mux.receive(0, value);
        napi_value result;
        check(napi_get_boolean(env, accepted, &result));
        return result;
    });
}

napi_value cell(napi_env env, napi_callback_info info)
{
    return invoke(env, [&] {
        const auto [handle, column, row] = arguments<3>(env, info);
        const auto x = number(env, column), y = number(env, row);
        const auto& cells = session(env, handle).terminal.crt().cells();
        const unsigned value = x < centurion::Crt::columns && y <= centurion::Crt::rows
            ? cells[y * centurion::Crt::columns + x] : 0;
        napi_value result;
        check(napi_create_uint32(env, value, &result));
        return result;
    });
}

napi_value save(napi_env env, napi_callback_info info)
{
    return invoke(env, [&] {
        const auto [handle, file] = arguments<2>(env, info);
        const auto* disk = session(env, handle).machine.hawk.image(1);
        centurion::save_disk(string(env, file), disk->data);
        napi_value result;
        check(napi_get_undefined(env, &result));
        return result;
    });
}

napi_value init(napi_env env, napi_value exports)
{
    const auto method = [](const char* name, napi_callback callback) {
        return napi_property_descriptor{ name, nullptr, callback, nullptr, nullptr, nullptr,
                                         napi_default, nullptr };
    };
    const napi_property_descriptor bindings[] = {
        method("open", open), method("reset", reset), method("advance", advance),
        method("input", input), method("cell", cell), method("save", save),
    };
    if (napi_define_properties(env, exports, std::size(bindings), bindings) != napi_ok)
        return nullptr;
    return exports;
}

}

NAPI_MODULE(centurion_headless, init)
