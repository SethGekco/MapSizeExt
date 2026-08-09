#!/usr/bin/env bash
# Build the curated MapSizeExt DLL (Krisztiaan's source + our 300x300 fixes).
# Local mingw, no Docker/MSVC. Produces MapSizeExt.dll.
set -e
cd "$(dirname "$0")"
i686-w64-mingw32-g++ -std=gnu++11 -shared -static-libgcc -Wl,--enable-stdcall-fixup \
  -o MapSizeExt.dll yr_map_512_plane_probe.c -lpsapi
echo "built: $(sha256sum MapSizeExt.dll | cut -c1-16)"
