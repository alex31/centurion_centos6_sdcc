#include "images.hpp"
#include "finch.hpp"

#include <cerrno>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <system_error>

namespace centurion
{

std::vector<std::uint8_t> read_disk(const std::filesystem::path& file)
{
    const auto size = std::filesystem::file_size(file);
    if (size == 0 || size > 4 * FinchImage::platter_size)
        throw std::invalid_argument("Unsupported disk image size");
    std::vector<std::uint8_t> disk(size);
    std::ifstream input(file, std::ios::binary);
    if (!input.read(reinterpret_cast<char*>(disk.data()), disk.size())
        || input.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("Cannot read complete disk image " + file.string());
    return disk;
}

void save_disk(const std::filesystem::path& file, std::span<const std::uint8_t> image)
{
    auto* output = std::fopen(file.c_str(), "wbx");
    if (!output)
        throw std::system_error(errno, std::generic_category(), file.string());
    const bool written = std::fwrite(image.data(), 1, image.size(), output) == image.size();
    const bool closed = std::fclose(output) == 0;
    if (!written || !closed) {
        std::error_code error;
        std::filesystem::remove(file, error);
        throw std::runtime_error("Could not save complete image to " + file.string());
    }
}

}
