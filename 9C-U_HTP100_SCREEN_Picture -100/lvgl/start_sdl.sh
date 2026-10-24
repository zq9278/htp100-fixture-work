#!/bin/sh

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
APP_BIN="${ROOT_DIR}/build-sdl-host/slk_sdl_sim"

needs_rebuild() {
    if [ ! -x "${APP_BIN}" ]; then
        return 0
    fi

    if [ "${ROOT_DIR}/lv_conf.h" -nt "${APP_BIN}" ] || [ "${ROOT_DIR}/run_sdl.sh" -nt "${APP_BIN}" ]; then
        return 0
    fi

    NEWER_FILE=$(
        find "${ROOT_DIR}/src" "${ROOT_DIR}/demos/slk" "${ROOT_DIR}/sim/desktop" \
            \( -name '*.c' -o -name '*.h' \) \
            -newer "${APP_BIN}" \
            -print \
            -quit
    )

    [ -n "${NEWER_FILE}" ]
}

if needs_rebuild; then
    echo "sources changed, rebuilding simulator..."
    "${ROOT_DIR}/run_sdl.sh"
else
    echo "simulator is up to date."
fi

exec "${APP_BIN}" "$@"
