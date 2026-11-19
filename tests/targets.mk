TEST_JOBS ?= 2
WEB_SEED ?=
TESTS := $(ROOT)/tests
TEST_OUT := $(OUT)/tests
C_EMULATOR := $(ROOT)/swext/centurion-c

.PHONY: check tests alltests test-c-emulator tests-centos tests-web

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
	$(PYTHON) "$(TESTS)/sdcc-cpu6-regression.py"

alltests: tests tests-web
	$(PYTHON) "$(TESTS)/run-suites.py" --jobs "$(TEST_JOBS)"

tests-centos: check
	@work=$$(mktemp -d "$(TEST_OUT)/centos.XXXXXX") && \
		"$(ROOT)/run/cpu6-os6-install" "$(SEED)" "$(TEST_OUT)/smoke/hello.exf" XHELLO6 "$$work/centos.img" && \
		node "$(TESTS)/centos.js" "$$work/centos.img"

tests-web: emulator
	node "$(TESTS)/web-console.js" $(if $(WEB_SEED),"$(WEB_SEED)")
