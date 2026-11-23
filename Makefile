ifneq ($(filter default undefined,$(origin CXX)),)
CXX := $(if $(shell command -v g++-15),g++-15,g++)
endif
CPPFLAGS ?=
CXXFLAGS ?= -O3 -flto -Wall -Wextra -Wpedantic
ARCHFLAGS ?= -march=native
ROOT := $(abspath ..)
OUT ?= $(ROOT)/build/out/emulator-cpp
FIRMWARE_JSON ?= $(ROOT)/build/out/emulator/firmware.json
CONFIG := $(OUT)/config
# Probe the selected compiler's preprocessor in C++26 mode.
CONTROL_STORE := $(shell if $(CXX) -std=c++26 -dM -E -x c++ /dev/null 2>/dev/null | grep -q ' __cpp_pp_embed '; then echo embed; else echo include; fi)
CXX_STANDARD := $(if $(filter embed,$(CONTROL_STORE)),c++26,c++23)
COMPILE_FLAGS := $(CPPFLAGS) $(CXXFLAGS) $(ARCHFLAGS) -std=$(CXX_STANDARD) -fPIC
COMPILE := $(CXX) $(COMPILE_FLAGS) -MMD -MP -I$(CONFIG)
CONTROL_STORE_FLAGS := $(if $(filter embed,$(CONTROL_STORE)),-DCPU6_USE_EMBED --embed-dir="$(CONFIG)")
GENERATED_DECODER_INPUTS := $(addprefix $(CONFIG)/,cpu6-control-store.rom cpu6-control-store.inc addresses.inc)
SOURCES := cpu6-compiled.cpp cpu6.cpp cpu6-step.cpp backplane.cpp cmd.cpp crt.cpp finch.cpp firmware.cpp hawk.cpp machine.cpp mux.cpp peripherals.cpp terminal.cpp
OBJECTS := $(addprefix $(OUT)/,$(SOURCES:.cpp=.o))
BLOCKS := $(addprefix $(OUT)/cpu6-block-,0.o 1.o 2.o 3.o 4.o 5.o 6.o 7.o)
OBJECTS += $(BLOCKS)
PROBE := $(OUT)/cpu6-probe
BOOT_PROBE := $(OUT)/cpp-hawk-boot
FINCH_PROBE := $(OUT)/cpp-finch-probe
PERIPHERAL_PROBE := $(OUT)/cpp-peripherals-probe
HEADLESS := $(OUT)/centurion-headless.node
NODE_CPPFLAGS ?= -I/usr/include/node
SERVER := $(OUT)/centurion-emulator
SERVER_SOURCES := images.cpp remote_panel.cpp runtime.cpp telnet_codec.cpp telnet_server.cpp web_server.cpp server.cpp
SERVER_OBJECTS := $(addprefix $(OUT)/,$(SERVER_SOURCES:.cpp=.o))

.PHONY: all probes headless FORCE
all: $(SERVER)
probes: $(PROBE) $(BOOT_PROBE) $(FINCH_PROBE) $(PERIPHERAL_PROBE)
headless: $(HEADLESS)

$(PROBE): $(OBJECTS) $(OUT)/cpp-emulator-probe.o
	$(CXX) $(CXXFLAGS) $(ARCHFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BOOT_PROBE): $(OBJECTS) $(OUT)/cpp-hawk-boot.o
	$(CXX) $(CXXFLAGS) $(ARCHFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(FINCH_PROBE): $(OBJECTS) $(OUT)/cpp-finch-probe.o
	$(CXX) $(CXXFLAGS) $(ARCHFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(PERIPHERAL_PROBE): $(OBJECTS) $(OUT)/cpp-peripherals-probe.o
	$(CXX) $(CXXFLAGS) $(ARCHFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(SERVER): $(OBJECTS) $(SERVER_OBJECTS)
	$(CXX) $(CXXFLAGS) $(ARCHFLAGS) $(LDFLAGS) $^ $(LDLIBS) -pthread -o $@

$(HEADLESS): $(OBJECTS) $(OUT)/images.o $(OUT)/headless.o
	$(CXX) $(CXXFLAGS) $(ARCHFLAGS) $(LDFLAGS) -shared $^ $(LDLIBS) -o $@

$(OUT)/headless.o: headless.cpp $(CONFIG)/build-flags | $(OUT)
	$(COMPILE) $(NODE_CPPFLAGS) -c $< -o $@

$(CONFIG)/build-flags $(GENERATED_DECODER_INPUTS) &: FORCE prepare-aot.py | $(OUT)
	python3 prepare-aot.py "$(CONFIG)" "$(FIRMWARE_JSON)" "$(CXX) $(COMPILE_FLAGS) $(NODE_CPPFLAGS) $(CONTROL_STORE)" "$(CONTROL_STORE)"

$(BLOCKS): $(OUT)/cpu6-block-%.o: cpu6-block.cpp $(CONFIG)/build-flags $(GENERATED_DECODER_INPUTS)
	$(COMPILE) $(CONTROL_STORE_FLAGS) -fno-lto -DCPU6_BUILD_BLOCK=$* -c $< -o $@

$(OUT)/cpu6-compiled.o: cpu6-compiled.cpp $(CONFIG)/build-flags $(GENERATED_DECODER_INPUTS)
	$(COMPILE) $(CONTROL_STORE_FLAGS) -c $< -o $@

$(OUT)/%.o: %.cpp $(CONFIG)/build-flags | $(OUT)
	$(COMPILE) -c $< -o $@

$(OUT)/cpp-%.o: $(ROOT)/tests/cpp-%.cpp $(CONFIG)/build-flags | $(OUT)
	$(COMPILE) -I. -c $< -o $@

$(OUT):
	mkdir -p $@

-include $(OBJECTS:.o=.d) $(SERVER_OBJECTS:.o=.d)
-include $(OUT)/headless.d
-include $(OUT)/cpp-emulator-probe.d $(OUT)/cpp-hawk-boot.d $(OUT)/cpp-finch-probe.d
-include $(OUT)/cpp-peripherals-probe.d
