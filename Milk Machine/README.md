# Automatic Milk Mixing Machine

## Overview

The Automatic Milk Mixing Machine is an Arduino-based project designed to automatically mix water and milk powder to prepare milk for various applications. It features a liquid level sensor to determine the milk level, motors to mix the ingredients, and relays to control water dispensing.

## Table of Contents

- [Automatic Milk Mixing Machine](#automatic-milk-mixing-machine)
  - [Overview](#overview)
  - [Table of Contents](#table-of-contents)
  - [Hardware Components](#hardware-components)
  - [Software Libraries](#software-libraries)
  - [Pin Configuration](#pin-configuration)
  - [Setup and Installation](#setup-and-installation)
  - [Usage](#usage)
  - [Customization](#customization)
  - [Troubleshooting](#troubleshooting)
  - [Contributing](#contributing)
  - [License](#license)

## Hardware Components

- Liquid Level Sensor
- DC Motors for Mixing
- Relays for Water Dispensing
- Arduino Board (e.g., Arduino Uno)
- Motor Driver (if needed)
- Power Supply

## Software Libraries

No external software libraries are required for this project.

## Pin Configuration

- Liquid Level Sensor Pin: A0
- Water Relay Pin: D1
- Mixer Relay Pin: D2
- Auger Motor PWM Pin: D5
- Agitator Motor PWM Pin: D6
- Motor 1 Direction Control Pin 1: D8
- Motor 1 Direction Control Pin 2: D9
- Motor 2 Direction Control Pin 1: D10
- Motor 2 Direction Control Pin 2: D11

## Setup and Installation

1. Connect the liquid level sensor to A0.
2. Wire the water relay to D1.
3. Wire the mixer relay to D2.
4. Connect the auger motor to D5 for PWM control.
5. Connect the agitator motor to D6 for PWM control.
6. Wire the motor direction controls (1 and 2) to pins D8, D9, D10, and D11.
7. Provide power supply to the motors, Arduino, and relays.

## Usage

1. Ensure the milk powder and water containers are properly set up.
2. Power on the Arduino.
3. The machine will automatically mix milk when the liquid level is low and stop when it's sufficient.
4. The mixer will continue running for 5 seconds after the mixing process begins.

## Customization

The project can be customized by adjusting the following parameters in the code:

- Motor speeds (AUGER_MOTOR_SPEED and AGITATOR_MOTOR_SPEED).
- Delay settings (LOOP_DELAY, MOTOR_ON_THRESHOLD, and MIXER_RUN_TIME).

Additional sensors or controls can be added for more advanced functionality.

## Troubleshooting

- If the machine is not mixing correctly, check the motor connections and speed settings.
- If the liquid level sensor does not work, ensure proper wiring and sensor functionality.

## Contributing

Contributions to this project are welcome. You can fork the repository, make improvements, and submit pull requests.

## License

This project is licensed under the [MIT License](LICENSE).

---

This documentation provides a comprehensive overview of the Automatic Milk Mixing Machine project, including hardware setup, usage instructions, customization options, and troubleshooting tips.
