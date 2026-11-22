ROOT := $(abspath ../../..)
OUT := $(ROOT)/build/out/chomp
SEED ?= $(ROOT)/build/out/image/centos-apps.img
CHOMP_IMAGE ?= $(OUT)/centos-chomp.img
PYTHON ?= python3

.DELETE_ON_ERROR:
.PHONY: all image
all: $(OUT)/XCHOMP.exf

$(OUT)/chomp_table.h: chomp_table.py
	mkdir -p "$(OUT)"
	$(PYTHON) "$<" "$@"

$(OUT)/XCHOMP.exf: chomp.c $(OUT)/chomp_table.h
	"$(ROOT)/run/cpu6-os6-sdcc" -O1 --lto --gc-sections -I "$(OUT)" "$<" -o "$@"

image: $(OUT)/XCHOMP.exf
	$(PYTHON) "$(ROOT)/scripts/install-os6-bundle.py" "$(SEED)" "XCHOMP=$<" -o "$(CHOMP_IMAGE)"
