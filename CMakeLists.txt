cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# If CLion directly sets arm-none-eabi-gcc as compiler (without toolchain file),
# force cross-compiling mode to avoid host Windows linker flags in try-compile.
if(CMAKE_C_COMPILER MATCHES "arm-none-eabi-gcc")
    set(CMAKE_SYSTEM_NAME Generic)
    set(CMAKE_SYSTEM_PROCESSOR arm)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
endif()

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME 1_LED)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME}
        Library/led/led.c
        Library/led/led.h
        Library/tftlcd/tftlcd.c
        Library/tftlcd/tftlcd.h
        Library/tftlcd/font.h
        Library/tftlcd/picture.h
        Library/key/key.c
        Library/key/key.h
        Library/SD_RW/sd_rw.c
        Library/SD_RW/sd_rw.h
        Library/usart/usart.c
        Library/usart/usart.h
        Library/EN25Q128/EN25Q128.c
        Library/EN25Q128/EN25Q128.h
        Library/Light_Sensor/light_sensor.c
        Library/Light_Sensor/light_sensor.h
        Library/flash/flash.c
        Library/flash/flash.h
        Library/font_storage/font_storage.c
        Library/font_storage/font_storage.h
        Library/font_codec/font_codec.c
        Library/font_codec/font_codec.h
        Library/ui/ui_app.c
        Library/ui/ui_app.h
        Library/ui/ui_common.c
        Library/ui/ui_common.h
        Library/ui/ui_main.c
        Library/ui/ui_main.h
        Library/ui/ui_list.c
        Library/ui/ui_list.h
        Library/ui/ui_view.c
        Library/ui/ui_view.h
        Library/ui/ui_font_import.c
        Library/ui/ui_font_import.h
        Library/ui/ui_create_file.c
        Library/ui/ui_create_file.h
        Library/rtc/rtc.c
        Library/rtc/rtc.h
        Library/beep/beep.c
        Library/beep/beep.h
        Library/AT24CXX/at24cxx.h
        Library/AT24CXX/at24cxx.c
        Library/SoftwareI2C/softwarei2c.c
        Library/SoftwareI2C/softwarei2c.h
        Library/DHT11/dht11.c
        Library/DHT11/dht11.h
        Library/delay/delay.c
        Library/delay/delay.h
        Library/touch/touch.c
        Library/touch/touch.h
        Library/sram/sram.c
        Library/sram/sram.h
        Malloc/malloc.c
        Malloc/malloc.h
)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    Malloc
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
# 开启硬件浮点运算
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

# 使能printf相关函数支持浮点数
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -u _printf_float)

# trigger reload
