.DEFAULT_GOAL := sdk
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
OUT := $(ROOT)/build/out
SEED ?= $(OUT)/image/CENTOS_13.IMG
JOBS ?= 4
PYTHON ?= python3

.PHONY: all sdk submodules toolchain emulator basic basic-image clean
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

basic: toolchain
	git -C "$(ROOT)" submodule update --init -- swext/stefan-basic
	$(PYTHON) "$(ROOT)/swext/stefan-basic/centos/build.py" --commands --vm-math --profile size --no-auto-inline --stack-size 1536 -o "$(OUT)/basic/BASIC.exf"

basic-image: basic emulator
	$(PYTHON) "$(ROOT)/swext/stefan-basic/centos/install.py" "$(OUT)/basic" --seed "$(SEED)" \
		--example "LIFE1D=$(ROOT)/image/programs/life1d/life1d.bas" -o "$(OUT)/basic/centos-basic.img"

clean:
	rm -rf -- "$(OUT)"

include $(ROOT)/tests/targets.mk

include $(ROOT)/image/targets.mk
