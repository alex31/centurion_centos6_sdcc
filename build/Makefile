ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
OUT := $(ROOT)/build/out
SEED ?= $(OUT)/CENTOS_13.IMG
JOBS ?= 4
PYTHON ?= python3

.PHONY: all submodules toolchain emulator basic basic-image check clean
all: toolchain emulator

submodules:
	git -C "$(ROOT)" submodule update --init --recursive

toolchain:
	git -C "$(ROOT)" submodule update --init -- swext/sdcc swext/fuzix-bintools swext/libee200
	$(PYTHON) "$(ROOT)/build/build-toolchain.py" --jobs "$(JOBS)" --cc "$(CC)"

emulator:
	git -C "$(ROOT)" submodule update --init -- swext/centurion-js
	"$(ROOT)/build/build-emulator.sh"

basic: toolchain
	git -C "$(ROOT)" submodule update --init -- swext/stefan-basic
	$(PYTHON) "$(ROOT)/swext/stefan-basic/centos/build.py" --commands --stack-size 1536 -o "$(OUT)/basic/BASIC.exf"

basic-image: basic emulator
	$(PYTHON) "$(ROOT)/swext/stefan-basic/centos/install.py" "$(OUT)/basic" --seed "$(SEED)" -o "$(OUT)/basic/centos-basic.img"

check: all
	mkdir -p "$(OUT)/check"
	"$(ROOT)/run/cpu6-bare-sdcc" --gc-sections "$(ROOT)/build/smoke.c" -o "$(OUT)/check/smoke.bin"
	node "$(ROOT)/build/check-core.cjs" "$(OUT)/check" "$(ROOT)/build/smoke.json"
	"$(ROOT)/run/cpu6-os6-sdcc" --lto --gc-sections "$(ROOT)/examples/hello_os6.c" -o "$(OUT)/check/hello.exf"

clean:
	rm -rf -- "$(OUT)"
