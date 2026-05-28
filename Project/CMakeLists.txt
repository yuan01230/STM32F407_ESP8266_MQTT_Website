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
        Library/gpio/gpio_device.c
        Library/gpio/gpio_device.h
        Library/bsp/board_drv_config.h
        Library/led/led.c
        Library/led/led.h
        Library/led/led_example.c
        Library/tftlcd/tftlcd.c
        Library/tftlcd/tftlcd.h
        Library/tftlcd/tftlcd_port.c
        Library/tftlcd/tftlcd_port.h
        Library/tftlcd/tftlcd_hx8357dn.c
        Library/tftlcd/tftlcd_hx8357dn.h
        Library/tftlcd/tftlcd_example.c
        Library/tftlcd/font.h
        Library/tftlcd/picture.h
        Library/key/key.c
        Library/key/key.h
        Library/key/key_example.c
        Library/EN25Q128/EN25Q128.c
        Library/EN25Q128/EN25Q128.h
        Library/Light_Sensor/light_sensor.c
        Library/Light_Sensor/light_sensor.h
        Library/Light_Sensor/light_sensor_example.c
        Library/font_storage/font_storage.c
        Library/font_storage/font_storage.h
        Library/font_storage/font_boot.c
        Library/font_storage/font_boot.h
        Library/font_codec/font_codec.c
        Library/font_codec/font_codec.h
        Library/SoftwareI2C/softwarei2c.c
        Library/SoftwareI2C/softwarei2c.h
        Library/mpu6050/MPU6050.c
        Library/mpu6050/MPU6050.h
        Library/mpu6050/dmp/inv_mpu.c
        Library/mpu6050/dmp/inv_mpu.h
        Library/mpu6050/dmp/inv_mpu_dmp_motion_driver.c
        Library/mpu6050/dmp/inv_mpu_dmp_motion_driver.h
        Library/DHT11/dht11.c
        Library/DHT11/dht11.h
        Library/DHT11/dht11_example.c
        Library/delay/delay.c
        Library/delay/delay.h
        Library/delay/delay_example.c
        Library/beep/beep.c
        Library/beep/beep.h
        Library/beep/beep_example.c
        Library/usart/usart.c
        Library/usart/usart.h
        Library/aliyun_iot/aliyun_iot.c
        Library/aliyun_iot/aliyun_iot.h
        Library/aliyun_iot/aliyun_iot_config.h
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
    Library/gpio
    Library/bsp
    Library/mpu6050/dmp
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
