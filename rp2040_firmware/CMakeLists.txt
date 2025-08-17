# This file is based on the structure of the official pico-examples

# --- Filesystem Generation ---
# Add a custom command to run the Python script to convert the HTML to a C file.
set(FSDATA_C ${CMAKE_CURRENT_BINARY_DIR}/fsdata.c)
add_custom_command(
    OUTPUT ${FSDATA_C}
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/pc_app/build_fs.py
    DEPENDS ${CMAKE_SOURCE_DIR}/rp2040_firmware/src/html/config.html
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating ${FSDATA_C} from config.html"
)

add_custom_target(
    fsdata_generator ALL
    DEPENDS ${FSDATA_C}
)

# --- Main Executable Target ---
add_executable(ds4_to_gamepad
    src/main.c
    src/usb_descriptors.c
    ${FSDATA_C}
)

# Link all necessary libraries to the target
target_link_libraries(ds4_to_gamepad PRIVATE
    pico_stdlib
    tinyusb_device
    tinyusb_board
    hardware_uart
    tinyusb_networking
    pico_lwip
)

# Add the src directory to the include path for our local headers
target_include_directories(ds4_to_gamepad PRIVATE src)

# Add compile definitions needed for our configuration
target_compile_definitions(ds4_to_gamepad PRIVATE
    TUD_NET_RNDIS=1
)

# Ensure our fsdata file is generated before we try to build the executable
add_dependencies(ds4_to_gamepad fsdata_generator)

# Create the .uf2 file and other outputs
pico_add_extra_outputs(ds4_to_gamepad)
