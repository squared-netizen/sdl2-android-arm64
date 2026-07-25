#!/data/data/com.termux/files/usr/bin/bash

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$ROOT/app/src/main/cpp"
BUILD_DIR="$ROOT/native-build"
OUTPUT_DIR="$ROOT/app/src/main/jniLibs/arm64-v8a"
SDL2_ROOT="$ROOT/third_party/SDL2"
SDL2_INCLUDE_DIR="$SDL2_ROOT/include/SDL2"
SDL2_LIBRARY_DIR="$SDL2_ROOT/lib/arm64-v8a"
CXX_RUNTIME="$PREFIX/lib/libc++_shared.so"
BUILD_TYPE="${NATIVE_BUILD_TYPE:-Debug}"
JOBS="${NATIVE_JOBS:-2}"

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

case "${1:-}" in
    "")
        ;;
    --clean)
        ;;
    *)
        fail "Usage: ./build-native.sh [--clean]"
        ;;
esac

command -v cmake >/dev/null 2>&1 || \
    fail "cmake was not found"

command -v ninja >/dev/null 2>&1 || \
    fail "ninja was not found"

command -v clang++ >/dev/null 2>&1 || \
    fail "clang++ was not found"

command -v file >/dev/null 2>&1 || \
    fail "file was not found"

for REQUIRED_HEADER in \
    SDL.h \
    SDL_ttf.h \
    SDL_mixer.h \
    SDL_image.h \
    SDL_net.h
do
    [[ -f "$SDL2_INCLUDE_DIR/$REQUIRED_HEADER" ]] || \
        fail "Required header is missing: $SDL2_INCLUDE_DIR/$REQUIRED_HEADER"
done

for REQUIRED_LIBRARY in \
    libSDL2.so \
    libSDL2_ttf.so \
    libSDL2_mixer.so \
    libSDL2_image.so \
    libSDL2_net.so
do
    [[ -f "$SDL2_LIBRARY_DIR/$REQUIRED_LIBRARY" ]] || \
        fail "Required library is missing: $SDL2_LIBRARY_DIR/$REQUIRED_LIBRARY"
done

[[ -f "$CXX_RUNTIME" ]] || \
    fail "C++ runtime not found: $CXX_RUNTIME"

if [[ "${1:-}" == "--clean" && -d "$BUILD_DIR" ]]; then
    BACKUP_DIR="$ROOT/native-build.old-$(date +%Y%m%d-%H%M%S)"
    mv "$BUILD_DIR" "$BACKUP_DIR"
    echo "Previous native build moved to: $BACKUP_DIR"
fi

mkdir -p "$BUILD_DIR" "$OUTPUT_DIR"

cmake \
    -S "$SOURCE_DIR" \
    -B "$BUILD_DIR" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_CXX_COMPILER="$PREFIX/bin/clang++" \
    -DCMAKE_LIBRARY_OUTPUT_DIRECTORY="$OUTPUT_DIR" \
    -DSDL2_ROOT="$SDL2_ROOT"

cmake --build "$BUILD_DIR" --parallel "$JOBS"

for SDL_LIBRARY in \
    libSDL2.so \
    libSDL2_ttf.so \
    libSDL2_mixer.so \
    libSDL2_image.so \
    libSDL2_net.so
do
    cp \
        "$SDL2_LIBRARY_DIR/$SDL_LIBRARY" \
        "$OUTPUT_DIR/$SDL_LIBRARY"
done

cp "$CXX_RUNTIME" "$OUTPUT_DIR/libc++_shared.so"

[[ -f "$OUTPUT_DIR/libmain.so" ]] || \
    fail "Native build did not produce $OUTPUT_DIR/libmain.so"

file \
    "$OUTPUT_DIR/libSDL2.so" \
    "$OUTPUT_DIR/libSDL2_ttf.so" \
    "$OUTPUT_DIR/libSDL2_mixer.so" \
    "$OUTPUT_DIR/libSDL2_image.so" \
    "$OUTPUT_DIR/libSDL2_net.so" \
    "$OUTPUT_DIR/libmain.so" \
    "$OUTPUT_DIR/libc++_shared.so"
