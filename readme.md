# Water Level Control System

A simple ESP32-based water level monitoring project using an ultrasonic sensor and an OLED display. The system measures the distance from the sensor to the water surface, calculates a water level percentage, and indicates the tank state using LEDs.

## Features

- Measures water level using an ultrasonic sensor
- Calculates and displays water level percentage on an SSD1306 OLED
- Uses LED indicators for low, medium, and high water level states
- Serial output for debugging and monitoring
- Sends water level telemetry over UDP for the Python GUI
- Adjustable distances and thresholds in code

## Hardware Components

- ESP32 development board
- Ultrasonic sensor module (HC-SR04 or equivalent)
- SSD1306 OLED display (I2C 128x64)
- 4 LEDs for status indicators
- Resistors for LEDs
- Breadboard and jumper wires

## Pin Mapping

| Signal | ESP32 Pin |
|---|---|
| TRIG (ultrasonic) | GPIO 17 |
| ECHO (ultrasonic) | GPIO 16 |
| Output LED | GPIO 15 |
| Low-level LED | GPIO 23 |
| Medium-level LED | GPIO 19 |
| High-level LED | GPIO 18 |

> Note: The code also contains a commented-out buzzer pin definition for a future buzzer alarm.

## How It Works

1. The ultrasonic sensor sends a pulse from `TRIG_PIN`.
2. The sensor measures the echo time on `ECHO_PIN`.
3. Distance is calculated using the speed of sound.
4. The water level is mapped from the measured distance to a percentage value.
5. OLED updates the displayed percentage and tank graphic.
6. LEDs indicate the current water level region.

## Threshold Behavior

- `DISTANCE_READ > MIN_RANGE` (greater than 30 cm): low water zone
- `DISTANCE_READ > MAX_RANGE` (greater than 10 cm): medium water zone
- Otherwise: high water zone

## Software

- Main sketch: `Water_Level_Control.cpp`
- PlatformIO configuration: `platformio.ini`

### Required Libraries

- `Adafruit SSD1306`
- `Adafruit GFX`
- `Wire` (built into Arduino core)

If you use PlatformIO, install libraries using the library manager or add them to `lib_deps`.

## Build and Upload

1. Open the project in PlatformIO.
2. Select the `esp32dev` environment from `platformio.ini`.
3. Build the project:

```bash
pio run
```

4. Upload to the ESP32 board:

```bash
pio run -t upload
```

5. Open the serial monitor at 9600 baud for text output:

```bash
pio device monitor --baud 9600
```

## UDP Python GUI

The ESP32 can broadcast water level telemetry as JSON over UDP port `4210`.
Before uploading, edit these values in `src/Water_Level_Control.cpp`:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
```

Run the GUI on a computer connected to the same Wi-Fi network:

```bash
python tools/water_level_udp_gui.py
```

The UDP payload format is:

```json
{"waterLevel":50,"distanceCm":12.34,"motor":false,"ip":"192.168.1.10"}
```

## Notes

- The OLED display is enabled by default in the code via `USE_DISPLAY = true`.
- The water level mapping uses `tankHeight` and `tankFloor` values.
- Adjust the pin assignments and thresholds in `Water_Level_Control.ino` if your hardware uses different wiring.

## Improvements

- Add a buzzer alarm for low/high levels.
- Save measured values to external storage or send them over Wi-Fi.
- Add calibration support for different tank heights and sensor positions.
- Use a dedicated water level sensor for more stable readings.

## Reference
Karacaa, H. (n.d.). WaterLevelProject [Computer software]. GitHub. Retrieved May 19, 2026, from https://github.com/hamzakaracaa/WaterLevelProject
