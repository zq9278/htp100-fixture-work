cmake_minimum_required(VERSION 3.24)

project(slk_sdl_sim C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(SDL2_BUNDLED_MINGW_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2-2.30.11/x86_64-w64-mingw32")
set(SDL2_LOCAL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/build-sdl-host/deps/prefix")

if(WIN32 AND EXISTS "${SDL2_BUNDLED_MINGW_ROOT}/include/SDL2/SDL.h")
    add_library(SDL2::SDL2 SHARED IMPORTED)
    set_target_properties(SDL2::SDL2 PROPERTIES
        IMPORTED_IMPLIB "${SDL2_BUNDLED_MINGW_ROOT}/lib/libSDL2.dll.a"
        IMPORTED_LOCATION "${SDL2_BUNDLED_MINGW_ROOT}/bin/SDL2.dll"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_BUNDLED_MINGW_ROOT}/include/SDL2"
    )
else()
    set(SDL2_PREFIX_HINTS
        "${SDL2_LOCAL_PREFIX}"
        "$ENV{SDL2_ROOT}"
        "/opt/homebrew"
        "/usr/local"
        "/usr"
    )

    find_package(SDL2 QUIET CONFIG PATHS ${SDL2_PREFIX_HINTS})

    if(NOT TARGET SDL2::SDL2)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL2_PC QUIET IMPORTED_TARGET sdl2)
            if(TARGET PkgConfig::SDL2_PC)
                add_library(SDL2::SDL2 INTERFACE IMPORTED)
                target_link_libraries(SDL2::SDL2 INTERFACE PkgConfig::SDL2_PC)
            endif()
        endif()
    endif()

    if(NOT TARGET SDL2::SDL2)
        find_path(SDL2_INCLUDE_DIR
            NAMES SDL.h
            PATH_SUFFIXES include/SDL2
            PATHS ${SDL2_PREFIX_HINTS}
        )
        find_library(SDL2_LIBRARY
            NAMES SDL2
            PATH_SUFFIXES lib
            PATHS ${SDL2_PREFIX_HINTS}
        )

        if(SDL2_INCLUDE_DIR AND SDL2_LIBRARY)
            add_library(SDL2::SDL2 UNKNOWN IMPORTED)
            set_target_properties(SDL2::SDL2 PROPERTIES
                IMPORTED_LOCATION "${SDL2_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
            )
        endif()
    endif()

    if(NOT TARGET SDL2::SDL2)
        message(FATAL_ERROR
            "SDL2 not found. Install SDL2 system-wide, set SDL2_ROOT, or run ./run_sdl.sh "
            "to build the local dependency in ${SDL2_LOCAL_PREFIX}."
        )
    endif()
endif()

file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

foreach(excluded_dir IN ITEMS
    "/src/draw/arm2d/"
    "/src/draw/nxp/"
    "/src/draw/renesas/"
    "/src/draw/sdl/"
    "/src/draw/stm32_dma2d/"
    "/src/draw/swm341_dma2d/"
    "/src/extra/libs/ffmpeg/"
    "/src/extra/libs/freetype/"
    "/src/extra/libs/rlottie/"
)
    list(FILTER LVGL_SOURCES EXCLUDE REGEX "${excluded_dir}")
endforeach()

file(GLOB_RECURSE UI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/demos/slk/*.c"
)

list(FILTER UI_SOURCES EXCLUDE REGEX "/demos/slk/images_all\\.c$")
list(FILTER UI_SOURCES EXCLUDE REGEX "/demos/slk/images/images_all\\.c$")

add_executable(slk_sdl_sim
    ${LVGL_SOURCES}
    ${UI_SOURCES}
    sim/desktop/main.c
    sim/desktop/desktop_shim.c
)

target_compile_definitions(slk_sdl_sim PRIVATE
    LV_CONF_INCLUDE_SIMPLE
    LV_SIMULATOR_DESKTOP
)

target_include_directories(slk_sdl_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/demos/slk
    ${CMAKE_CURRENT_SOURCE_DIR}/sim/desktop/include
)

target_link_libraries(slk_sdl_sim PRIVATE SDL2::SDL2)

if(MINGW)
    target_link_options(slk_sdl_sim PRIVATE -mconsole)
endif()

if(WIN32 AND EXISTS "${SDL2_BUNDLED_MINGW_ROOT}/bin/SDL2.dll")
    add_custom_command(TARGET slk_sdl_sim POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_BUNDLED_MINGW_ROOT}/bin/SDL2.dll"
        "$<TARGET_FILE_DIR:slk_sdl_sim>/SDL2.dll"
    )
endif()
