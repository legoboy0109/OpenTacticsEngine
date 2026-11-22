# Toolchain file for the big-endian proof (docs/XBOX360.md "Big-endian proof"): the engine core and ote_tests built for
# 64-bit big-endian PowerPC Linux (bootlin's powerpc64-power8 glibc toolchain), run under qemu-user. The Xbox 360, PS3 and
# Wii U are big-endian; this is the cheap way to find byte-order bugs with no console toolchain involved.
# tools/xbox360/bigendian_check.sh drives it (it passes -DOTE_BIGENDIAN_TC=<the unpacked toolchain folder>).
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR ppc64)
set(_p "${OTE_BIGENDIAN_TC}/bin/powerpc64-linux-")
set(CMAKE_C_COMPILER "${_p}gcc")
set(CMAKE_CXX_COMPILER "${_p}g++")
# Static: no sysroot lookups under qemu-user.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
