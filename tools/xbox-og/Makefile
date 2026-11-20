# The OpenTacticsEngine player for the original Xbox (2001), built with nxdk. Run through build.sh, which stages the app
# folder (docs/XBOX_OG.md); by hand:
#   NXDK_DIR=/path/to/nxdk PATH=$NXDK_DIR/bin:$PATH make -C <build dir> -f <repo>/tools/xbox-og/Makefile -j4
# Objects and bin/default.xbe land in the build dir (the cwd); nothing is written into the repo.
REPO := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
OUT  := $(CURDIR)/obj

XBE_TITLE = OpenTactics
NXDK_CXX = y
NXDK_SDL = y
LTO =

find = $(shell cd $(REPO) && find $(1) -name '$(2)' | sort)
CORE_SRCS    := $(call find,engine/core,*.cpp)
LUA_SRCS     := $(filter-out third_party/lua/src/lua.c third_party/lua/src/luac.c,$(call find,third_party/lua/src,*.c))
GFX3D_SRCS   := $(patsubst $(REPO)/%,%,$(wildcard $(REPO)/engine/gfx3d/*.cpp))
AUDIO_SRCS   := $(call find,engine/audio,*.cpp) engine/audio/stb_vorbis_impl.c
COMMON_SRCS  := $(call find,engine/platform/common,*.cpp) engine/platform/common/bitmap_font_data.c
NXDK_SRCS    := $(patsubst $(REPO)/%,%,$(wildcard $(REPO)/engine/platform/nxdk/*.cpp))
# The SDL2 backend; the OpenGL / Direct3D 11 / UWP glue is not built (nxdk_hw3d.cpp stands in for the 3D glue).
BACKEND_SRCS := engine/platform/sdl2/sdl2_app.cpp engine/platform/sdl2/sdl2_audio.cpp engine/platform/sdl2/sdl2_files.cpp \
                engine/platform/sdl2/sdl2_input.cpp engine/platform/sdl2/sdl2_renderer.cpp tools/xbox-og/nx_compat.c tools/xbox-og/nx_printf.c tools/xbox-og/nx_mem.c
PLAYER_SRCS  := $(call find,engine/player,*.cpp)

SRCS := $(addprefix $(OUT)/,$(CORE_SRCS) $(LUA_SRCS) $(GFX3D_SRCS) $(AUDIO_SRCS) $(COMMON_SRCS) $(NXDK_SRCS) $(BACKEND_SRCS) $(PLAYER_SRCS))

# -O2: nxdk's own default is -O0. LUA_USE_C89: no popen/etc. OTE_NXDK: the engine's own switch for the Xbox build.
# nx_compat.h (force-included) renames the libc functions nxdk stubs out with assert(0); STBI_NO_SIMD: the CPU has SSE
# but not SSE2, which stb_image would use. OTE_NO_CONSOLE_API: no console to attach to (console.cpp). OTE_SDL2_UPLOAD_ARGB8888: SDL's
# textures are made in the 2D layer's own format (ARGB8888), so that its blitters need no per-pixel conversion (docs/XBOX_OG.md "Speed").
NXDK_CFLAGS   += -include $(REPO)/tools/xbox-og/nx_compat.h -O2 -std=c99 -DOTE_NXDK=1 -DLUA_USE_C89 -I$(REPO)/third_party/lua/src -I$(REPO)/third_party/stb -w
NXDK_CXXFLAGS += -include $(REPO)/tools/xbox-og/nx_compat.h -O2 -std=c++17 -fno-rtti -DOTE_NXDK=1 -DOTE_PLATFORM_SDL2=1 -DOTE_NO_CONSOLE_API=1 -DOTE_SDL2_UPLOAD_ARGB8888=1 \
                 -DSTBI_NO_SIMD=1 -I$(REPO)/engine -I$(REPO)/engine/player -I$(REPO)/third_party -I$(REPO)/third_party/stb \
                 -I$(REPO)/third_party/lua/src -Wall -Wextra -Wno-unused-parameter

# The default 64 KB stack is too small: the engine keeps 64 KB read buffers on the stack ("Stack overflow caught").
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
