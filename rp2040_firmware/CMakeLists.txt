# This file is based on the structure of the official pico-examples and tinyusb examples

# --- Define paths to the networking example files within the SDK ---
set(TINYUSB_NET_EXAMPLE_PATH ${PICO_SDK_PATH}/lib/tinyusb/examples/device/net/net_lwip_webserver/src)
set(FSDATA_C ${CMAKE_CURRENT_BINARY_DIR}/fsdata.c)

# --- Custom command to generate the web filesystem ---
add_custom_command(
    OUTPUT ${FSDATA_C}
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/pc_app/build_fs.py
    DEPENDS ${CMAKE_SOURCE_DIR}/rp2040_firmware/src/html/config.html
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating web filesystem from HTML"
)

add_custom_target(
    fsdata_generator ALL
    DEPENDS ${FSDATA_C}
)

# --- Main Executable Target ---
add_executable(ds4_to_gamepad
    # Project-specific source files
    src/main.c
    src/usb_descriptors.c

    # Generated web filesystem
    ${FSDATA_C}

    # Source files for DHCP and DNS servers from the TinyUSB example
    ${TINYUSB_NET_EXAMPLE_PATH}/dhserver.c
    ${TINYUSB_NET_EXAMPLE_PATH}/dnserver.c
    ${TINYUSB_NET_EXAMPLE_PATH}/lwip_hooks.c
)

# --- Link Libraries ---
target_link_libraries(ds4_to_gamepad PRIVATE
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
    tinyusb_device
    tinyusb_board
    hardware_uart
)

# --- Include Directories ---
target_include_directories(ds4_to_gamepad PRIVATE
    # Local headers in our src folder
    src
    # Include path for the networking example headers (dhserver.h, etc.)
    ${TINYUSB_NET_EXAMPLE_PATH}
)

# --- Compile Definitions ---
target_compile_definitions(ds4_to_gamepad PRIVATE
    TUD_NET_RNDIS=1
)

# --- Build Dependencies ---
# Ensure fsdata is generated before the main executable is built
add_dependencies(ds4_to_gamepad fsdata_generator)

# --- Final Outputs ---
pico_add_extra_outputs(ds4_to_gamepad)
