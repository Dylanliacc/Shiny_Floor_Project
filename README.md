# Shiny_Floor_Project
seeed Showcase Project


This project is designed to create a tunnel lighting effect with dual-mirror imaging, incorporating various LED lighting effects for product displays within a transparent floor. The project is structured to work with CMake and the ESP-IDF framework.

## Project Structure


- glasslight strip
- led strip
- main_prj
- tcp client
- readme.md

## Structure Descriptions

- glasslight strip: This directory contains code and configurations for testing the LED strip specifically used for the transparent floor lighting.
- led strip: This directory contains code and configurations for testing individual LED boards.
- main_prj: This is the final project directory that integrates all components and configurations for the complete tunnel lighting and display system.
- tcp client: This directory contains code for testing the TCP client functionality separately.
- readme.md: This file provides an overview and instructions for the project.

### Core File Descriptions

- **CMakeLists.txt**: This file configures the build process using CMake. It registers the source files and includes directories necessary for the project.
- **led_strip_encoder.c**: This source file contains the implementation of functions to control the LED strip.
- **led_strip_encoder.h**: This header file declares the functions and definitions used in `led_strip_encoder.c`.
- **led_strip_example_main.c**: This is the main program file that demonstrates how to use the functions provided in `led_strip_encoder.c` to control the LED strip.

## Data Protocol

The data protocol allows the upper computer (TCP server) to deploy different port numbers to control multiple control boards. The protocol supports five commands: `off`, `constant`, `breath`, `rainbow`, and `rgb [ ] [ ] [ ]`.

### Commands

1. **off**: Turn off the LED strip.
    
    - **Example**: `off`
2. **constant**: Set the LED strip to a constant color.
    
    - **Example**: `constant 255 0 0` (This sets the LED strip to a constant red color.)
3. **breath**: Set the LED strip to a breathing effect (fading in and out).
    
    - **Example**: `breath 0 255 0` (This sets the LED strip to a green breathing effect.)
4. **rainbow**: Set the LED strip to a rainbow effect.
    
    - **Example**: `rainbow`
5. **rgb [ ] [ ] [ ]**: Set the LED strip to a custom RGB color.
    
    - **Example**: `rgb 0 0 255` (This sets the LED strip to blue.)

### Control Example

Each control board listens on a specific TCP port. The upper computer can send commands to these ports to control the LED strips connected to different control boards.

- **TCP Server Configuration**:
    
    - Control Board 1: `Port 3333`
    - Control Board 2: `Port 3334`
    - Control Board 3: `Port 3335`
- **Sending Commands**:
    
    - To turn off the LED strip on Control Board 1:
        - Send `off` to `Port 3333`
    - To set the LED strip on Control Board 2 to a constant blue color:
        - Send `constant 0 0 255` to `Port 3334`
    - To set the LED strip on Control Board 3 to a rainbow effect:
        - Send `rainbow` to `Port 3335`

The commands should be sent as plain text messages over the TCP connection to the respective port numbers of the control boards.

## Requirements

- ESP-IDF (Espressif IoT Development Framework)
- CMake (minimum version required by ESP-IDF)

## Building and Flashing

1. **Set up ESP-IDF**: Follow the instructions on the ESP-IDF Getting Started Guide to set up the ESP-IDF environment.
    
2. **Clone the Project**: Clone this repository to your local machine.
    
    
    `git clone https://github.com/your-repository/led_strip_encoder.git cd led_strip_encoder`
    
3. **Configure the Project**: Navigate to the project directory and configure the project using CMake.
    
    `idf.py set-target esp32 idf.py menuconfig`
    
4. **Build the Project**: Build the project using the following command.
    
    `idf.py build`
    
5. **Flash the Project**: Flash the project to your ESP32 board.
    
    `idf.py -p /dev/ttyUSB0 flash`
    
    (Replace `/dev/ttyUSB0` with the correct serial port for your board.)
    
6. **Monitor the Output**: Monitor the output from the serial port to see the debug and status messages.
    `idf.py monitor`
    

## Usage

The example main program initializes the LED strip and demonstrates basic operations such as setting colors and patterns. Modify the `led_strip_example_main.c` file to customize the behavior of the LED strip for your specific needs.


## License

This project is licensed under the MIT License. See the LICENSE file for details.
