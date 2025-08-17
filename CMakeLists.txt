cmake_minimum_required(VERSION 3.13)

# Find the Pico SDK
find_package(PicoSDK REQUIRED)

project(rp2040_input_converter LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

# Add the firmware subdirectory
add_subdirectory(rp2040_firmware)
