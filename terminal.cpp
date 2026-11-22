// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#include "terminal.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace centurion
{
namespace
{

constexpr std::array boot_prompts {
    "D=", "SYSTEM DISK (S)= 1", "ENTER NEW SYSTEM DATE", "ENTER SYSTEM TIME", "CRT0 READY",
};
constexpr std::array boot_statuses {
    "Waiting for the system disk prompt",
    "Waiting for the date prompt",
    "Waiting for the time prompt",
    "Waiting for CENTOS",
    "CENTOS is ready",
};
constexpr auto recent_limit = 120;
constexpr auto boot_timeout = std::chrono::seconds(45);

bool whitespace(char character) { return std::isspace(static_cast<unsigned char>(character)) != 0; }

}

void Terminal::reset()
{
    crt_.reset();
    pending_.clear();
    recent_output_.clear();
    boot_status_.clear();
    boot_stage_ = 0;
}

void Terminal::set_available(bool available)
{
    available_ = available;
    if (!available_) {
        if (boot_stage_)
            cancel_boot("Boot cancelled: raw serial in use");
        pending_.clear();
    }
}

void Terminal::cancel_boot(std::string_view status)
{
    boot_stage_ = 0;
    pending_.clear();
    boot_status_ = status;
}

void Terminal::enqueue(std::span<const std::uint8_t> data)
{
    if (data.size() > queue_limit - pending_.size())
        throw std::runtime_error("Terminal input is full");
    pending_.insert(pending_.end(), data.begin(), data.end());
}

void Terminal::enqueue(std::string_view data)
{
    enqueue({ reinterpret_cast<const std::uint8_t*>(data.data()), data.size() });
}

void Terminal::receive(std::uint8_t byte)
{
    byte &= 127;
    if (crt_.receive(byte) && available_) {
        const auto status = crt_.status_message();
        if (status.size() <= queue_limit - pending_.size())
            enqueue(status);
    }
    if (recent_output_.size() == recent_limit)
        recent_output_.erase(0, 1);
    recent_output_ += static_cast<char>(byte);
    if (boot_stage_ && recent_output_.ends_with("ERROR")
        && (recent_output_.size() == 5
            || std::string_view("\r\n\f").contains(recent_output_[recent_output_.size() - 6]))) {
        cancel_boot("Boot failed: check the CENTOS image in DSK2 unit 1");
        return;
    }
    advance_boot();
}

void Terminal::telnet_byte(std::uint8_t byte)
{
    if (!available_)
        return;
    if (boot_stage_) {
        if (byte != 3)
            return;
        cancel_boot("Boot cancelled");
    }
    if (byte == 127)
        byte = 8;
    else if (byte == '\n')
        byte = '\r';
    if (pending_.size() < queue_limit)
        pending_.push_back(byte & 127);
}

void Terminal::key(std::string_view key)
{
    if (!available_)
        throw std::runtime_error("CRT1 is reserved for raw serial");
    if (recent_output_.empty())
        throw std::runtime_error("The terminal is waiting for output from the CPU");
    if (!boot_stage_ && crt_.application_cursor_keys() && (key == "\x1b[5~" || key == "\x1b[6~")) {
        enqueue(key);
        return;
    }
    const bool arrow = key.size() == 3 && (key.starts_with("\x1b[") || key.starts_with("\x1bO"))
        && key[2] >= 'A' && key[2] <= 'D';
    if (!arrow && (key.size() != 1 || static_cast<unsigned char>(key.front()) > 127))
        return;
    constexpr std::array<std::uint8_t, 4> arrows { 26, 10, 6, 21 };
    const auto code = arrow   ? arrows[key[2] - 'A']
        : key.front() == 127  ? 8
        : key.front() == '\n' ? 13
                              : key.front();
    if (boot_stage_) {
        if (code != 3)
            return;
        cancel_boot("Boot cancelled");
    }
    if (arrow && crt_.application_cursor_keys())
        enqueue(std::array<std::uint8_t, 3> { 27, 'O', static_cast<std::uint8_t>(key[2]) });
    else
        enqueue(std::array { static_cast<std::uint8_t>(code) });
}

void Terminal::paste(std::string_view text)
{
    if (!available_ || recent_output_.empty() || boot_stage_)
        return;
    std::string ascii;
    ascii.reserve(std::min<std::size_t>(text.size(), queue_limit));
    const auto available = queue_limit - pending_.size();
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
            continue;
        if (c == '\n' || c == '\r' || c == '\t' || (c >= 32 && c < 127)) {
            if (ascii.size() == available)
                throw std::runtime_error("Terminal input is full");
            ascii += c == '\n' ? '\r' : static_cast<char>(c);
        }
    }
    enqueue(ascii);
}

bool Terminal::at_boot_prompt() const
{
    const auto first = crt_.cells().begin() + crt_.y() * Crt::columns;
    auto end = first + crt_.x();
    while (end != first && whitespace(end[-1] & 127))
        --end;
    return end - first >= 2 && (end[-2] & 127) == 'D' && (end[-1] & 127) == '='
        && (end - first == 2 || whitespace(end[-3] & 127));
}

bool Terminal::boot_ready() const
{
    return available_ && !boot_stage_ && pending_.empty() && at_boot_prompt();
}

void Terminal::validate_clock(std::string_view date, std::string_view time)
{
    const auto digits = [](std::string_view input) {
        return input.size() == 6 && std::ranges::all_of(input, [](char c) { return c >= '0' && c <= '9'; });
    };
    if (!digits(date) || !digits(time))
        throw std::invalid_argument("Invalid boot date or time");
}

void Terminal::boot(std::string_view date, std::string_view time, bool wait_for_prompt)
{
    validate_clock(date, time);
    if (!wait_for_prompt && !boot_ready())
        throw std::runtime_error("Wait for the ROM D= prompt");
    date_ = date;
    time_ = time;
    boot_stage_ = 1;
    boot_status_ = "Waiting for the ROM D= prompt";
    deadline_ = Clock::now() + boot_timeout;
    advance_boot();
}

void Terminal::advance_boot()
{
    if (!boot_stage_)
        return;
    const auto stage = boot_stage_ - 1;
    if (stage == 0 ? !at_boot_prompt() : !recent_output_.ends_with(boot_prompts[stage]))
        return;
    boot_status_ = boot_statuses[stage];
    deadline_ = Clock::now() + boot_timeout;
    ++boot_stage_;
    switch (stage) {
    case 0: enqueue("H1"); break;
    case 1: enqueue("\r"); break;
    case 2: enqueue(date_ + '\r'); break;
    case 3: enqueue(time_ + '\r'); break;
    case 4: boot_stage_ = 0; break;
    }
}

void Terminal::tick()
{
    if (!available_)
        return;
    if (boot_stage_ && Clock::now() > deadline_)
        cancel_boot("Boot timed out waiting for the next prompt");
    if (recent_output_.empty() || pending_.empty() || !mux_.can_receive(port_)
        || (!mux_.rts(port_) && pending_.front() != 3))
        return;
    mux_.receive(port_, pending_.front());
    pending_.pop_front();
}

}
