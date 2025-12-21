#pragma once
#ifndef _VERSION_h
#define _VERSION_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#if defined(ESP32)
#include "ChipID.h"
#endif

// Software Definitions
#define MODEL "EANx O2 Upcycler"
#define VERSION "0.5.3 Beta"
#define FILE "EANx_Upcycler on Platform IO"

/*
 * One Route is to Embed the Date and time from GCC
 * These Are dynamically replaced at build time so are dynamically embedded in your project without external tools
 * https ://gcc.gnu.org/onlinedocs/cpp/Standard-Predefined-Macros.html
 */

#define VAL_DATE __DATE__
#define VAL_TIME __TIME__

/*
 * This can be expanded with specific defines from your Toolchain or Own Code
 */

/*#if defined(ARDUINO_AVR_UNO)
    #define VAL_MCU "AVR_ATmega328p"
#elif defined (ARDUINO_ARCH_ESP32)
    #define VAL_MCU "ESP32"
#else
    #define VAL_MCU "Unknown"
#endif */

void printVersionToSerial()
{
    Serial.println(F("---------------------------------------------------"));
    Serial.println(F("Software Build"));
    Serial.println(F("---------------------------------------------------"));
    // Output our Validation from GCC Variables
    Serial.print(F("Date:\t"));
    Serial.println(VAL_DATE);
    Serial.print(F("Time:\t"));
    Serial.println(VAL_TIME);

    // Output our Validation from Toolchain and Library Information
    Serial.print(F("MCU:\t"));
    Serial.println(VAL_MCU);

#ifdef ESP32
    ChipID();
#endif

    // Output model and version info
    Serial.print(F("Model:\t"));
    Serial.println(MODEL);

    Serial.print(F("Ver:\t"));
    Serial.println(VERSION);

    Serial.print(F("File:\t"));
    Serial.println(FILE);

    Serial.println(F("---------------------------------------------------"));
}

#endif