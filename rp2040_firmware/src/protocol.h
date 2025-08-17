#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// --- Protocol Configuration ---

// START_BYTE: A constant marker to indicate the beginning of a new packet.
// This helps the receiver synchronize with the data stream.
#define START_BYTE 0xAA

// BAUDRATE: The required speed for the serial communication.
// A rate of 1,000,000 bps (1 Mbps) is chosen to meet the 1000Hz polling rate.
// Packet size = 22 bytes. With 10 bits per byte (1 start, 8 data, 1 stop),
// Transmission time = (22 * 10) / 1,000,000 = 0.22ms, which is well within
// the 1ms target.
#define BAUDRATE 1000000

// --- Data Structures ---

// To ensure that the structs are packed without any padding,
// which is crucial for consistent serialization across different
// compilers and systems (PC and RP2040).
#pragma pack(push, 1)

/**
 * @brief Holds all input data from a DualShock 4 controller.
 *
 * This structure is designed to be compact and comprehensive, including all
 * digital buttons, analog sticks, triggers, and motion sensor data.
 */
typedef struct {
    // Buttons: 16 bits are used to represent the state of all digital buttons.
    // Each bit corresponds to a specific button. This is a common and
    // efficient way to handle multiple boolean states.
    // Example mapping (to be finalized):
    // Bit 0: Square, Bit 1: Cross, Bit 2: Circle, Bit 3: Triangle
    // Bit 4: L1, Bit 5: R1, Bit 6: L2 (digital), Bit 7: R2 (digital)
    // Bit 8: Share, Bit 9: Options, Bit 10: L3, Bit 11: R3
    // Bit 12: PS, Bit 13: Touchpad Click
    uint16_t buttons;

    // Joysticks: Each axis is represented by an 8-bit unsigned integer,
    // providing a range from 0 to 255. Typically, 128 is the neutral position.
    uint8_t  left_stick_x;
    uint8_t  left_stick_y;
    uint8_t  right_stick_x;
    uint8_t  right_stick_y;

    // Triggers: The analog pressure of L2 and R2 triggers, each represented
    // by an 8-bit unsigned integer (0 for released, 255 for fully pressed).
    uint8_t  l2_trigger;
    uint8_t  r2_trigger;

    // IMU (Inertial Measurement Unit) Data: Gyroscope and Accelerometer.
    // Each axis is a 16-bit signed integer, providing high-resolution
    // motion data. This is the standard representation for these sensors.
    int16_t  accel_x;
    int16_t  accel_y;
    int16_t  accel_z;
    int16_t  gyro_x;
    int16_t  gyro_y;
    int16_t  gyro_z;
} DS4InputData; // Total size: 20 bytes

/**
 * @brief The complete serial packet sent from PC to RP2040.
 *
 * This struct wraps the core DS4 input data with framing and error-checking
 * fields to ensure reliable communication.
 */
typedef struct {
    // start_byte: Marks the beginning of the packet. Must be START_BYTE.
    uint8_t       start_byte;

    // data: The actual controller input data.
    DS4InputData  data;

    // checksum: Used for error detection. This should be a simple 8-bit
    // checksum calculated over the `data` field.
    // e.g., checksum = byte_0 ^ byte_1 ^ ... ^ byte_n
    uint8_t       checksum;
} SerialPacket; // Total size: 1 (start) + 20 (data) + 1 (checksum) = 22 bytes

#pragma pack(pop)

// --- HID Report Structures ---

// Hat switch values
typedef enum {
    HAT_SWITCH_UP = 0,
    HAT_SWITCH_UP_RIGHT = 1,
    HAT_SWITCH_RIGHT = 2,
    HAT_SWITCH_DOWN_RIGHT = 3,
    HAT_SWITCH_DOWN = 4,
    HAT_SWITCH_DOWN_LEFT = 5,
    HAT_SWITCH_LEFT = 6,
    HAT_SWITCH_UP_LEFT = 7,
    HAT_SWITCH_NEUTRAL = 8,
} hat_switch_t;

// Nintendo Switch Button Mapping (Matches Pro Controller)
typedef enum {
    SWITCH_BTN_Y = 0,
    SWITCH_BTN_B = 1,
    SWITCH_BTN_A = 2,
    SWITCH_BTN_X = 3,
    SWITCH_BTN_L = 4,
    SWITCH_BTN_R = 5,
    SWITCH_BTN_ZL = 6,
    SWITCH_BTN_ZR = 7,
    SWITCH_BTN_MINUS = 8,
    SWITCH_BTN_PLUS = 9,
    SWITCH_BTN_L_CLICK = 10,
    SWITCH_BTN_R_CLICK = 11,
    SWITCH_BTN_HOME = 12,
    SWITCH_BTN_CAPTURE = 13,
} switch_button_t;

// Switch report is a raw byte array because its bit packing is complex.
// The report format will be constructed manually in a helper function.
#define SWITCH_REPORT_SIZE 12
typedef uint8_t hid_switch_report_t[SWITCH_REPORT_SIZE];


#endif // PROTOCOL_H
