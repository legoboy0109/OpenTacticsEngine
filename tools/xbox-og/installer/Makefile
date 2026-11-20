# The test installer (installer/main.cpp) and the two discs made from it. Run through build.sh --test-iso.
#   APP_DIR=<staged OpenTacticsEngine folder> [ARGS_FILE=<test script>] [EXTRA_DIR=<folder merged into the payload>] NXDK_DIR=... make -f <repo>/tools/xbox-og/installer/Makefile
# Results in the cwd: ote-installer.iso (installs the app folder, then starts it) and ote-launcher.iso (starts what is
# already installed; tiny, for a second boot with the same hard disk).
SELF := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
OUT  := $(CURDIR)/obj

XBE_TITLE = OTE\ installer
GEN_XISO = $(CURDIR)/ote-installer.iso
NXDK_CXX = y
LTO =
SRCS := $(OUT)/installer-main.obj

NXDK_CXXFLAGS += -O2 -std=c++17 -fno-rtti
NXDK_LDFLAGS += -stack:262144

include $(NXDK_DIR)/Makefile

$(OUT)/installer-main.obj: $(SELF)/main.cpp
	@mkdir -p $(dir $@)
	@echo "[ CXX      ] installer/main.cpp"
	$(VE) $(CXX) $(NXDK_CXXFLAGS) $(CXXFLAGS) -c -o '$@' '$<'

# Disc contents: bin/ (the XBE, mode.txt, payload/) is what nxdk packs into ote-installer.iso.
$(GEN_XISO): stage-disc
.PHONY: stage-disc
stage-disc: $(OUTPUT_DIR)/default.xbe
	@test -n "$(APP_DIR)" || { echo "APP_DIR is not set"; exit 1; }
	@rm -rf $(OUTPUT_DIR)/payload && mkdir -p $(OUTPUT_DIR)/payload && cp -R $(APP_DIR)/. $(OUTPUT_DIR)/payload/
	@if [ -n "$(EXTRA_DIR)" ]; then cp -R "$(EXTRA_DIR)"/. $(OUTPUT_DIR)/payload/; fi
	@if [ -n "$(ARGS_FILE)" ]; then cp "$(ARGS_FILE)" $(OUTPUT_DIR)/payload/args.txt; fi
	@printf 'install' > $(OUTPUT_DIR)/mode.txt
	@rm -rf $(CURDIR)/launcher-disc && mkdir -p $(CURDIR)/launcher-disc
	@cp $(OUTPUT_DIR)/default.xbe $(CURDIR)/launcher-disc/ && printf 'launch' > $(CURDIR)/launcher-disc/mode.txt
	@$(EXTRACT_XISO) -c $(CURDIR)/launcher-disc $(CURDIR)/ote-launcher.iso >/dev/null
	@rm -rf $(CURDIR)/dash-disc && mkdir -p $(CURDIR)/dash-disc
	@cp $(OUTPUT_DIR)/default.xbe $(CURDIR)/dash-disc/ && printf 'dash' > $(CURDIR)/dash-disc/mode.txt
	@$(EXTRACT_XISO) -c $(CURDIR)/dash-disc $(CURDIR)/ote-dash.iso >/dev/null
