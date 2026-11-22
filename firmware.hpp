// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace centurion
{

class Firmware
{
public:
    static Firmware load(const std::filesystem::path& pack, const std::filesystem::path& spec);
    [[nodiscard]] std::span<const std::uint8_t> table(std::string_view name) const;

private:
    std::map<std::string, std::vector<std::uint8_t>, std::less<>> tables_;
};

}
