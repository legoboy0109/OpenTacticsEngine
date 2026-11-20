# Original-Xbox (nxdk) feasibility build. Not part of the normal build.
#   NXDK_DIR=/path/to/nxdk make -C <build dir> -f <repo>/tools/xbox-og/Makefile -j4
# Objects, main.exe, bin/default.xbe and the .iso all land in the build dir (the cwd); nothing is written into the repo.
# See tools/xbox-og/FINDINGS.md.
REPO := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
OUT  := $(CURDIR)/obj

XBE_TITLE = OTE\ OG\ test
GEN_XISO = $(CURDIR)/ote-og-test.iso
NXDK_CXX = y
LTO =

CORE_SRCS := $(shell cd $(REPO) && find engine/core -name '*.cpp' | sort)
LUA_SRCS  := $(filter-out third_party/lua/src/lua.c third_party/lua/src/luac.c, \
               $(shell cd $(REPO) && find third_party/lua/src -name '*.c' | sort))
EXTRA_SRCS := engine/platform/common/bitmap_font.cpp engine/platform/common/bitmap_font_data.c engine/gfx3d/soft3d.cpp engine/gfx3d/shapes.cpp \
               tools/xbox-og/bench3d.cpp tools/xbox-og/nx_compat.c tools/xbox-og/nx_printf.c tools/xbox-og/main.cpp

SRCS := $(addprefix $(OUT)/,$(CORE_SRCS) $(LUA_SRCS) $(EXTRA_SRCS))

# -O2: nxdk's own default is -O0. LUA_USE_C89: no popen/etc. OTE_NXDK: the engine's own switch (core/base/fileio.*).
NXDK_CFLAGS   += -include $(REPO)/tools/xbox-og/nx_compat.h -O2 -std=c99 -DOTE_NXDK=1 -DLUA_USE_C89 -I$(REPO)/third_party/lua/src -w
NXDK_CXXFLAGS += -include $(REPO)/tools/xbox-og/nx_compat.h -O2 -std=c++17 -fno-rtti -DOTE_NXDK=1 -I$(REPO)/engine -I$(REPO)/tools/xbox-og -I$(REPO)/third_party/lua/src

# 64 KB default stack is too small: the engine keeps 64 KB read buffers on the stack ("Stack overflow caught").
NXDK_LDFLAGS += -stack:1048576

include $(NXDK_DIR)/Makefile

$(OUT)/%.obj: $(REPO)/%.cpp
	@mkdir -p $(dir $@)
	@echo "[ CXX      ] $*.cpp"
	$(VE) $(CXX) $(NXDK_CXXFLAGS) $(CXXFLAGS) -MD -MP -MT '$@' -MF '$(patsubst %.obj,%.cpp.d,$@)' -c -o '$@' '$<'

$(OUT)/%.obj: $(REPO)/%.c
	@mkdir -p $(dir $@)
	@echo "[ CC       ] $*.c"
	$(VE) $(CC) $(NXDK_CFLAGS) $(CFLAGS) -MD -MP -MT '$@' -MF '$(patsubst %.obj,%.c.d,$@)' -c -o '$@' '$<'

# The disc: the XBE plus the demo game (only demo-* games may ever go here).
$(GEN_XISO): stage-disc
.PHONY: stage-disc
stage-disc: $(OUTPUT_DIR)/default.xbe
	@mkdir -p $(OUTPUT_DIR)/games
	@rsync -a --delete $(REPO)/games/demo-srpg/ $(OUTPUT_DIR)/games/demo-srpg/
