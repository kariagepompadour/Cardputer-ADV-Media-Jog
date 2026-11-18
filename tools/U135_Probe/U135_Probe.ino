/*
  U135_Probe — Cardputer ADV + Unit Encoder (U135) minimal check

  Base:   m5stack/M5Unit-Encoder  examples/Unit_Encoder_M5Core2/Unit_Encoder_M5Core2.ino
          Original example: Copyright (c) 2021 by M5Stack.
          Licensed under the MIT License; see LICENSES/M5Stack-MIT.txt.
  Modifications: Copyright (c) 2026 Kariage POMPADOUR Entertainment Corporation. MIT License; see LICENSE.
  Display init: copied from SOUND TERRARIUM setup() (proven on this Cardputer ADV)

  Scope (only):
    1. detect rotation in one direction
    2. detect rotation in the other direction
    3. detect the push button
  Not done: LED writes, Mode register writes, USB/BLE HID, keyboard, audio.
  This sketch only READS from the unit (registers 0x10 and 0x20).
  Reads use readRegChecked(): the library's readBytes() sequence plus result
  checks (v2, after review). The library is still used for begin() / constants.

  Wiring: Grove PORT on Cardputer ADV -> U135
    G2 = SDA (yellow), G1 = SCL (white), 5V (red), GND (black)

  Board: M5Stack board package "M5Cardputer" (m5stack:esp32:m5stack_cardputer)
*/
#include <M5Cardputer.h>
#include <Wire.h>
#include "Unit_Encoder.h"

// Port pins per M5Unified pin table for board_M5CardputerADV (EX SCL=G1, SDA=G2).
static constexpr uint8_t PORT_SDA = 2;
static constexpr uint8_t PORT_SCL = 1;
static constexpr uint32_t I2C_FREQ = 200000;  // library default, unchanged

Unit_Encoder sensor;

static bool unitFound = false;
static int16_t lastValue = 0;
static uint8_t lastButton = 0;
static int32_t plusEvents = 0;
static int32_t minusEvents = 0;
static int32_t buttonChanges = 0;
static const char* lastEvent = "-";
static uint32_t lastProbeMs = 0;

static uint32_t readErrors = 0;

static bool probeUnit()
{
    Wire.beginTransmission(ENCODER_ADDR);
    return Wire.endTransmission() == 0;
}

// Same transaction sequence as Unit_Encoder::readBytes() (M5Unit-Encoder 0.0.2),
// but returns false instead of passing 0xFF filler bytes on as data.
static bool readRegChecked(uint8_t reg, uint8_t* buf, uint8_t len)
{
    Wire.beginTransmission(ENCODER_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) return false;
    if (Wire.requestFrom((uint8_t)ENCODER_ADDR, len) != len) return false;
    for (uint8_t i = 0; i < len; i++) {
        int b = Wire.read();
        if (b < 0) return false;
        buf[i] = (uint8_t)b;
    }
    return true;
}

static bool readEncoder(int16_t& value)
{
    uint8_t d[2];
    if (!readRegChecked(ENCODER_REG, d, 2)) return false;
    value = (int16_t)(d[0] | (d[1] << 8));
    return true;
}

// Register 0x20 is documented as 0 or 1; anything else is rejected.
static bool readButtonRaw(uint8_t& raw)
{
    uint8_t d;
    if (!readRegChecked(BUTTON_REG, &d, 1)) return false;
    if (d > 1) return false;
    raw = d;
    return true;
}

static void drawStatus()
{
    auto& d = M5Cardputer.Display;
    d.fillScreen(TFT_BLACK);
    d.setTextSize(2);
    d.setCursor(4, 4);
    if (!unitFound) {
        d.setTextColor(TFT_RED, TFT_BLACK);
        d.println("U135 NOT FOUND");
        d.setTextSize(1);
        d.setTextColor(TFT_WHITE, TFT_BLACK);
        d.println();
        d.println(" I2C 0x40 on G2(SDA)/G1(SCL)");
        d.println(" retry every 1 s");
        return;
    }
    d.setTextColor(TFT_GREEN, TFT_BLACK);
    d.printf("VAL  %d\n", lastValue);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.printf("BTN  raw=%u\n", lastButton);
    d.printf("+ %ld  - %ld\n", (long)plusEvents, (long)minusEvents);
    d.printf("BTN chg %ld\n", (long)buttonChanges);
    d.printf("ERR %lu\n", (unsigned long)readErrors);
    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.printf("%s\n", lastEvent);
}

// Returns false if either initial read fails; the caller then treats the unit as not found.
static bool readInitialState()
{
    if (!readEncoder(lastValue) || !readButtonRaw(lastButton)) {
        readErrors++;
        Serial.println("[U135] initial read failed");
        return false;
    }
    Serial.printf("[U135] found. initial value=%d button_raw=%u\n", lastValue, lastButton);
    return true;
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    // Same LCD-only init as SOUND TERRARIUM: no M5Cardputer.begin() / M5.begin().
    M5Cardputer.Display.begin();
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(128);
    M5Cardputer.Display.fillScreen(TFT_BLACK);

    // Wire = I2C_NUM_0 on the external port. The internal bus (G8/G9) is not touched.
    sensor.begin(&Wire, ENCODER_ADDR, PORT_SDA, PORT_SCL, I2C_FREQ);

    unitFound = probeUnit();
    Serial.printf("[U135] probe 0x40: %s\n", unitFound ? "ACK" : "no ACK");
    if (unitFound) {
        unitFound = readInitialState();
    }
    drawStatus();
    lastProbeMs = millis();
}

void loop()
{
    uint32_t now = millis();

    if (!unitFound) {
        if (now - lastProbeMs >= 1000) {
            lastProbeMs = now;
            unitFound = probeUnit();
            Serial.printf("[U135] probe 0x40: %s\n", unitFound ? "ACK" : "no ACK");
            if (unitFound) {
                unitFound = readInitialState();
                drawStatus();
            }
        }
        delay(20);
        return;
    }

    // Detect unplug without reading garbage from a missing device.
    if (now - lastProbeMs >= 500) {
        lastProbeMs = now;
        if (!probeUnit()) {
            unitFound = false;
            Serial.println("[U135] lost (no ACK)");
            drawStatus();
            return;
        }
    }

    bool changed = false;

    // A failed read is counted and skipped; it never becomes a rotation/button event.
    int16_t value = lastValue;
    if (!readEncoder(value)) {
        readErrors++;
        Serial.println("[U135] encoder read failed");
        changed = true;
    }
    int16_t delta = (int16_t)(value - lastValue);  // wrap-safe for int16 counter
    if (delta > 0) {
        plusEvents++;
        lastEvent = "ROTATE +";
        changed = true;
    } else if (delta < 0) {
        minusEvents++;
        lastEvent = "ROTATE -";
        changed = true;
    }
    if (delta != 0) {
        Serial.printf("[U135] value=%d delta=%d\n", value, delta);
        lastValue = value;
    }

    // Polarity is not fixed here: protocol PDF says 1=Pressed, the official
    // example treats 0 as pressed. Report raw changes and confirm on hardware.
    uint8_t button = lastButton;
    if (!readButtonRaw(button)) {
        readErrors++;
        Serial.println("[U135] button read failed");
        changed = true;
    }
    if (button != lastButton) {
        buttonChanges++;
        lastEvent = button ? "BTN raw 0->1" : "BTN raw 1->0";
        Serial.printf("[U135] button_raw %u -> %u\n", lastButton, button);
        lastButton = button;
        changed = true;
    }

    if (changed) {
        drawStatus();
    }
    delay(20);  // same period as the official example
}
