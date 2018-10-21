.PHONY: generate configure build

all: generate configure build

overlay:

force_overlay:

copy:

generate:
	@echo "================ Generate Build File Start =================";
	@cd $(ROOT_DIR); \
	if [ ! -d $(BUILD_DIR) ]; then \
		if [ "Release" == "$(BUILD_TARGET)" ]; then \
			gn gen $(BUILD_DIR) --args=$(RELEASE_BUILD_PARAM); \
		else \
			gn gen $(BUILD_DIR); \
		fi \
	else	\
		gn gen $(BUILD_DIR); \
	fi
	@echo "================= Generate Build File End ==================";

configure:
	@echo "============= Configure Build Parameter Start ==============";
	@echo "============== Configure Build Parameter End ===============";

prebuild:

build_target:
	@echo "======================= Build Start ========================";
	@#ninja -C $(ROOT_DIR)/$(BUILD_DIR) $(tg) $(MAKE_OPTIONS)
	autoninja -C $(ROOT_DIR)/$(BUILD_DIR) $(tg)
	@echo "======================== Build End =========================";

build_test:
	@if [ $(tg) == "remote_main" ]; then \
		echo "===================== Build Test Start ====================="; \
		autoninja -C $(ROOT_DIR)/$(BUILD_DIR) $(test); \
		echo "====================== Build Test End ======================"; \
	fi

install:

build: build_target build_test install

clean:
	@echo "===================== Make Clean Start =====================";
	@#rm -rf $(ROOT_DIR)/$(BUILD_DIR)/$(tg)
	rm -rf $(ROOT_DIR)/out
	@echo "====================== Make Clean End ======================";

