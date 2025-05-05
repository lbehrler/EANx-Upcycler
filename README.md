# EANx-Upcycler

This proeject is the enhanced feature version on Platform IO of this project https://github.com/lbehrler/EANx-Analyzer-Arduino

# EANx-Analyzer-Arduino / ESP32 

> **Note:** If you have an issue with this project, please review your hardware docuemntation first.  If you are still having an issue or have a software specific issue, please open an issue [here](https://github.com/lbehrler/EANx-Upcycler/issues).

EANx Analysis with output to an OLED color display.
The code can be customized to match a wide variety of hardware.  The current version in the main branch is setup to use an ESP32-Pico-D4, a Seeeduino Xiao, or an Arduino Nano with an SPI interface OLED screen 240 x 240.

In its simplest form the EANx Analyzer reads an analog input from an ADS1115, converts it to voltage, grabs a running average of ADC values and and prints the result to the display and debug to Serial Monitor.

The EANx Analyzer project is meant to be a DIY O2 Analyzer.  

## Disclaimers

**Using or monitoring oxygen levels above atmospheric composition (20.9%) can be hazardous. While oxygen is not combustible or explosive, its presence accelerates and promotes combustion. High levels of oxygen will make most materials combustible including metals. Do not use oxygen levels above 20.9% in the presence of any flame or ignition source.**

**Use at your own risk.  Inaccurate gas analysis can lead to serious personal injury or death.**  

If you are using the device and software for critical purposes, please seek proper training and know the limits of devices of this type. By using this analyzer to analyze breathing gas, you assume all risks for the use of analyzed gas and the risk from any inferences made from the output of the device, both above and below the water.

## Acknowledgements
Some of this code is adapted from other EANx project scripts: 
  - https://github.com/ppppaoppp/DIY-Nitrox-Analyzer-04_12_2019.git
  - https://github.com/ejlabs/arduino-nitrox-analyzer.git

## Basic Operations 
Unit is rechargeable with any 5-20v USB-C negotiating charger (i.e. phone charger).  The unit has a battery meter in the upper right of the screen for the 16340 battery status, and in the upper left for the sensor unit.   If the sensor unit is yellow or red, please replace the O2 sensor, if the battery monitor is yellow or red please fully charge the unit before use. 

### Firmware and Package Upates
The recommended method for firmware updates or to reflash your device is to use the browser based updater here:  ([Firmware Update Page](https://lbehrler.github.io/EANxUpcycler.github.io/)).

Alternately updates can be made using Expressif's ESP Flash Download Tool or an online Serial Connection tool (https://serial.huhn.me/) to update the firmware on your device.  Do so with caution the firmware portion starts at 0x10000 memory position. 



## Project Files

The project folder contains several files and one folder:

+ `LICENSE` - The license file for this project
+ `readme.md` - This file.
+ `EANx_Analyzer_XXXXXXX.ino` - the basic application.
+ `pin_config.h` - Header file for various ESP32 / Arduino pinout settings. 
+ `OTA.h` - Simple OTA upate header (optional utility).
+ `bat_stat.h` - Simple Battery status script for ESP32 based boards.

## Configuration
Depending on your hardware configuration and components used you will have to set your pin definitions in the header files. 

If you are using a basic ST3%XX or ST13XX series display you can use [U8g2](https://github.com/olikraus/u8g2) or the Adafruit librarires. For more advance display features and better efficiency the [Bodmer TFT eSPI library](https://github.com/Bodmer/TFT_eSPI) can be configured allowing for greater options. If you are using the EANx Analyzer OLED TFT eSPI.ino then the TFT eSPI library is required. 

## Revision History
+ 11/29/2022 - initial release
+ 12/30/2022 - prototype release
+ 01/03/2023 - custom PCB prototype release
+ 07/20/2023 - custom PCB option ESP32 release

## Schematics 

*Seeeduino Xiao version with SPI OLED:*
![](https://github.com/lbehrler/EANx-Analyzer/blob/645330fc3275fe3a1c8c88061cc2e68e7b1bfda9/Seeed_Xiao_EANx_Analyzer_SPI_OLED%20schematic.png)

## Prototype

The current prototype unit is built using:
+ LilyGo-T-OI-PLUS [ESP32 C3 with 16340 Battery holder](https://github.com/Xinyuan-LilyGO/LilyGo-T-OI-PLUS)
+ Generic ADS1115 
+ SPI Organic light-emitting diode (OLED) ST7789
+ O2 Sensor
+ 16340 Battery
+ Custom PCB fit to project case
+ Project case

*Prototype 4*

![Custom PCB version](https://lh3.googleusercontent.com/pw/AIL4fc_xV2YWr-FOy9fTgCVv1qJvXfss7XXug2cmfTwabwHQb4MrrQRQn25uYb2qKOo2hIIW4s7WPj61GEHNv8foAk6tiZ7ztep2YEQh7d-Uges87c64GSATMVJgouoNBcy8N-WcfUyh_iExNov7f3sfYL7hUw=w525-h933-s-no?authuser=0)


