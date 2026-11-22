// C++ port of the CenRE server's Telnet frontend.
#include "telnet_codec.hpp"

#include <cctype>
#include <utility>

namespace centurion
{
namespace
{

constexpr std::uint8_t iac = 255, will = 251, wont = 252, do_option = 253, dont = 254;
constexpr std::uint8_t subnegotiation = 250, subnegotiation_end = 240;
constexpr std::uint8_t echo = 1, suppress_go_ahead = 3, terminal_type = 24, window_size = 31;

}

std::string_view TelnetCodec::greeting() noexcept
{
    return "\x1b[2J\x1b[H\x1b[?25l\xff\xfb\x01\xff\xfb\x03\xff\xfd\x18\xff\xfd\x1f";
}

void TelnetCodec::data(std::uint8_t byte, const Key& key)
{
    if (std::exchange(carriage_return_, byte == '\r') && (byte == '\n' || byte == 0))
        return;
    if (byte == 8 || byte == 127)
        key(127);
    else if (byte >= 32 || byte == '\r' || byte == '\n' || byte == '\t' || byte == 27 || byte == 3
             || byte == 4 || byte == 25)
        key(byte);
}

void TelnetCodec::option(std::uint8_t byte, const Reply& reply)
{
    std::uint8_t response = 0;
    switch (command_) {
    case will:
        if (byte == terminal_type)
            reply("\xff\xfa\x18\x01\xff\xf0");
        else if (byte != window_size)
            response = byte == suppress_go_ahead ? do_option : dont;
        break;
    case do_option:
        if (byte != echo && byte != suppress_go_ahead)
            response = wont;
        break;
    }
    if (response != 0) {
        const char message[]
            = { static_cast<char>(iac), static_cast<char>(response), static_cast<char>(byte) };
        reply({ message, sizeof message });
    }
}

void TelnetCodec::subnegotiation_byte(std::uint8_t byte)
{
    if (sub_length_ < sub_data_.size())
        sub_data_[sub_length_++] = byte;
}

void TelnetCodec::finish_subnegotiation()
{
    if (suboption_ == window_size && sub_length_ == 4) {
        const auto columns = (sub_data_[0] << 8) | sub_data_[1];
        const auto rows = (sub_data_[2] << 8) | sub_data_[3];
        if (columns)
            columns_ = columns;
        if (rows)
            rows_ = rows;
    } else if (suboption_ == terminal_type && sub_length_ > 1 && sub_data_[0] <= 1) {
        type_.clear();
        for (unsigned index = 1; index < sub_length_; ++index) {
            const auto byte = sub_data_[index];
            if (byte >= 32 && byte < 127)
                type_ += static_cast<char>(std::tolower(byte));
        }
    }
}

void TelnetCodec::consume(std::span<const std::uint8_t> input, const Key& key, const Reply& reply)
{
    for (const auto byte : input) {
        switch (state_) {
        case State::Data:
            if (byte == iac)
                state_ = State::Command;
            else
                data(byte, key);
            break;
        case State::Command:
            state_ = State::Data;
            if (byte == iac) {
                data(byte, key);
            } else if (byte == subnegotiation) {
                state_ = State::SubnegotiationOption;
            } else if (byte >= will && byte <= dont) {
                command_ = byte;
                state_ = State::Option;
            }
            break;
        case State::Option:
            option(byte, reply);
            state_ = State::Data;
            break;
        case State::SubnegotiationOption:
            suboption_ = byte;
            sub_length_ = 0;
            state_ = State::Subnegotiation;
            break;
        case State::Subnegotiation:
            if (byte == iac)
                state_ = State::SubnegotiationCommand;
            else
                subnegotiation_byte(byte);
            break;
        case State::SubnegotiationCommand:
            if (byte == subnegotiation_end) {
                finish_subnegotiation();
                state_ = State::Data;
            } else {
                if (byte == iac)
                    subnegotiation_byte(byte);
                state_ = State::Subnegotiation;
            }
            break;
        }
    }
}

}
