# README: Detailed Overview of the Arduino Cattle Feeder Code

## Introduction

Welcome to the comprehensive README for the Arduino Cattle Feeder project. This document aims to elucidate the functionalities, roles, and intricacies of each block of code in the program. Whether you're looking to make adjustments, troubleshoot issues, or simply understand the system better, this guide will serve as your one-stop reference.

## Table of Contents

1. [Introduction](#introduction)
2. [`#include` Directives](#include-directives)
3. [Constant Definitions](#constant-definitions)
4. [Function Declarations](#function-declarations)
5. [Global Variables](#global-variables)
6. [Function Descriptions](#function-descriptions)
   - [`void initializePins()`](#void-initializepins)
   - [`void setup()`](#void-setup)
   - [`void manageOperation()`](#void-manageoperation)
   - [Other Functions](#other-functions)
7. [The `loop()` Function](#the-loop-function)
8. [Conclusion](#conclusion)


## `#include` Directives

### Overview

The `#include` directives are the first lines you'll encounter in the code. They are essential for incorporating various libraries that bring in predefined functions and objects, facilitating code efficiency and reducing complexity.

### Code Snippet

```cpp
#include <Arduino.h>
#include <Bounce2.h>
#include "Wire.h"
```

### Detailed Explanation

#### `#include <Arduino.h>`

- **What It Does**:  
  This is the standard Arduino library that gets included implicitly in most Arduino sketches. It contains the core set of functions like `pinMode()`, `digitalRead()`, and `digitalWrite()` which are fundamental to Arduino programming.
  
- **Why It's Important**:  
  Without this, you'd be missing out on essential functionalities that make Arduino easy and accessible.

#### `#include <Bounce2.h>`

- **What It Does**:  
  The Bounce2 library is used for "debouncing" digital inputs. When a button is pressed or a switch is toggled, the signal can fluctuate rapidly before settling down. This library helps filter out that noise.
  
- **Why It's Important**:  
  In an automated system like a cattle feeder, you want your sensor readings to be accurate. Unwanted noise could cause the system to misbehave, leading to inconsistent feeding schedules or even mechanical wear and tear.

#### `#include "Wire.h"`

- **What It Does**:  
  This library is used for I2C communication between the Arduino and other I2C compatible devices. This can include various sensors, displays, or even other microcontrollers.
  
- **Why It's Important**:  
  For a complex system that involves more than just an Arduino board—like additional sensors or displays—the I2C communication protocol is essential for effective data transfer.

---

That wraps up the `#include` Directives section. Each library included serves a unique purpose, ensuring that the code runs efficiently and as intended. Feel free to move to the next section or let me know if you'd like to delve deeper into any specific part.Absolutely! Here's the detailed section on the `#include` Directives:

---

## `#include` Directives

### Overview

The `#include` directives are the first lines you'll encounter in the code. They are essential for incorporating various libraries that bring in predefined functions and objects, facilitating code efficiency and reducing complexity.

### Code Snippet

```cpp
#include <Arduino.h>
#include <Bounce2.h>
#include "Wire.h"
```

### Detailed Explanation

#### `#include <Arduino.h>`

- **What It Does**:  
  This is the standard Arduino library that gets included implicitly in most Arduino sketches. It contains the core set of functions like `pinMode()`, `digitalRead()`, and `digitalWrite()` which are fundamental to Arduino programming.
  
- **Why It's Important**:  
  Without this, you'd be missing out on essential functionalities that make Arduino easy and accessible.

#### `#include <Bounce2.h>`

- **What It Does**:  
  The Bounce2 library is used for "debouncing" digital inputs. When a button is pressed or a switch is toggled, the signal can fluctuate rapidly before settling down. This library helps filter out that noise.
  
- **Why It's Important**:  
  In an automated system like a cattle feeder, you want your sensor readings to be accurate. Unwanted noise could cause the system to misbehave, leading to inconsistent feeding schedules or even mechanical wear and tear.

#### `#include "Wire.h"`

- **What It Does**:  
  This library is used for I2C communication between the Arduino and other I2C compatible devices. This can include various sensors, displays, or even other microcontrollers.
  
- **Why It's Important**:  
  For a complex system that involves more than just an Arduino board—like additional sensors or displays—the I2C communication protocol is essential for effective data transfer.

## Constant Definitions

### Overview

The Constant Definitions section is a cornerstone of the code's structure, serving as a centralized location where essential values are defined. Instead of scattering these important constants throughout the code, this section groups them together for easy modification and reference.

### Code Intent and Benefits

By using the `#define` preprocessor directive, the code:

1. **Centralizes Control**: All critical settings, such as pin assignments and time delays, are located at the top of the script. This makes it easier to make changes without having to sift through the code.
  
2. **Enhances Readability**: Named constants are self-explanatory, making the code more understandable for developers who might work on it in the future.

3. **Ensures Reusability and Modularity**: Constants can be reused across different parts of the code, increasing modularity. This makes it easier to reconfigure one part of the system without affecting others.

4. **Minimizes Error**: Using named constants instead of "magic numbers" reduces the chance of introducing errors during code modification.

### Types of Constants Defined

The constants defined serve different functional aspects of the system:

- **Pin Assignments**: Constants like `LIQUID_LEVEL_SENSOR_PIN`, `WATER_RELAY_PIN`, and `MIXER_RELAY_PIN` specify which pins on the Arduino are used for what purpose. This is essential for hardware-software interaction.

- **Timings and Delays**: Constants like `SENSOR_HIGH_DELAY` determine how long certain operations or states should last, affecting the system's real-time performance.

### Best Practices

It's highly recommended to keep this section updated and well-commented. Any changes in hardware should be immediately reflected here. Likewise, if you're tuning system performance, this is often the first place to look for tweaking timing variables.

## Function Declarations

### Code Snippet

```cpp
void initializePins();
void manageOperation();
uint8_t readReg(uint8_t reg, const void * pBuf, size_t size);
bool writeReg(uint8_t reg, const void * pBuf, size_t size);
```

### Brief Overview

- **`void initializePins();`**: Sets up the GPIO pins. Expect to see `pinMode()` calls to configure each pin as INPUT or OUTPUT.
  
- **`void manageOperation();`**: Handles the core logic of the system. This function is where the main decisions based on sensor data are made.
  
- **`uint8_t readReg(uint8_t reg, const void * pBuf, size_t size);`**: Reads data from an I2C device. Will use I2C-related commands.
  
- **`bool writeReg(uint8_t reg, const void * pBuf, size_t size);`**: Writes data to an I2C device. Similar in function to `readReg()` but for writing.

Declaring these functions at the beginning of the code makes it modular and easier to manage, allowing for specific functionalities to be isolated and debugged more easily.

## Global Variables

In this section, we'll discuss the global variables used in the code and how they are utilized to control the various components and manage states. Global variables hold the information that is shared among different functions and states in the Arduino program. Below is a brief overview of each variable:

### 1. `Bounce debouncer` and `Bounce debouncerWash`
- These are objects of the Bounce library used for debouncing signals from buttons or sensors. The `debouncer` is used for the liquid level sensor, and `debouncerWash` is used for the wash button.

### 2. Time-related Unsigned Long Variables
- `previousMillis`, `lastMotorStartTime`, `mixerStartTime`, `lastSensorReadingTime`, `sensorHighStartTime`, `lastPrintTime`, `distanceErrorStart`
  - These variables are used for timing purposes, whether it's for debouncing, keeping track of when a motor started, or measuring the time a sensor has been active.

### 3. Boolean State Variables
- `mixerRunning`, `isSensorHigh`, `isDelayComplete`, `isWashing`, `wasWashing`
  - These Boolean variables are used to store the state of various components and processes. For example, `mixerRunning` stores whether the mixer is running.

### 4. Buffer and Data Variables
- `uint8_t buf[2] = { 0 };`
  - This is the buffer used for storing data read from the I2C sensor.
- `uint8_t dat = 0xB0;`
  - This variable holds the data to be written to a register of an I2C device.

### 5. `int distance`
  - Used for storing the distance measured by the sensor.

These global variables play an essential role in the function and control logic of the code. The key advantage of using global variables is that they enable sharing information seamlessly between different functions, thereby facilitating modular coding practices. However, their scope should be managed carefully to prevent unintended side-effects.## 5. Global Variables

In this section, we'll discuss the global variables used in the code and how they are utilized to control the various components and manage states. Global variables hold the information that is shared among different functions and states in the Arduino program. Below is a brief overview of each variable:

### 1. `Bounce debouncer` and `Bounce debouncerWash`
- These are objects of the Bounce library used for debouncing signals from buttons or sensors. The `debouncer` is used for the liquid level sensor, and `debouncerWash` is used for the wash button.

### 2. Time-related Unsigned Long Variables
- `previousMillis`, `lastMotorStartTime`, `mixerStartTime`, `lastSensorReadingTime`, `sensorHighStartTime`, `lastPrintTime`, `distanceErrorStart`
  - These variables are used for timing purposes, whether it's for debouncing, keeping track of when a motor started, or measuring the time a sensor has been active.

### 3. Boolean State Variables
- `mixerRunning`, `isSensorHigh`, `isDelayComplete`, `isWashing`, `wasWashing`
  - These Boolean variables are used to store the state of various components and processes. For example, `mixerRunning` stores whether the mixer is running.

### 4. Buffer and Data Variables
- `uint8_t buf[2] = { 0 };`
  - This is the buffer used for storing data read from the I2C sensor.
- `uint8_t dat = 0xB0;`
  - This variable holds the data to be written to a register of an I2C device.

### 5. `int distance`
  - Used for storing the distance measured by the sensor.

## Function Descriptions

In this section, we'll delve into the various functions used in the Arduino program. These functions are designed to manage the hardware components and application logic of the system. We'll discuss what each function does and how they contribute to the overall functionality.

### 1. `void initializePins()`

This function is responsible for setting up the initial state of the GPIO pins on the Arduino. It defines whether each pin will be used for input or output, and in some cases, activates the internal pull-up resistors. This is a crucial step because the behavior of the program depends on these settings.

```cpp
void initializePins() {
  pinMode(AUGER_MOTOR_PWM_PIN, OUTPUT);  // Auger Motor Pin as Output
  // More pin setups here...
  debouncerWash.attach(WASH_PIN, INPUT_PULLUP);  // Attach Wash Pin to debouncer with Pull-Up
  debouncer.attach(LIQUID_LEVEL_SENSOR_PIN, INPUT_PULLUP);  // Attach Liquid Level Sensor Pin to debouncer with Pull-Up
  debouncer.interval(100);  // Debounce interval for Liquid Level Sensor
  debouncerWash.interval(100);  // Debounce interval for Wash Pin
}
```

### 2. `void setup()`

The `setup()` function is a standard Arduino function that runs once at the beginning of the program. Here, we initialize the Serial communication for debugging purposes, configure the I2C (Inter-Integrated Circuit) communication, and call the `initializePins()` function.

```cpp
void setup() {
  Serial.begin(9600);  // Initialize Serial communication
  Wire.begin();  // Initialize I2C communication
  initializePins();  // Call initializePins to set up GPIO
}
```

### 3. `void manageOperation()`

This function handles the core logic of the application. It checks sensor states and based on them, calls other functions to start or stop motors, relays, etc. It also implements a delay mechanism for the liquid level sensor. 

```cpp
void manageOperation() {
  int sensorState = debouncer.read();  // Read the state of the liquid level sensor
  // Handling delays and more logic here...
  if (isDelayComplete) {
    startMotors();  // Function to start motors
    turnOnWaterRelay();  // Function to turn on water relay
    // More operations...
  } else {
    resetOperations();  // Function to reset all operations
  }
}
```

### Other Functions

#### 1. `void resetOperations()`

This function is responsible for stopping all the motors and relays. It's usually called when certain conditions are not met.

#### 2. `void startMotors()` and `void stopMotors()`

These functions are self-explanatory; they start and stop the motors, respectively.

#### 3. `void turnOnWaterRelay()` and `void turnOffWaterRelay()`

These functions control the water relay, either turning it on or off.

#### 4. `void countSensorHighTime()`

This function counts the time the liquid level sensor has been high and takes specific actions if it's high for too long.

Each function has a well-defined role in the operation of the system. Structuring the code in this way keeps it modular, which makes debugging and adding new features easier.## 6. Function Descriptions

In this section, we'll delve into the various functions used in the Arduino program. These functions are designed to manage the hardware components and application logic of the system. We'll discuss what each function does and how they contribute to the overall functionality.

### 1. `void initializePins()`

This function is responsible for setting up the initial state of the GPIO pins on the Arduino. It defines whether each pin will be used for input or output, and in some cases, activates the internal pull-up resistors. This is a crucial step because the behavior of the program depends on these settings.

```cpp
void initializePins() {
  pinMode(AUGER_MOTOR_PWM_PIN, OUTPUT);  // Auger Motor Pin as Output
  // More pin setups here...
  debouncerWash.attach(WASH_PIN, INPUT_PULLUP);  // Attach Wash Pin to debouncer with Pull-Up
  debouncer.attach(LIQUID_LEVEL_SENSOR_PIN, INPUT_PULLUP);  // Attach Liquid Level Sensor Pin to debouncer with Pull-Up
  debouncer.interval(100);  // Debounce interval for Liquid Level Sensor
  debouncerWash.interval(100);  // Debounce interval for Wash Pin
}
```

### 2. `void setup()`

The `setup()` function is a standard Arduino function that runs once at the beginning of the program. Here, we initialize the Serial communication for debugging purposes, configure the I2C (Inter-Integrated Circuit) communication, and call the `initializePins()` function.

```cpp
void setup() {
  Serial.begin(9600);  // Initialize Serial communication
  Wire.begin();  // Initialize I2C communication
  initializePins();  // Call initializePins to set up GPIO
}
```

### 3. `void manageOperation()`

This function handles the core logic of the application. It checks sensor states and based on them, calls other functions to start or stop motors, relays, etc. It also implements a delay mechanism for the liquid level sensor. 

```cpp
void manageOperation() {
  int sensorState = debouncer.read();  // Read the state of the liquid level sensor
  // Handling delays and more logic here...
  if (isDelayComplete) {
    startMotors();  // Function to start motors
    turnOnWaterRelay();  // Function to turn on water relay
    // More operations...
  } else {
    resetOperations();  // Function to reset all operations
  }
}
```

### Other Functions

#### 1. `void resetOperations()`

This function is responsible for stopping all the motors and relays. It's usually called when certain conditions are not met.

#### 2. `void startMotors()` and `void stopMotors()`

These functions are self-explanatory; they start and stop the motors, respectively.

#### 3. `void turnOnWaterRelay()` and `void turnOffWaterRelay()`

These functions control the water relay, either turning it on or off.

#### 4. `void countSensorHighTime()`

This function counts the time the liquid level sensor has been high and takes specific actions if it's high for too long.

## The `loop()` Function

The `loop()` function serves as the heartbeat of any Arduino program. It's where the program spends the majority of its time and is essentially a forever-running loop, where all the scheduled tasks and event-driven activities occur. Here, we will dissect what each line of the `loop()` function does in your program.

```cpp
void loop() {
  debouncer.update();          // Update the debouncer for the liquid level sensor
  debouncerWash.update();      // Update the debouncer for the wash pin
  handleWashFunction();        // Manage washing state
  
  if(isWashing) return;        // Exit the loop early if in washing mode
  
  manageOperation();           // Manage motors and relays based on sensor state
  checkDistanceError();        // Check if distance exceeds threshold and handle accordingly
  
  // Check if one second has passed; note that this will also work if millis() wraps
  if(millis() - lastPrintTime >= 1000) {  
    lastPrintTime = millis();  // Update lastPrintTime
    printDistance();           // Print the current distance
  }
}
```

### Explanation

1. **Debouncer Updates**:  
   The `debouncer.update()` and `debouncerWash.update()` functions update the debouncing mechanisms for both the liquid level sensor and the wash pin. Debouncing is crucial for smoothing out the noise in digital input signals.

2. **Washing State**:  
   `handleWashFunction()` is called to manage the washing state of the system. Depending on whether the washing state is activated or not, this function will modify the `isWashing` flag.

3. **Early Exit**:  
   The line `if(isWashing) return;` allows the function to exit early if the system is in washing mode. This is an effective way to prevent other functionalities from being executed when the system should only focus on washing.

4. **Operational Management**:  
   `manageOperation()` is invoked next, which handles the operation of motors and relays based on sensor inputs. This is the core function that drives your application logic.

5. **Distance Error Check**:  
   `checkDistanceError()` will check if the distance (presumably measured by some sensor) exceeds a certain threshold. If it does, the function will handle it accordingly (not shown here, but we assume it is defined elsewhere in the code).

6. **Timed Actions**:  
   The block of code `if(millis() - lastPrintTime >= 1000)` checks whether one second has passed since the last print action. If so, it updates the last print time and calls `printDistance()` to print the current distance. The use of `millis()` allows this to be non-blocking, which is excellent for multitasking.

### Conclusion

Thank you for taking the time to go through this README. We've covered the essential aspects of the codebase to give you a solid understanding of how this Arduino project operates. From the initialization of pins and setup to real-time operations and distance checking, each component has been designed to work in a cohesive, modular manner. 

We encourage you to delve into the code and experiment with it. If you have any questions or find areas for improvement, please feel free to contribute or ask questions. Your involvement will undoubtedly make the project better for everyone.

Happy Coding!### Conclusion for README

Thank you for taking the time to go through this README. We've covered the essential aspects of the codebase to give you a solid understanding of how this Arduino project operates. From the initialization of pins and setup to real-time operations and distance checking, each component has been designed to work in a cohesive, modular manner. 

We encourage you to delve into the code and experiment with it. If you have any questions or find areas for improvement, please feel free to contribute or ask questions. Your involvement will undoubtedly make the project better for everyone.

Happy Coding!