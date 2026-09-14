#pragma once

#include <Arduino.h>

namespace Post {

// Each bit marks a FAILED block, so a fully healthy device reports mask 0.
constexpr uint8_t BIT_NVS    = 0;
constexpr uint8_t BIT_RADIO  = 1;
constexpr uint8_t BIT_ADC    = 2;
constexpr uint8_t BIT_BUTTON = 3;

// Runs all self-tests and stores the resulting mask. `radioInitOk` is the
// outcome of the radio initialisation performed by the caller, since the
// radio is brought up once during setup and must not be re-initialised here.
uint8_t run(bool radioInitOk, uint8_t buttonPin, uint8_t batteryAdcPin);

// Returns the mask produced by the last run(), for logging and telemetry.
uint8_t mask();

// Prints a human-readable breakdown of the last run to Serial.
void printReport();

} // namespace Post
