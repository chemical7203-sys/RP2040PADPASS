# This file is based on the root CMakeLists.txt from the official pico-examples
cmake_minimum_required(VERSION 3.13)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

project(rp2040_input_converter_project C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

# Add the subdirectory with the actual firmware code
add_subdirectory(rp2040_firmware)
