# Link attempt for the REAL player (engine/player + SDL2 backend) against nxdk's SDL2 2.0.9. Evidence only.
#   NXDK_DIR=/path/to/nxdk make -C <build dir> -f <repo>/tools/xbox-og/Makefile.player -j4
REPO := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
OUT  := $(CURDIR)/obj-player

XBE_TITLE = OTE\ OG\ player
GEN_XISO = $(CURDIR)/ote-og-player.iso
NXDK_CXX = y
NXDK_SDL = y
LTO =

find = $(shell cd $(REPO) && find $(1) -name '$(2)' | sort)
CORE_SRCS   := $(call find,engine/core,*.cpp)
LUA_SRCS    := $(filter-out third_party/lua/src/lua.c third_party/lua/src/luac.c,$(call find,third_party/lua/src,*.c))
GFX3D_SRCS  := $(wildcard $(REPO)/engine/gfx3d/*.cpp)
GFX3D_SRCS  := $(patsubst $(REPO)/%,%,$(GFX3D_SRCS))
AUDIO_SRCS  := $(call find,engine/audio,*.cpp) engine/audio/stb_vorbis_impl.c
COMMON_SRCS := $(call find,engine/platform/common,*.cpp) engine/platform/common/bitmap_font_data.c
# The SDL2 backend minus the hardware-3D glue (OpenGL / Direct3D 11 / UWP): the software 3D drawer is used.
BACKEND_SRCS := engine/platform/sdl2/sdl2_app.cpp engine/platform/sdl2/sdl2_audio.cpp engine/platform/sdl2/sdl2_files.cpp \
                engine/platform/sdl2/sdl2_input.cpp engine/platform/sdl2/sdl2_renderer.cpp tools/xbox-og/stub_hw3d.cpp tools/xbox-og/nx_compat.c
PLAYER_SRCS := $(call find,engine/player,*.cpp)

SRCS := $(addprefix $(OUT)/,$(CORE_SRCS) $(LUA_SRCS) $(GFX3D_SRCS) $(AUDIO_SRCS) $(COMMON_SRCS) $(BACKEND_SRCS) $(PLAYER_SRCS))

NXDK_CFLAGS   += -include $(REPO)/tools/xbox-og/nx_compat.h -O2 -std=c99 -DOTE_NXDK=1 -DLUA_USE_C89 -I$(REPO)/third_party/lua/src -I$(REPO)/third_party/stb -w
NXDK_CXXFLAGS += -include $(REPO)/tools/xbox-og/nx_compat.h -O2 -std=c++17 -fno-rtti -DOTE_NXDK=1 -DOTE_PLATFORM_SDL2=1 -DOTE_NO_CONSOLE_API=1 -DSTBI_NO_SIMD=1 \
                 -I$(REPO)/engine -I$(REPO)/engine/player -I$(REPO)/third_party -I$(REPO)/third_party/stb -I$(REPO)/third_party/lua/src

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

# The disc: the XBE plus assets/ and the demo games from games/index.json (only demo-* games may ever go here).
$(GEN_XISO): stage-disc
.PHONY: stage-disc
stage-disc: $(OUTPUT_DIR)/default.xbe
	@mkdir -p $(OUTPUT_DIR)/games
	@rsync -a --delete $(REPO)/assets/ $(OUTPUT_DIR)/assets/
	@cp $(REPO)/games/index.json $(OUTPUT_DIR)/games/index.json
	@for g in demo-srpg demo-economy demo-hex; do rsync -a --delete $(REPO)/games/$$g/ $(OUTPUT_DIR)/games/$$g/; done
