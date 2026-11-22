#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace centurion
{

[[nodiscard]] std::vector<std::uint8_t> read_disk(const std::filesystem::path& file);
void save_disk(const std::filesystem::path& file, std::span<const std::uint8_t> image);

}
