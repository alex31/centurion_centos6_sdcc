CXX = g++-15
CPPFLAGS ?=
CXXFLAGS ?= -O3 -flto -Wall -Wextra -Wpedantic
ARCHFLAGS ?= -march=native
ROOT := $(abspath ..)
OUT ?= $(ROOT)/build/out/emulator-cpp
FIRMWARE_JSON ?= $(ROOT)/build/out/emulator/firmware.json
CONFIG := $(OUT)/config
COMPILE_FLAGS := $(CPPFLAGS) $(CXXFLAGS) $(ARCHFLAGS) -std=c++26 -fPIC
COMPILE := $(CXX) $(COMPILE_FLAGS) -MMD -MP -I$(CONFIG)
EMBED := --embed-dir="$(CONFIG)"
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

$(CONFIG)/build-flags $(CONFIG)/cpu6-control-store.rom $(CONFIG)/addresses.inc &: FORCE prepare-aot.py | $(OUT)
	python3 prepare-aot.py "$(CONFIG)" "$(FIRMWARE_JSON)" "$(CXX) $(COMPILE_FLAGS) $(NODE_CPPFLAGS)"

$(BLOCKS): $(OUT)/cpu6-block-%.o: cpu6-block.cpp $(CONFIG)/build-flags $(CONFIG)/cpu6-control-store.rom $(CONFIG)/addresses.inc
	$(COMPILE) $(EMBED) -fno-lto -DCPU6_BUILD_BLOCK=$* -c $< -o $@

$(OUT)/cpu6-compiled.o: cpu6-compiled.cpp $(CONFIG)/build-flags $(CONFIG)/cpu6-control-store.rom $(CONFIG)/addresses.inc
	$(COMPILE) $(EMBED) -c $< -o $@

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
