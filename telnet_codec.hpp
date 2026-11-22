// C++ port of the CenRE server's Telnet frontend.
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace centurion
{

class TelnetCodec
{
public:
    using Key = std::function<void(std::uint8_t)>;
    using Reply = std::function<void(std::string_view)>;

    [[nodiscard]] static std::string_view greeting() noexcept;
    void consume(std::span<const std::uint8_t> input, const Key& key, const Reply& reply);
    [[nodiscard]] unsigned rows() const noexcept { return rows_; }
    [[nodiscard]] unsigned columns() const noexcept { return columns_; }
    [[nodiscard]] std::string_view type() const noexcept { return type_; }

private:
    enum class State { Data, Command, Option, SubnegotiationOption, Subnegotiation, SubnegotiationCommand };
    void data(std::uint8_t byte, const Key& key);
    void option(std::uint8_t byte, const Reply& reply);
    void subnegotiation_byte(std::uint8_t byte);
    void finish_subnegotiation();
    State state_ { State::Data };
    std::uint8_t command_ {};
    std::uint8_t suboption_ {};
    unsigned sub_length_ {};
    std::array<std::uint8_t, 64> sub_data_ {};
    unsigned columns_ {80}, rows_ {24};
    std::string type_;
    bool carriage_return_ {};
};

}
