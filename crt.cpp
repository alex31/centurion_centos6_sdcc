// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#include "crt.hpp"

#include <algorithm>
#include <charconv>
#include <string_view>
#include <utility>

namespace centurion
{
namespace
{

constexpr auto half = 0x100, blink = 0x200, blank = 0x400, reverse = 0x1000, underline = 0x2000;
constexpr auto visual_attributes = half | blink | blank | reverse | underline;

void append_number(std::string& text, int value)
{
    char digits[16];
    const auto [end, error] = std::to_chars(digits, digits + sizeof digits, value);
    if (error == std::errc {})
        text.append(digits, end);
}

void append_cursor(std::string& text, int row, int column)
{
    text += "\x1b[";
    append_number(text, row + 1);
    text += ';';
    append_number(text, column + 1);
    text += 'H';
}

void append_attributes(std::string& text, std::uint16_t value)
{
    text += "\x1b[0";
    if (value & reverse)
        text += ";7";
    if (value & underline)
        text += ";4";
    if (value & blink)
        text += ";5";
    if (value & half)
        text += ";2";
    text += 'm';
}

}

void Crt::reset()
{
    cells_.fill(' ');
    column_ = row_ = address_row_ = 0;
    attribute_ = 0;
    line_drawing_ = show_control_ = application_cursor_ = false;
    mode_ = Mode::Normal;
    insert_ = Insert::Off;
    csi_length_ = 0;
}

void Crt::cursor(int column, int row)
{
    column_ = std::clamp(column, 0, columns - 1);
    row_ = std::clamp(row, 0, rows - 1);
}

void Crt::scroll()
{
    std::copy(cells_.begin() + columns, cells_.begin() + main_cells, cells_.begin());
    std::fill_n(cells_.begin() + main_cells - columns, columns, ' ');
}

void Crt::next_line()
{
    if (row_ == rows - 1)
        scroll();
    else
        ++row_;
}

void Crt::next_column()
{
    if (++column_ == columns) {
        column_ = 0;
        next_line();
    }
}

void Crt::clear()
{
    std::fill_n(cells_.begin(), main_cells, ' ');
    cursor(0, 0);
}

std::array<std::uint8_t, 11> Crt::status_message() const noexcept
{
    return { 2,
             0x50,
             0x6b,
             0x64,
             0x58,
             0x40,
             0x40,
             static_cast<std::uint8_t>(column_ + 32),
             static_cast<std::uint8_t>(row_ + 32),
             static_cast<std::uint8_t>(cells_[row_ * columns + column_] & 127),
             13 };
}

void Crt::attributes(std::span<const int> values)
{
    for (const auto value : values) {
        switch (value) {
        case 0: attribute_ &= ~visual_attributes; break;
        case 1:
        case 22: attribute_ &= ~half; break;
        case 2: attribute_ |= half; break;
        case 4: attribute_ |= underline; break;
        case 5: attribute_ |= blink; break;
        case 7: attribute_ |= reverse; break;
        case 8: attribute_ |= blank; break;
        case 24: attribute_ &= ~underline; break;
        case 25: attribute_ &= ~blink; break;
        case 27: attribute_ &= ~reverse; break;
        case 28: attribute_ &= ~blank; break;
        }
    }
}

void Crt::csi(std::uint8_t byte)
{
    if ((byte >= '0' && byte <= '9') || byte == ';' || byte == '?') {
        if (csi_length_ < csi_parameters_.size()) {
            csi_parameters_[csi_length_++] = byte;
            mode_ = Mode::Csi;
        }
        return;
    }
    if (byte == 27) {
        mode_ = Mode::Escape;
        return;
    }
    if (byte < 0x40 || byte > 0x7e)
        return;

    auto parameters = std::string_view(csi_parameters_.data(), csi_length_);
    const bool private_mode = parameters.starts_with('?');
    if (private_mode) {
        parameters.remove_prefix(1);
        while (!parameters.empty()) {
            const auto end = parameters.find(';');
            if (parameters.substr(0, end) == "1" && (byte == 'h' || byte == 'l')) {
                application_cursor_ = byte == 'h';
                break;
            }
            if (end == parameters.npos)
                break;
            parameters.remove_prefix(end + 1);
        }
        return;
    }

    // Preserve omitted fields for CUP and the zero default for SGR.
    std::array<int, 33> values {};
    unsigned count = 0;
    do {
        const auto end = parameters.find(';');
        const auto digits = parameters.substr(0, end);
        const auto [_, error] = std::from_chars(digits.data(), digits.data() + digits.size(), values[count]);
        if (error == std::errc::result_out_of_range)
            values[count] = main_cells;
        ++count;
        if (end == parameters.npos)
            break;
        parameters.remove_prefix(end + 1);
    } while (true);
    const auto n = values[0];
    const auto offset = row_ * columns + column_;
    const auto start = cells_.begin() + row_ * columns;
    switch (byte) {
    case 'H':
    case 'f': cursor(std::max(values[1], 1) - 1, std::max(n, 1) - 1); break;
    case 'G': cursor(std::max(n, 1) - 1, row_); break;
    case 'J':
        if (n <= 2)
            std::fill(cells_.begin() + (n == 0 ? offset : 0),
                      cells_.begin() + (n == 1 ? offset + 1 : main_cells), ' ');
        break;
    case 'K':
        if (n <= 2)
            std::fill(start + (n == 0 ? column_ : 0), start + (n == 1 ? column_ + 1 : columns), ' ');
        break;
    case 'm': attributes(std::span(values).first(count)); break;
    }
}

bool Crt::escape(std::uint8_t byte)
{
    const auto line = cells_.begin() + row_ * columns;
    const auto current = line + column_;
    const auto end = cells_.begin() + main_cells;
    switch (byte) {
    case '[':
        csi_length_ = 0;
        mode_ = Mode::Csi;
        break;
    case 5: return true;
    case '0': mode_ = Mode::Attributes; break;
    case '1': line_drawing_ = true; break;
    case '2': line_drawing_ = false; break;
    case 'E':
        std::copy(current + 1, line + columns, current);
        line[columns - 1] = ' ';
        break;
    case 'e':
        std::copy(current + 1, end, current);
        end[-1] = ' ';
        break;
    case 'F':
    case 'f':
        insert_ = insert_ != Insert::Off ? Insert::Off : (byte == 'F' ? Insert::Line : Insert::Screen);
        break;
    case 'L':
        std::copy(line + columns, end, line);
        std::fill(end - columns, end, ' ');
        column_ = 0;
        break;
    case 'M':
        std::copy_backward(line, end - columns, end);
        std::fill_n(line, columns, ' ');
        column_ = 0;
        break;
    case 'G': clear(); break;
    case 'K': std::fill(current, line + columns, ' '); break;
    case 'Y': mode_ = Mode::SelectRow; break;
    case 'Z': show_control_ = true; break;
    case 'k': std::fill(current, end, ' '); break;
    case 's':
        insert_ = Insert::Off;
        line_drawing_ = false;
        break;
    }
    return false;
}

void Crt::print(std::uint8_t byte)
{
    const auto offset = row_ * columns + column_;
    if (insert_ != Insert::Off) {
        const auto end = cells_.begin() + (insert_ == Insert::Line ? (row_ + 1) * columns : main_cells);
        std::copy_backward(cells_.begin() + offset, end - 1, end);
    }
    if (line_drawing_ && byte >= 64 && byte < 108) {
        const unsigned glyph = byte + 64;
        cells_[offset] = (glyph & 0xfc) | (attribute_ & 0xfc00) | ((glyph << 8) & 0x300);
    } else {
        cells_[offset] = byte | attribute_;
    }
    next_column();
}

bool Crt::receive(std::uint8_t byte)
{
    byte &= 127;
    switch (std::exchange(mode_, Mode::Normal)) {
    case Mode::Csi: csi(byte); return false;
    case Mode::Escape: return escape(byte);
    case Mode::SelectRow:
        address_row_ = byte - 32;
        mode_ = Mode::SelectColumn;
        return false;
    case Mode::SelectColumn:
        if (byte - 32 < columns && address_row_ < rows)
            cursor(byte - 32, address_row_);
        return false;
    case Mode::VerticalTab: cursor(byte & 31, address_row_); return false;
    case Mode::Attributes:
        if ((byte & 0x48) == 0x40)
            attribute_ = (byte & 0x37) << 8;
        return false;
    case Mode::Horizontal: {
        const auto column = (byte >> 4) * 10 + (byte & 15);
        cursor(column >= columns ? column - columns : column, row_);
        return false;
    }
    case Mode::Normal: break;
    }

    if (std::exchange(show_control_, false) || byte >= 32) {
        print(byte);
        return false;
    }
    switch (byte) {
    case 1:
    case 9: cursor(0, rows - 1); break;
    case 6: next_column(); break;
    case 8:
    case 21:
        if (column_ != 0)
            --column_;
        else
            cursor(columns - 1, (row_ + rows - 1) % rows);
        break;
    case 10: next_line(); break;
    case 11: mode_ = Mode::VerticalTab; break;
    case 12: clear(); break;
    case 13: column_ = 0; break;
    case 16: mode_ = Mode::Horizontal; break;
    case 26: row_ = (row_ + rows - 1) % rows; break;
    case 27: mode_ = Mode::Escape; break;
    }
    return false;
}

std::string CrtRenderer::diff(const Crt& crt)
{
    std::string output;
    for (int row = 0; row < Crt::rows; ++row) {
        const auto start = row * Crt::columns;
        const auto line = std::span(crt.cells()).subspan(start, Crt::columns);
        if (initialized_ && std::ranges::equal(line, std::span(cells_).subspan(start, Crt::columns)))
            continue;
        append_cursor(output, row, 0);
        std::uint16_t last = 0xffff;
        for (const auto cell : line) {
            const std::uint16_t current = (cell & blank) ? 0 : (cell & visual_attributes);
            if (current != std::exchange(last, current))
                append_attributes(output, current);
            const auto character = cell & 127;
            output
                += (cell & blank) || character < 32 || character == 127 ? ' ' : static_cast<char>(character);
        }
    }
    if (!output.empty() || !initialized_ || x_ != crt.x() || y_ != crt.y())
        append_cursor(output, crt.y(), crt.x());
    x_ = crt.x();
    y_ = crt.y();
    cells_ = crt.cells();
    initialized_ = true;
    return output;
}

}
