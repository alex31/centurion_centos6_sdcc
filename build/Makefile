.DEFAULT_GOAL := sdk
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
OUT := $(ROOT)/build/out
SEED ?= $(OUT)/image/CENTOS_13.IMG
JOBS ?= 4
PYTHON ?= python3
FIRMWARE_JSON ?= $(if $(CENTURION_FIRMWARE),$(abspath $(CENTURION_FIRMWARE)),$(OUT)/emulator/firmware.json)

.PHONY: all sdk submodules toolchain emulator emulator-cpp-sources emulator-cpp emulator-cpp-headless basic basic-image clean
all: apps-image
sdk: toolchain emulator

submodules:
	git -C "$(ROOT)" submodule update --init --recursive

toolchain:
	git -C "$(ROOT)" submodule update --init -- swext/sdcc swext/fuzix-bintools
	$(PYTHON) "$(ROOT)/build/build-toolchain.py" --jobs "$(JOBS)" --cc "$(CC)"

emulator:
	git -C "$(ROOT)" submodule update --init -- swext/centurion-js
	"$(ROOT)/build/build-emulator.sh"

emulator-cpp-sources:
	test -f "$(ROOT)/emulator/Makefile" || git -C "$(ROOT)" submodule update --init -- emulator

emulator-cpp: emulator-cpp-sources
	$(MAKE) -C "$(ROOT)/emulator" OUT="$(OUT)/emulator-cpp" FIRMWARE_JSON="$(FIRMWARE_JSON)" all probes

emulator-cpp-headless: emulator-cpp-sources
	$(MAKE) -C "$(ROOT)/emulator" OUT="$(OUT)/emulator-cpp" FIRMWARE_JSON="$(FIRMWARE_JSON)" headless

basic: toolchain
	git -C "$(ROOT)" submodule update --init -- swext/stefan-basic
	$(PYTHON) "$(ROOT)/swext/stefan-basic/centos/build.py" --commands --vm-math --profile speed --stack-size 1536 --memory 5632 -o "$(OUT)/basic/BASIC.exf"

basic-image: basic emulator
	git -C "$(ROOT)" submodule update --init -- swext/101-basic-games
	$(PYTHON) "$(ROOT)/image/install-basic.py" "$(OUT)/basic" --seed "$(SEED)" -o "$(OUT)/basic/centos-basic.img"

clean:
	rm -rf -- "$(OUT)"

include $(ROOT)/tests/targets.mk

include $(ROOT)/image/targets.mk
