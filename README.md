Certainly! Below is an expanded README with function names and their respective parameter arguments:

---

# Project Title

## Overview

This Arduino project is designed for controlling temperature and humidity using an AC unit and a heater. It includes functions to handle different states of the AC and heater based on environmental conditions.

## Components

- Arduino board
- DHTesp library
- DFRobot_SHT20 library
- LiquidCrystal_I2C library

## Pin Configuration

- `temp_up_rc_button` - Pin for temperature increase remote control button
- `temp_down_rc_button` - Pin for temperature decrease remote control button
- `ac_on_off_rc_button` - Pin for AC power on/off remote control button
- `ir_pin` - Pin for infrared LED
- `dip_switch_d`, `dip_switch_c`, `dip_switch_b`, `dip_switch_a` - Pins for DIP switches
- `HEATER_MODE` - Pin for selecting heater mode
- `humidifier_indication` - Pin for humidifier indication
- `ac_disabled_pin` - Pin for AC unit enable/disable switch

## Libraries Used

- DHTesp
- Wire
- DFRobot_SHT20
- LiquidCrystal_I2C
- SPI

## Installation

1. Install the required libraries (`DHTesp`, `DFRobot_SHT20`, `LiquidCrystal_I2C`) in the Arduino IDE.
2. Connect the components according to the specified pin configuration.
3. Upload the provided Arduino sketch to your Arduino board.

## Usage

1. Ensure all components are correctly connected.
2. Power on the Arduino board.
3. Monitor the LCD display for temperature, humidity, and system status.
4. Use the remote control buttons to adjust temperature or toggle AC and heater modes.

## Configuration

- Adjust temperature and humidity thresholds in the code as needed.
- Set DIP switches to configure specific options.

## Function Details

### `float Dip_Switch_Offset()`

Returns the offset value based on the state of DIP switches.

### `String ac_state_lcd()`

Returns a formatted string representing the current state of the AC unit for display.

### `void press_rc_button(int rc_button_pin, bool enable_led)`

Simulates pressing a remote control button.

### `void ac_on_sensor_fault()`

Simulates a sequence of button presses to address AC sensor faults.

### `void heater_on_sensor_fault()`

Simulates a sequence of button presses to address heater sensor faults.

### `void heating_enable()`

Simulates a sequence of button presses to enable heating.

### `void heating_disable()`

Simulates a sequence of button presses to disable heating.

### `void ac_ir_command_repeat()`

Simulates repeating AC infrared commands.

### `float Heater_Dip_Switch_Offset()`

Returns the offset value for the heater based on the state of DIP switches.

### `void ac_cooling_enable()`

Simulates a sequence of button presses to enable AC cooling.

### `void ac_cooling_disable()`

Simulates a sequence of button presses to disable AC cooling.

### `void ac_power_toggle()`

Simulates a button press to toggle AC power.

### `void humidifier_on()`

Turns on the humidifier indication.

### `void humidifier_off()`

Turns off the humidifier indication.

### `String repeat_action_remaining_time_string_get()`

Returns a formatted string indicating the remaining time for a repeat action.

### `void heater_processing(float min_temp, float max_temp, float room_temp, bool reading_error, bool ac_on_button_on_box)`

Handles the processing logic for the heater based on temperature conditions.

### `void ac_processing(float min_hidx, float max_hidx, float room_temp, float room_humd, float room_hidx, bool reading_error, bool ac_on_button_on_box)`

Handles the processing logic for the AC unit based on temperature and humidity conditions.

## Notes

- This project assumes specific hardware configurations. Ensure compatibility before making modifications.
- Refer to the comments in the code for detailed explanations of each section.

## Author

[Ahmed]

