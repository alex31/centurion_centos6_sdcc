APP_IMAGE ?= $(OUT)/image/centos-apps.img
CENTURION_OS6_BACKEND ?= cpp
ifeq ($(CENTURION_OS6_BACKEND),javascript)
IMAGE_EMULATOR := emulator
else
IMAGE_EMULATOR := emulator-cpp-headless
endif

.PHONY: apps-image
apps-image: toolchain $(IMAGE_EMULATOR)
	git -C "$(ROOT)" submodule update --init -- swext/stefan-basic swext/mcu-max swext/ekermit swext/101-basic-games swext/centurion-js
	CENTURION_OS6_BACKEND="$(CENTURION_OS6_BACKEND)" $(PYTHON) "$(ROOT)/image/build.py" --seed "$(SEED)" --output "$(APP_IMAGE)"
