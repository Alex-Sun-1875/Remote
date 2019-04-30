export CCACHE_CPP2=yes
export CCACHE_HARDLINK=true
export CCACHE_SLOPPINESS=file_macro,time_macros,include_file_mtime,include_file_ctime,file_stat_matches
export PATH:=$(HOME)/chromium/src/third_party/llvm-build/Release+Asserts/bin:$(PATH)

OS := $(shell uname -o)
RELEASE_BUILD_PARAM='is_debug=false \
										 is_component_build=true \
										 enable_nacl=false \
										 blink_symbol_level=0 \
										 cc_wrapper="ccache"'

DEBUG_BUILD_PARAM='is_debug=true \
									 is_component_build=true \
									 symbol_level=1 \
									 enable_nacl=false \
									 blink_symbol_level=0 \
									 remove_webcore_debug_symbols=false \
									 cc_wrapper="ccache"'

ifeq (y, $(debug))
	BUILD_TARGET:=Debug
else
	BUILD_TARGET:=Release
endif


ifeq (Darwin, $(OS))
	IDE := --ide=xcode
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
	tg:=chrome
endif

ifeq (, $(test))
	test:=main_test
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
ROOT_DIR := $(shell cd ../../; pwd)

include target.mk
