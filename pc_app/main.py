import pygame
import serial
import time
import ctypes
import math

# --- Configuration ---
SERIAL_PORT = "COM3"
BAUDRATE = 1000000
LOOP_PERIOD_MS = 1 # 1ms for 1000Hz

# --- GUI Configuration ---
WINDOW_WIDTH = 800
WINDOW_HEIGHT = 600
BG_COLOR = (30, 30, 30)
STICK_BASE_COLOR = (80, 80, 80)
STICK_NUB_COLOR = (120, 120, 120)
TRIGGER_BASE_COLOR = (80, 80, 80)
TRIGGER_ACTIVE_COLOR_START = (100, 100, 150)
TRIGGER_ACTIVE_COLOR_END = (150, 150, 255)

# --- Protocol Definition (mirrors common/protocol.h) ---
START_BYTE = 0xAA

class DS4InputData(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("buttons", ctypes.c_uint16),
        ("left_stick_x", ctypes.c_uint8), ("left_stick_y", ctypes.c_uint8),
        ("right_stick_x", ctypes.c_uint8), ("right_stick_y", ctypes.c_uint8),
        ("l2_trigger", ctypes.c_uint8), ("r2_trigger", ctypes.c_uint8),
        ("accel_x", ctypes.c_int16), ("accel_y", ctypes.c_int16), ("accel_z", ctypes.c_int16),
        ("gyro_x", ctypes.c_int16), ("gyro_y", ctypes.c_int16), ("gyro_z", ctypes.c_int16),
    ]

class SerialPacket(ctypes.Structure):
    _pack_ = 1
    _fields_ = [("start_byte", ctypes.c_uint8), ("data", DS4InputData), ("checksum", ctypes.c_uint8)]

def calculate_checksum(data_struct):
    return sum(bytes(data_struct)) & 0xFF # Simple sum checksum

def lerp_color(color1, color2, t):
    """Linearly interpolate between two colors."""
    t = max(0, min(1, t))
    return tuple(int(a + (b - a) * t) for a, b in zip(color1, color2))

def draw_gui(screen, joystick):
    """Renders the controller visualization."""
    screen.fill(BG_COLOR)

    # --- Left Joystick ---
    left_stick_base_pos = (200, 300)
    pygame.draw.circle(screen, STICK_BASE_COLOR, left_stick_base_pos, 50)

    # Get stick values (-1.0 to 1.0)
    ls_x = joystick.get_axis(0)
    ls_y = joystick.get_axis(1)

    # Calculate nub position
    nub_x = int(left_stick_base_pos[0] + ls_x * 40)
    nub_y = int(left_stick_base_pos[1] + ls_y * 40)
    pygame.draw.circle(screen, STICK_NUB_COLOR, (nub_x, nub_y), 30)

    # --- Right Joystick (static for now) ---
    right_stick_base_pos = (600, 300)
    pygame.draw.circle(screen, STICK_BASE_COLOR, right_stick_base_pos, 50)
    pygame.draw.circle(screen, STICK_NUB_COLOR, right_stick_base_pos, 30)

    # --- L2 Trigger ---
    l2_axis_val = (joystick.get_axis(4) + 1.0) / 2.0 # Normalize to 0.0 - 1.0
    l2_rect = pygame.Rect(150, 100, 100, 50)
    pygame.draw.rect(screen, TRIGGER_BASE_COLOR, l2_rect, border_radius=10)
    l2_color = lerp_color(TRIGGER_BASE_COLOR, TRIGGER_ACTIVE_COLOR_END, l2_axis_val)
    # Draw an inner rect to show pressure
    pygame.draw.rect(screen, l2_color, l2_rect.inflate(-10, -10), border_radius=8)


    # --- R2 Trigger ---
    r2_axis_val = (joystick.get_axis(5) + 1.0) / 2.0 # Normalize to 0.0 - 1.0
    r2_rect = pygame.Rect(550, 100, 100, 50)
    pygame.draw.rect(screen, TRIGGER_BASE_COLOR, r2_rect, border_radius=10)
    r2_color = lerp_color(TRIGGER_BASE_COLOR, TRIGGER_ACTIVE_COLOR_END, r2_axis_val)
    pygame.draw.rect(screen, r2_color, r2_rect.inflate(-10, -10), border_radius=8)

    # --- Buttons ---
    font = pygame.font.SysFont(None, 24)
    for i in range(joystick.get_numbuttons()):
        if joystick.get_button(i):
            text = font.render(f'B{i}', True, (255, 255, 255))
            screen.blit(text, (350 + (i % 4) * 50, 250 + (i // 4) * 30))

    pygame.display.flip()


def main():
    # --- Initialization ---
    print("Initializing Pygame...")
    pygame.init()
    pygame.joystick.init()

    if pygame.joystick.get_count() == 0:
        print("Error: No joystick/gamepad detected.")
        return

    joystick = pygame.joystick.Joystick(0)
    joystick.init()
    print(f"Initialized Joystick: {joystick.get_name()}")

    screen = pygame.display.set_mode((WINDOW_WIDTH, WINDOW_HEIGHT))
    pygame.display.set_caption("DS4 Input Visualizer & Sender")

    ser = None
    try:
        print(f"Opening serial port {SERIAL_PORT} at {BAUDRATE} bps...")
        ser = serial.Serial(SERIAL_PORT, BAUDRATE)
    except serial.SerialException as e:
        print(f"Warning: Could not open serial port: {e}. Running in GUI-only mode.")

    print("--- Starting Controller Data Transmission ---")
    print("Press Ctrl+C or close the window to exit.")

    packet = SerialPacket()
    packet.start_byte = START_BYTE
    running = True

    try:
        while running:
            start_time = time.perf_counter()

            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False

            # --- Data Collection ---
            buttons = sum(1 << i for i in range(joystick.get_numbuttons()) if joystick.get_button(i))
            packet.data.buttons = buttons
            packet.data.left_stick_x = int((joystick.get_axis(0) + 1.0) * 127.5)
            packet.data.left_stick_y = int((joystick.get_axis(1) + 1.0) * 127.5)
            packet.data.right_stick_x = int((joystick.get_axis(2) + 1.0) * 127.5)
            packet.data.right_stick_y = int((joystick.get_axis(3) + 1.0) * 127.5)
            packet.data.l2_trigger = int((joystick.get_axis(4) + 1.0) * 127.5)
            packet.data.r2_trigger = int((joystick.get_axis(5) + 1.0) * 127.5)

            packet.data.accel_x, packet.data.accel_y, packet.data.accel_z = 0, 0, 0
            packet.data.gyro_x, packet.data.gyro_y, packet.data.gyro_z = 0, 0, 0

            # --- Packet Finalization & Transmission ---
            if ser:
                packet.checksum = calculate_checksum(packet.data)
                ser.write(bytes(packet))

            # --- GUI Rendering ---
            draw_gui(screen, joystick)

            # --- Loop Timing ---
            elapsed_ms = (time.perf_counter() - start_time) * 1000
            sleep_duration_ms = LOOP_PERIOD_MS - elapsed_ms
            if sleep_duration_ms > 0:
                time.sleep(sleep_duration_ms / 1000.0)

    except KeyboardInterrupt:
        print("\n--- Stopping ---")
    finally:
        joystick.quit()
        pygame.quit()
        if ser and ser.is_open:
            ser.close()
            print("Serial port closed.")

if __name__ == "__main__":
    main()
