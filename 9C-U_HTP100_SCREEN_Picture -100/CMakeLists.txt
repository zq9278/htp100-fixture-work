cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME 9C-HTP100-screen)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Restore the user sources that existed in the original MDK project.
file(GLOB_RECURSE USER_HARDWARE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/HardWare/Src/*.c
)

file(GLOB_RECURSE USER_LVGL_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/src/*.c
)

foreach(EXCLUDED_LVGL_DIR IN ITEMS
    "/draw/arm2d/"
    "/draw/nxp/"
    "/draw/renesas/"
    "/draw/sdl/"
    "/draw/swm341_dma2d/"
    "/extra/libs/ffmpeg/"
    "/extra/libs/freetype/"
    "/extra/libs/fsdrv/"
    "/extra/libs/rlottie/"
)
    list(FILTER USER_LVGL_SOURCES EXCLUDE REGEX "${EXCLUDED_LVGL_DIR}")
endforeach()

file(GLOB_RECURSE USER_LVGL_PORT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/porting/*.c
)
list(FILTER USER_LVGL_PORT_SOURCES EXCLUDE REGEX "/lv_port_fs_template\\.c$")
list(FILTER USER_LVGL_PORT_SOURCES EXCLUDE REGEX "/lv_port_disp_template copy\\.c$")

file(GLOB_RECURSE USER_UI_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/demos/slk/*.c
)
list(FILTER USER_UI_SOURCES EXCLUDE REGEX "/images_all\\.c$")
# Fixture-only build; retain original files in the project for reference.
list(FILTER USER_HARDWARE_SOURCES EXCLUDE REGEX "/(screen|ui-myself)\\.c$")
set(USER_UI_SOURCES "")


# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${USER_HARDWARE_SOURCES}
    ${USER_LVGL_SOURCES}
    ${USER_LVGL_PORT_SOURCES}
    ${USER_UI_SOURCES}
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/HardWare/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/porting
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/demos/slk
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/demos/slk/components
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/demos/slk/fonts
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/demos/slk/images
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/demos/slk/screens
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    LV_CONF_INCLUDE_SIMPLE
    STM32H7
    SCREEN_PANEL_OLD=0
)

# Every default build produces BOTH hardware variants from the same UI/protocol.
# The existing target is the new panel; this second target selects the retained
# old-panel vendor initialization sequence in Lcd_int.c.
set(OLD_SCREEN_TARGET "${CMAKE_PROJECT_NAME}-old")
get_target_property(SCREEN_ALL_SOURCES ${CMAKE_PROJECT_NAME} SOURCES)
# Include CubeMX application/startup sources too; these are attached directly
# to the first executable rather than exported by the stm32cubemx interface.
add_executable(${OLD_SCREEN_TARGET} ${SCREEN_ALL_SOURCES})
get_target_property(SCREEN_INCLUDE_DIRS ${CMAKE_PROJECT_NAME} INCLUDE_DIRECTORIES)
target_include_directories(${OLD_SCREEN_TARGET} PRIVATE ${SCREEN_INCLUDE_DIRS})
target_compile_definitions(${OLD_SCREEN_TARGET} PRIVATE
    LV_CONF_INCLUDE_SIMPLE STM32H7 SCREEN_PANEL_OLD=1
)
target_link_libraries(${OLD_SCREEN_TARGET} stm32cubemx STM32_Drivers ${TOOLCHAIN_LINK_LIBRARIES})
# Each target must have its own map file when both link concurrently.
target_link_options(${OLD_SCREEN_TARGET} PRIVATE "-Wl,-Map=${OLD_SCREEN_TARGET}.map")

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

# Generate a raw binary image alongside the ELF after linking.
if(CMAKE_OBJCOPY)
  foreach(SCREEN_TARGET IN ITEMS ${CMAKE_PROJECT_NAME} ${OLD_SCREEN_TARGET})
    set(BINARY_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${SCREEN_TARGET}.bin)
    set(HEX_OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${SCREEN_TARGET}.hex)

    add_custom_command(TARGET ${SCREEN_TARGET} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${SCREEN_TARGET}>
            ${BINARY_OUTPUT_FILE}
        COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:${SCREEN_TARGET}>
            ${HEX_OUTPUT_FILE}
        BYPRODUCTS
            ${BINARY_OUTPUT_FILE}
            ${HEX_OUTPUT_FILE}
        COMMENT "Generating ${SCREEN_TARGET}.bin and ${SCREEN_TARGET}.hex"
    )

    set_property(TARGET ${SCREEN_TARGET} APPEND PROPERTY
        ADDITIONAL_CLEAN_FILES
        ${BINARY_OUTPUT_FILE}
        ${HEX_OUTPUT_FILE}
    )
  endforeach()
endif()
