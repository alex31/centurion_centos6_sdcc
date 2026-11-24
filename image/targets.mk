APP_IMAGE ?= $(OUT)/image/centos-apps.img
CENTURION_OS6_BACKEND ?= auto

.PHONY: apps-image assemble-apps-image
apps-image: toolchain
	+@set -e; \
	backend="$(CENTURION_OS6_BACKEND)"; \
	case "$$backend" in \
	    auto) \
	        if $(MAKE) -C "$(ROOT)" emulator-cpp-headless; then \
	            backend=cpp; \
	        else \
	            printf '%s\n' 'C++ emulator build failed. Continuing with the JavaScript emulator.' >&2; \
	            backend=javascript; \
	        fi ;; \
	    cpp) $(MAKE) -C "$(ROOT)" emulator-cpp-headless ;; \
	    javascript) ;; \
	    *) printf '%s\n' 'CENTURION_OS6_BACKEND must be auto, cpp or javascript.' >&2; exit 2 ;; \
	esac; \
	$(MAKE) -C "$(ROOT)" assemble-apps-image CENTURION_OS6_BACKEND="$$backend"

assemble-apps-image: $(if $(filter javascript,$(CENTURION_OS6_BACKEND)),emulator)
	git -C "$(ROOT)" submodule update --init -- swext/stefan-basic swext/mcu-max swext/ekermit swext/101-basic-games swext/centurion-js
	CENTURION_OS6_BACKEND="$(CENTURION_OS6_BACKEND)" $(PYTHON) "$(ROOT)/image/build.py" --seed "$(SEED)" --output "$(APP_IMAGE)"
