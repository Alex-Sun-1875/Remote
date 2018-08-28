.PHONY: generate configure build

all: generate configure build

overlay:

force_overlay:

copy:

generate:
	@echo "================ Generate Build File Start =================";
	@cd $(ROOT_DIR); \
	if [ ! -d $(BUILD_DIR) ]; then \
		gn gen $(BUILD_DIR); \
	else	\
		gn gen $(BUILD_DIR); \
	fi
	@echo "================= Generate Build File End ==================";

configure:

prebuild:

build_target:
	@echo "======================= Build Start ========================";
	#ninja -C $(ROOT_DIR)/$(BUILD_DIR) $(tg) $(MAKE_OPTIONS)
	autoninja -C $(ROOT_DIR)/$(BUILD_DIR) $(tg)
	@echo "======================== Build End =========================";

install:

build: build_target install

clean:
	@echo "===================== Make Clean Start =====================";
	rm -rf $(ROOT_DIR)/$(BUILD_DIR)/$(tg)
	@echo "====================== Make Clean End ======================";

