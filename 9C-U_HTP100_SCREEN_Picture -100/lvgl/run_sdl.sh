#!/bin/sh

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_DIR="${ROOT_DIR}/build-sdl-host"
DEPS_DIR="${BUILD_DIR}/deps"
DEPS_ALIAS_DIR="${TMPDIR:-/tmp}/slk_lvgl_sdl2_deps"
SDL_VERSION="2.30.11"
SDL_ARCHIVE="SDL2-${SDL_VERSION}.tar.gz"
SDL_URL="https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/${SDL_ARCHIVE}"
SDL_SRC_DIR="${DEPS_ALIAS_DIR}/SDL2-${SDL_VERSION}"
SDL_PREFIX="${DEPS_ALIAS_DIR}/prefix"
SDL_CONFIG="${SDL_PREFIX}/bin/sdl2-config"
APP_BIN="${BUILD_DIR}/slk_sdl_sim"
OBJ_DIR="${BUILD_DIR}/obj"
SOURCE_LIST="${BUILD_DIR}/sources.list"

mkdir -p "${DEPS_DIR}" "${OBJ_DIR}"
ln -sfn "${DEPS_DIR}" "${DEPS_ALIAS_DIR}"

need_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "missing required command: $1" >&2
        exit 1
    fi
}

need_cmd clang
need_cmd make
need_cmd curl
need_cmd tar

cpu_count() {
    if command -v getconf >/dev/null 2>&1; then
        getconf _NPROCESSORS_ONLN 2>/dev/null && return
    fi
    if command -v sysctl >/dev/null 2>&1; then
        sysctl -n hw.ncpu 2>/dev/null && return
    fi
    echo 4
}

if [ ! -x "${SDL_CONFIG}" ]; then
    if [ ! -d "${SDL_SRC_DIR}" ]; then
        ARCHIVE_PATH="${DEPS_ALIAS_DIR}/${SDL_ARCHIVE}"
        if [ ! -f "${ARCHIVE_PATH}" ]; then
            echo "downloading SDL2 ${SDL_VERSION}..."
            curl -L "${SDL_URL}" -o "${ARCHIVE_PATH}"
        fi
        echo "extracting SDL2 ${SDL_VERSION}..."
        tar -xzf "${ARCHIVE_PATH}" -C "${DEPS_ALIAS_DIR}"
    fi

    echo "building local SDL2 into ${SDL_PREFIX}..."
    (
        cd "${SDL_SRC_DIR}"
        chmod +x ./configure
        ./configure --prefix="${SDL_PREFIX}"
        make -j"$(cpu_count)"
        make install
    )
fi

SDL_CFLAGS=$("${SDL_CONFIG}" --prefix="${SDL_PREFIX}" --cflags)
SDL_LIBS=$("${SDL_CONFIG}" --prefix="${SDL_PREFIX}" --libs)

echo "building simulator..."
: > "${SOURCE_LIST}"
find "${ROOT_DIR}/src" \
    \( -path "${ROOT_DIR}/src/draw/arm2d" \
    -o -path "${ROOT_DIR}/src/draw/nxp" \
    -o -path "${ROOT_DIR}/src/draw/renesas" \
    -o -path "${ROOT_DIR}/src/draw/sdl" \
    -o -path "${ROOT_DIR}/src/draw/stm32_dma2d" \
    -o -path "${ROOT_DIR}/src/draw/swm341_dma2d" \
    -o -path "${ROOT_DIR}/src/extra/libs/ffmpeg" \
    -o -path "${ROOT_DIR}/src/extra/libs/freetype" \
    -o -path "${ROOT_DIR}/src/extra/libs/rlottie" \
    \) -prune -o -name '*.c' -print0 >> "${SOURCE_LIST}"
find "${ROOT_DIR}/demos/slk" -name '*.c' ! -name 'images_all.c' ! -path '*/images/images_all.c' -print0 >> "${SOURCE_LIST}"
printf '%s\0' "${ROOT_DIR}/sim/desktop/main.c" "${ROOT_DIR}/sim/desktop/desktop_shim.c" >> "${SOURCE_LIST}"

xargs -0 clang -std=c99 -O2 -Wall -Wextra \
    -DLV_CONF_INCLUDE_SIMPLE \
    -DLV_SIMULATOR_DESKTOP \
    -I"${ROOT_DIR}" \
    -I"${ROOT_DIR}/demos/slk" \
    -I"${ROOT_DIR}/sim/desktop/include" \
    ${SDL_CFLAGS} \
    ${SDL_LIBS} \
    -o "${APP_BIN}" < "${SOURCE_LIST}"

echo "built ${APP_BIN}"
