// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#include "firmware.hpp"

#include <boost/json/src.hpp>

#include <fstream>
#include <iterator>
#include <stdexcept>

namespace centurion
{
namespace
{

boost::json::object read_object(const std::filesystem::path& file)
{
    std::ifstream stream(file);
    if (!stream)
        throw std::runtime_error("Cannot open " + file.string());
    const std::string input { std::istreambuf_iterator<char>(stream), {} };
    return boost::json::parse(input).as_object();
}

std::vector<std::uint8_t> decode_base64(std::string_view encoded, std::size_t length)
{
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const auto padding = (3 - length % 3) % 3;
    if (encoded.size() != (length + 2) / 3 * 4
        || !encoded.ends_with(std::string_view("==").substr(0, padding)))
        throw std::runtime_error("invalid base64 length or padding");

    std::vector<std::uint8_t> result;
    result.reserve(length);
    unsigned pending = 0;
    int bits = 0;
    for (const auto digit : encoded.substr(0, encoded.size() - padding)) {
        const auto value = alphabet.find(digit);
        if (value == alphabet.npos)
            throw std::runtime_error("invalid base64 digit");
        pending = (pending << 6) | static_cast<unsigned>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            result.push_back(static_cast<std::uint8_t>(pending >> bits));
            pending &= (1u << bits) - 1;
        }
    }
    if (pending != 0 || result.size() != length)
        throw std::runtime_error("invalid base64 tail");
    return result;
}

}

Firmware Firmware::load(const std::filesystem::path& pack, const std::filesystem::path& spec)
{
    const auto file = read_object(pack);
    if (file.at("format").as_string() != "cenre-firmware-v1")
        throw std::runtime_error("Invalid CenRE firmware format in " + pack.string());
    const auto& roms = file.at("roms").as_object();

    Firmware firmware;
    for (const auto& [name, size] : read_object(spec)) {
        const auto length = size.as_int64();
        if (length < 0 || length > 0x40000)
            throw std::runtime_error("Invalid firmware table length for " + std::string(name));
        try {
            const auto& encoded = roms.at(name).as_string();
            firmware.tables_.emplace(name, decode_base64(encoded, static_cast<std::size_t>(length)));
        } catch (const std::exception& error) {
            throw std::runtime_error("Firmware table " + std::string(name) + " in " + pack.string() + ": "
                                     + error.what());
        }
    }
    return firmware;
}

std::span<const std::uint8_t> Firmware::table(std::string_view name) const
{
    const auto entry = tables_.find(name);
    if (entry == tables_.end())
        throw std::runtime_error("Missing firmware table " + std::string(name));
    return entry->second;
}

}
