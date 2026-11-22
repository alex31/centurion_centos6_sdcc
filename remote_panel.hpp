// C++ port of the CenRE server's binary WebSocket control protocol.
#pragma once

#include <string>
#include <string_view>

namespace centurion
{

class Runtime;
class Cpu6;

[[nodiscard]] std::string encode_panel_state(const Cpu6& cpu);
[[nodiscard]] std::string remote_panel_message(Runtime& runtime, std::string_view bytes);

}
