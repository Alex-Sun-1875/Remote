HOST_NAME=$(shell hostname)
RELEASE_BUILD_PARAM="is_debug = false  \
						 is_component_build = true \
						 symbol_level = 0 \
						 enable_nacl = false \
						 remove_webcore_debug_symbols = true \
						 cc_wrapper=\"ccache\""


ifeq (y, $(DEBUG))
	BUILD_TARGET:=Debug
else
	BUILD_TARGET:=Release
endif

ifeq (, $(MAKE_OPTIONS))
ifeq (mac, $(SYSTEM))
	MAKE_OPTIONS=-j4
else
	MAKE_OPTIONS=-j4
endif
endif

ifeq (, $(CCACHE))
	CCACHE=y
endif

ifeq (, $(tg))
	tg:=remote_main
endif

ifeq ($(CCACHE), y)
	CC 			:=ccache $(CC)
	CXX			:=ccache $(CXX)
	CPP			:=ccache $(CPP)
	LD 			:=ccache $(LD)
	AR 			:=ccache $(AR)
	AS 			:=ccache $(AS)
	NM 			:=ccache $(NM)
	STRIP		:=ccache $(STRIP)
	OBJCOPY	:=ccache $(OBJCOPY)
	OBJDUMP :=ccache $(OBJDUMP)
	RANLIB	:=ccache $(RANLIB)
endif

BUILD_DIR	:=out/$(BUILD_TARGET)
PWD := $(shell pwd)
ROOT_DIR := $(shell cd ../; pwd)

include target.mk
