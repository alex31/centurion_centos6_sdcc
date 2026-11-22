// C++ port of CenRE. Copyright (c) 2023 Meisaka Yukara.
// Distributed under the CenRE License in LICENSE.txt.
#pragma once

#include "crt.hpp"
#include "mux.hpp"

#include <chrono>
#include <deque>
#include <span>
#include <string_view>

namespace centurion
{

class Terminal
{
public:
    explicit Terminal(Mux& mux, unsigned port)
        : mux_(mux)
        , port_(port)
    {
    }

    void receive(std::uint8_t byte);
    void telnet_byte(std::uint8_t byte);
    void key(std::string_view key);
    void paste(std::string_view text);
    void boot(std::string_view date, std::string_view time, bool wait_for_prompt = false);
    static void validate_clock(std::string_view date, std::string_view time);
    void tick();
    void reset();
    void set_available(bool available);

    [[nodiscard]] const Crt& crt() const noexcept { return crt_; }
    [[nodiscard]] bool available() const noexcept { return available_; }
    [[nodiscard]] bool boot_ready() const;
    [[nodiscard]] const std::string& boot_status() const noexcept { return boot_status_; }

private:
    using Clock = std::chrono::steady_clock;
    static constexpr unsigned queue_limit = 4096;

    bool at_boot_prompt() const;
    void advance_boot();
    void cancel_boot(std::string_view status);
    void enqueue(std::span<const std::uint8_t> data);
    void enqueue(std::string_view data);

    Mux& mux_;
    unsigned port_;
    Crt crt_;
    std::deque<std::uint8_t> pending_;
    std::string recent_output_, date_, time_, boot_status_;
    Clock::time_point deadline_ {};
    unsigned boot_stage_ {};
    bool available_ { true };
};

}
