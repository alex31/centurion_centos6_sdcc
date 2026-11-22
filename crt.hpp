// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace centurion
{

class Crt
{
public:
    static constexpr int columns = 80;
    static constexpr int rows = 24;
    static constexpr int main_cells = columns * rows;
    using Cells = std::array<std::uint16_t, columns*(rows + 1)>;

    Crt() { reset(); }
    void reset();
    // A true result requests transmission of status_message() to the guest.
    bool receive(std::uint8_t byte);
    [[nodiscard]] std::array<std::uint8_t, 11> status_message() const noexcept;
    [[nodiscard]] const Cells& cells() const noexcept { return cells_; }
    [[nodiscard]] int x() const noexcept { return column_; }
    [[nodiscard]] int y() const noexcept { return row_; }
    [[nodiscard]] bool application_cursor_keys() const noexcept { return application_cursor_; }

private:
    enum class Mode { Normal, Escape, SelectRow, SelectColumn, VerticalTab, Attributes, Horizontal, Csi };
    enum class Insert { Off, Line, Screen };

    void cursor(int column, int row);
    void scroll();
    void next_line();
    void next_column();
    void clear();
    void csi(std::uint8_t byte);
    bool escape(std::uint8_t byte);
    void print(std::uint8_t byte);
    void attributes(std::span<const int> values);

    Cells cells_;
    int column_ {}, row_ {}, address_row_ {};
    std::uint16_t attribute_ {};
    bool line_drawing_ {}, show_control_ {}, application_cursor_ {};
    Mode mode_ { Mode::Normal };
    Insert insert_ { Insert::Off };
    std::array<char, 32> csi_parameters_ {};
    unsigned csi_length_ {};
};

class CrtRenderer
{
public:
    [[nodiscard]] std::string diff(const Crt& crt);
    void reset() noexcept { initialized_ = false; }

private:
    Crt::Cells cells_ {};
    int x_ {}, y_ {};
    bool initialized_ {};
};

}
