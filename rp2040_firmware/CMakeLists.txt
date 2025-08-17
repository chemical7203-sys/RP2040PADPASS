add_executable(ds4_to_gamepad
    src/main.c
    src/usb_descriptors.c
    src/html/fsdata.c
)

# Pull in our pico_stdlib which is required
# Also link the TinyUSB device library, hardware UART, networking and lwIP
target_link_libraries(ds4_to_gamepad pico_stdlib tinyusb_device hardware_uart tinyusb_networking pico_lwip)

# Add src directory to include path for lwipopts.h
target_include_directories(ds4_to_gamepad PRIVATE src)

# Define the network interface for TinyUSB
target_compile_definitions(ds4_to_gamepad PRIVATE
    TUD_NET_RNDIS=1
)

# --- Filesystem Generation ---
# Add a custom command to run the Python script to convert the HTML to a C file.
# This command is attached to a custom target so we can make it a dependency.
set(FSDATA_C ${CMAKE_CURRENT_BINARY_DIR}/fsdata.c)
add_custom_command(
    OUTPUT ${FSDATA_C}
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/pc_app/build_fs.py
    DEPENDS ${CMAKE_SOURCE_DIR}/rp2040_firmware/src/html/config.html
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating ${FSDATA_C} from config.html"
)

# Add a custom target to execute the command.
add_custom_target(
    fsdata_generator ALL
    DEPENDS ${FSDATA_C}
)

# Add the generated file to our executable
target_sources(ds4_to_gamepad PRIVATE ${FSDATA_C})

# Make sure fsdata is generated before the main executable is built.
add_dependencies(ds4_to_gamepad fsdata_generator)


# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(ds4_to_gamepad)
