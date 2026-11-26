TEST_JOBS ?= 2
BASIC_BENCHMARKS ?= BM1,BM2
WEB_SEED ?= $(wildcard $(APP_IMAGE))
TESTS := $(ROOT)/tests
TEST_OUT := $(OUT)/tests
C_EMULATOR := $(ROOT)/swext/centurion-c

.PHONY: check tests alltests test-c-emulator tests-centos tests-web tests-games tests-finch tests-emulator-cpp bench-basic

test-c-emulator:
	git -C "$(ROOT)" submodule update --init -- swext/centurion-c
	mkdir -p "$(OUT)/test-c-emulator"
	$(MAKE) -C "$(OUT)/test-c-emulator" -f "$(C_EMULATOR)/Makefile" VPATH="$(C_EMULATOR)" centurion

check: sdk
	mkdir -p "$(TEST_OUT)/smoke"
	"$(ROOT)/run/cpu6-bare-sdcc" --gc-sections "$(TESTS)/smoke.c" -o "$(TEST_OUT)/smoke/smoke.bin"
	node "$(TESTS)/compiler-audit-core.js" "$(TEST_OUT)/smoke" "$(TESTS)/smoke.json"
	"$(ROOT)/run/cpu6-os6-sdcc" --lto --gc-sections "$(ROOT)/examples/hello_os6.c" -o "$(TEST_OUT)/smoke/hello.exf"

tests: check test-c-emulator
	mkdir -p "$(TEST_OUT)/compiler"
	"$(ROOT)/run/sdcc" --std=c99 --opt-code-speed -S "$(TESTS)/sdcc-stale-use.c" -o "$(TEST_OUT)/compiler/sdcc-stale-use.s" > "$(TEST_OUT)/compiler/sdcc-stale-use.log" 2>&1 || { cat "$(TEST_OUT)/compiler/sdcc-stale-use.log"; exit 1; }
	$(PYTHON) "$(TESTS)/sdcc-cpu6-regression.py"

alltests: tests tests-web
	$(PYTHON) "$(TESTS)/run-suites.py" --jobs "$(TEST_JOBS)"

tests-centos: check
	@work=$$(mktemp -d "$(TEST_OUT)/centos.XXXXXX") && \
		"$(ROOT)/run/cpu6-os6-install" "$(SEED)" "$(TEST_OUT)/smoke/hello.exf" XHELLO6 "$$work/centos.img" && \
		node "$(TESTS)/centos.js" "$$work/centos.img"
	$(PYTHON) "$(TESTS)/libc-stdlib.py" --centos "$(SEED)"

tests-web: emulator
	node "$(TESTS)/web-console.js" $(if $(WEB_SEED),"$(WEB_SEED)")

tests-games: emulator
	node "$(TESTS)/games-vt100.js" "$(APP_IMAGE)"
	node "$(TESTS)/wumpus.js" "$(APP_IMAGE)"

tests-finch: sdk
	@mkdir -p "$(TEST_OUT)" && work=$$(mktemp -d "$(TEST_OUT)/finch.XXXXXX") && \
		"$(ROOT)/run/cpu6-bare-sdcc" --gc-sections "$(TESTS)/finch.c" -o "$$work/finch.bin" && \
		node "$(TESTS)/finch.js" "$$work/finch.bin"

# Run after generating the application image and building the JavaScript reference.
tests-emulator-cpp: emulator-cpp
	@mkdir -p "$(TEST_OUT)" && work=$$(mktemp -d "$(TEST_OUT)/emulator-cpp.XXXXXX") && \
		trap 'rm -rf -- "$$work"' EXIT && \
		firmware="$${CENTURION_FIRMWARE:-$(INPUTS)/firmware.json}" && \
		spec="$(ROOT)/swext/centurion-js/src/firmware-spec.json" && \
		"$(ROOT)/run/cpu6-bare-sdcc" --gc-sections "$(TESTS)/cmd-mmio.c" -o "$$work/cmd.bin" && \
		"$(OUT)/emulator-cpp/cpp-peripherals-probe" "$$firmware" "$$spec" "$$work/cmd.bin" && \
		"$(ROOT)/run/cpu6-bare-sdcc" --gc-sections "$(TESTS)/finch.c" -o "$$work/finch.bin" && \
		$(PYTHON) "$(TESTS)/compare-finch.py" "$(OUT)/emulator-cpp/cpp-finch-probe" "$$firmware" "$$spec" "$$work/finch.bin"
	$(PYTHON) "$(TESTS)/cpp-telnet.py" --image "$(APP_IMAGE)" --kermit --format-data
	node "$(TESTS)/web-console.js" --native "$(APP_IMAGE)"
	node "$(TESTS)/apps-image.js" "$(APP_IMAGE)" 44 --compare

bench-basic: emulator
	node "$(TESTS)/basic-benchmark.js" "$(APP_IMAGE)" "" "$(BASIC_BENCHMARKS)"
