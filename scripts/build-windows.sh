#!/bin/sh
# Cross-compile a Windows build from Linux (Ubuntu 24.04 tested).
# Output: build-win/pkg/LOTR/ with lotr.exe + SDL DLLs, ready to zip.
#
# Needs: gcc-mingw-w64-x86-64 cmake autoconf automake libtool pkg-config git curl
set -e

SDL_VER=2.30.12
MIX_VER=2.8.1
XML_TAG=v2.13.5

SRC=$(cd "$(dirname "$0")/.." && pwd)
W=$SRC/build-win
P=$W/prefix
HOST=x86_64-w64-mingw32
mkdir -p "$W/dl" "$P"
cd "$W/dl"

[ -f SDL2.tgz ] || curl -sSLf -o SDL2.tgz \
  https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VER/SDL2-devel-$SDL_VER-mingw.tar.gz
[ -f MIX.tgz ] || curl -sSLf -o MIX.tgz \
  https://github.com/libsdl-org/SDL_mixer/releases/download/release-$MIX_VER/SDL2_mixer-devel-$MIX_VER-mingw.tar.gz
tar xzf SDL2.tgz && tar xzf MIX.tgz
cp -r SDL2-$SDL_VER/$HOST/* "$P/" && cp -r SDL2_mixer-$MIX_VER/$HOST/* "$P/"
sed -i "s|^prefix=.*|prefix=$P|" "$P/bin/sdl2-config" "$P"/lib/pkgconfig/*.pc
rm -f "$P"/lib/*.la   # prebuilt .la files carry the SDL CI runner's paths

if [ ! -f "$P/lib/libxml2.a" ]; then
  [ -d libxml2 ] || git clone -q --depth 1 --branch $XML_TAG https://github.com/GNOME/libxml2.git
  cmake -S libxml2 -B libxml2/build -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=$HOST-gcc -DCMAKE_RC_COMPILER=$HOST-windres \
    -DCMAKE_INSTALL_PREFIX="$P" -DBUILD_SHARED_LIBS=OFF \
    -DLIBXML2_WITH_PYTHON=OFF -DLIBXML2_WITH_ZLIB=OFF -DLIBXML2_WITH_LZMA=OFF \
    -DLIBXML2_WITH_ICONV=OFF -DLIBXML2_WITH_ICU=OFF -DLIBXML2_WITH_TESTS=OFF \
    -DLIBXML2_WITH_PROGRAMS=OFF -DLIBXML2_WITH_HTTP=OFF
  cmake --build libxml2/build -j8 --target install
fi

# Build out of a clean copy so a native in-tree build doesn't interfere.
# The copy is a git clone: only committed changes get built.
if [ -n "$(git -C "$SRC" status --porcelain --untracked-files=no)" ]; then
  echo "WARNING: uncommitted changes in $SRC are NOT in this build:" >&2
  git -C "$SRC" status --short --untracked-files=no >&2
fi
rm -rf "$W/src" && git -C "$SRC" worktree prune && git clone -q "$SRC" "$W/src"
cd "$W/src"
autoreconf -fi
PKG_CONFIG_LIBDIR=$P/lib/pkgconfig PKG_CONFIG_PATH=$P/lib/pkgconfig SDL2_CONFIG=$P/bin/sdl2-config \
./configure --host=$HOST --prefix=/ --disable-shared --enable-static \
  CPPFLAGS="-I$P/include -I$P/include/SDL2 -DLIBXML_STATIC" \
  LDFLAGS="-L$P/lib -static-libgcc" LIBS="-lws2_32 -lbcrypt"
make -j8 -C liblotr
make -j8 -C src

D=$W/pkg/LOTR
rm -rf "$W/pkg" && mkdir -p "$D/cd"
cp src/lotr.exe "$D/" && $HOST-strip "$D/lotr.exe"
cp "$P/bin/SDL2.dll" "$P/bin/SDL2_mixer.dll" "$D/"
cp doc/COPYING "$D/COPYING.txt"
cp "$W/dl/SDL2-$SDL_VER/LICENSE.txt" "$D/LICENSE-SDL2.txt"
cp "$W/dl/SDL2_mixer-$MIX_VER/LICENSE.txt" "$D/LICENSE-SDL2_mixer.txt"
cp "$SRC/scripts/README-windows.txt" "$D/README.txt"
printf 'Put the CD edition game files (arts.dat etc.) in this folder.\r\n' > "$D/cd/PUT_GAME_FILES_HERE.txt"
mkdir -p "$D/tools"
cp "$SRC/scripts/extract-data-track.ps1" "$SRC/scripts/extract-music.ps1" "$D/tools/"
echo "Built: $D"
