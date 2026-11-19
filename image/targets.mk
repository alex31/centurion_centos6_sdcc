APP_IMAGE ?= $(OUT)/image/centos-apps.img

.PHONY: apps-image
apps-image: sdk
	git -C "$(ROOT)" submodule update --init -- swext/stefan-basic swext/mcu-max swext/ekermit
	$(PYTHON) "$(ROOT)/image/build.py" --seed "$(SEED)" --output "$(APP_IMAGE)"
