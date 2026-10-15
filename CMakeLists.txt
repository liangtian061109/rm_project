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
set(CMAKE_PROJECT_NAME uart)

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

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
# 自动收集 APP/、Bsp/ 和 Modules/ 下所有 .c 源文件
file(GLOB_RECURSE USER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/APP/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Bsp/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Modules/*.c"
)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${USER_SOURCES}
)

# Add include paths
# 自动收集 APP/、Bsp/ 和 Modules/ 下所有含 .h 的目录作为头文件搜索路径
file(GLOB_RECURSE USER_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/APP/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Bsp/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Modules/*.h"
)
set(USER_INCLUDE_DIRS "")
foreach(f ${USER_HEADERS})
    get_filename_component(d ${f} DIRECTORY)
    list(APPEND USER_INCLUDE_DIRS ${d})
endforeach()
list(REMOVE_DUPLICATES USER_INCLUDE_DIRS)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${USER_INCLUDE_DIRS}
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
# BEGIN: DAPLink one-shot flash (independent from STM32 VS Code integration)
set(DAPLINK_OPENOCD "D:/TI/tools/openocd/bin/openocd.exe" CACHE FILEPATH "OpenOCD executable used by the DAPLink flash target")
set(DAPLINK_OPENOCD_SCRIPTS "D:/TI/tools/openocd/share/openocd/scripts" CACHE PATH "OpenOCD scripts directory")
set(DAPLINK_SERIAL "7adb8f0d" CACHE STRING "CMSIS-DAP serial number used for STM32 flashing")
set(DAPLINK_OBJCOPY "C:/Users/30710/AppData/Local/stm32cube/bundles/gnu-tools-for-stm32/14.3.1+st.2/bin/arm-none-eabi-objcopy.exe" CACHE FILEPATH "GNU objcopy used to generate the HEX file")
set(DAPLINK_HEX "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex")

add_custom_target(flash_daplink
    COMMAND "${DAPLINK_OBJCOPY}" -O ihex "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>" "${DAPLINK_HEX}"
    COMMAND "${DAPLINK_OPENOCD}"
        -s "${DAPLINK_OPENOCD_SCRIPTS}"
        -f interface/cmsis-dap.cfg
        -c "adapter serial ${DAPLINK_SERIAL}"
        -c "transport select swd"
        -f target/stm32f4x.cfg
        -c "program {${DAPLINK_HEX}} verify"
        -c "reset run"
        -c "exit"
    DEPENDS ${CMAKE_PROJECT_NAME}
    USES_TERMINAL
    VERBATIM
    COMMENT "Flashing ${CMAKE_PROJECT_NAME}.hex through DAPLink (CMSIS-DAP)"
)
# END: DAPLink one-shot flash


