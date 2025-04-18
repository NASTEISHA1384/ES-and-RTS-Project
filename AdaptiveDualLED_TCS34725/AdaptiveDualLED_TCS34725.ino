// Project: Dual-LED Adaptive Lighting System using TCS34725 Sensor
// Description: Controls white and yellow LEDs based on ambient lighting
// Author: Mohammad (Cyber & EW Student)
// Hardware: Arduino + TCS34725 + 2 PWM LEDs
// Version: v1.0 (Initial Public Release)
// Date: 2025-04-18

#include <Wire.h>
#include "Adafruit_TCS34725.h"

// Initialize the TCS34725 color sensor
Adafruit_TCS34725 tcs = Adafruit_TCS34725(
  TCS34725_INTEGRATIONTIME_50MS,
  TCS34725_GAIN_4X
);

// Pin Definitions
const int yellowLEDPin = 9;
const int whiteLEDPin  = 10;
const int testPin      = 3;  // Debug/test LED (Always ON)

// System Settings
const float targetYellowRatio = 0.6;     // Desired yellow light ratio
const float L_min = 150.0;               // Minimum comfortable light
const float L_max = 1000.0;              // Maximum comfortable light
const int maxLEDIntensity = 255;         // 8-bit PWM max
const int default_k = 50;                // Fallback default LED base value

// State Memory
int prevYellowLED = 0;
int prevWhiteLED = 0;
bool inFallbackMode = false;

void setup() {
  Serial.begin(9600);

  if (tcs.begin()) {
    Serial.println("TCS34725 found");
  } else {
    Serial.println("No TCS34725 found ... check your connections");
    while (1);
  }

  pinMode(yellowLEDPin, OUTPUT);
  pinMode(whiteLEDPin, OUTPUT);
  pinMode(testPin, OUTPUT);
}

void loop() {
  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c);

  // Estimate ambient colors
  float ambientWhite  = (float)min(r, min(g, b));                 // White = minimum of RGB
  float ambientYellow = ((float)r + (float)g) / 2.0;              // Yellow = average of red & green

  // Overall intensity with contribution from LEDs
  float YOI = (float)prevYellowLED + ambientYellow;               // Yellow Output Intensity
  float WOI = (float)prevWhiteLED  + ambientWhite;                // White Output Intensity
  float total = YOI + WOI;

  float currentYellowRatio = (total > 0) ? YOI / total : -1;
  float ambientTotal = ambientYellow + ambientWhite;

  // Debug Information
  Serial.println("\n-- New Loop Iteration --");
  Serial.print("Raw R: "); Serial.print(r);
  Serial.print(" G: "); Serial.print(g);
  Serial.print(" B: "); Serial.print(b);
  Serial.print(" C: "); Serial.println(c);
  Serial.print("Ambient White: "); Serial.println(ambientWhite);
  Serial.print("Ambient Yellow: "); Serial.println(ambientYellow);
  Serial.print("Ambient Total: "); Serial.println(ambientTotal);
  Serial.print("YOI: "); Serial.println(YOI);
  Serial.print("WOI: "); Serial.println(WOI);
  Serial.print("Total: "); Serial.println(total);
  Serial.print("Current Yellow Ratio: "); Serial.println(currentYellowRatio);

  // Initialize new LED values
  int newYellowLED = prevYellowLED;
  int newWhiteLED  = prevWhiteLED;

  // Comfort Band Logic
  const float hysteresis = 2.0;
  bool isTooDark     = ambientTotal < (L_min - hysteresis);
  bool isTooBright   = ambientTotal > (L_max + hysteresis);
  bool isComfortable = !isTooDark && !isTooBright;

  // Special override during fallback
  if (inFallbackMode && isTooBright) {
    Serial.println("Override: Fallback mode light interpreted as 'too bright' — SKIPPING");
    isTooBright = false;
    isComfortable = true;
  }

  // --- Mode 1: Sensor in complete darkness ---
  if (currentYellowRatio == -1) {
    Serial.println("Mode: Darkness Fallback");
    newYellowLED = default_k * targetYellowRatio;
    newWhiteLED  = default_k * (1.0 - targetYellowRatio);
    inFallbackMode = true;
  }

  // --- Mode 2 & 3: Too Dark or Too Bright — Downscale both LEDs proportionally ---
  else if (isTooDark || isTooBright) {
    Serial.println(isTooDark ? "Light Zone: Too Dark" : "Light Zone: Too Bright");

    float scalingFactor = 0.9;  // Simple proportional downscale
    newYellowLED = constrain(round(prevYellowLED * scalingFactor), 0, maxLEDIntensity);
    newWhiteLED  = constrain(round(prevWhiteLED  * scalingFactor), 0, maxLEDIntensity);

    // Maintain ratio within tight band
    float ratio = (newYellowLED + newWhiteLED > 0) ? 
                  (float)newYellowLED / (newYellowLED + newWhiteLED) : -1;

    while ((ratio < 0.58 || ratio > 0.62)) {
      if (ratio > 0.62) {
        newYellowLED = constrain(round(newYellowLED * scalingFactor), 0, maxLEDIntensity);
      } else if (ratio < 0.58) {
        newWhiteLED = constrain(round(newWhiteLED * scalingFactor), 0, maxLEDIntensity);
      }
      ratio = (newYellowLED + newWhiteLED > 0) ? 
              (float)newYellowLED / (newYellowLED + newWhiteLED) : -1;
    }

    // Cutoff for very low values
    if (newYellowLED <= 5) { newYellowLED = 0; newWhiteLED = 0; }
    if (newWhiteLED  <= 5) { newYellowLED = 0; newWhiteLED = 0; }
  }

  // --- Mode 4: Within Comfort Band — Fine-tune ratio ---
  else {
    Serial.println("Light Zone: Comfort Band");

    // Yellow too low → Increase Yellow
    if (currentYellowRatio < 0.58) {
      Serial.println("Mode: Comfortable - Increase Yellow Light");
      float adjust = 0.1 * (0.6 - currentYellowRatio) * maxLEDIntensity;
      newYellowLED = constrain(prevYellowLED + round(adjust), 0, maxLEDIntensity);
    }

    // Yellow too high → Increase White
    else if (currentYellowRatio > 0.62) {
      Serial.println("Mode: Comfortable - Increase White Light");
      float adjust = 0.1 * (currentYellowRatio - 0.6) * maxLEDIntensity;
      newWhiteLED = constrain(prevWhiteLED + round(adjust), 0, maxLEDIntensity);
    }
  }

  // Final LED Output
  analogWrite(yellowLEDPin, newYellowLED);
  analogWrite(whiteLEDPin, newWhiteLED);
  analogWrite(testPin, 255); // Keep always on for test/debug visibility

  // Print final output values
  Serial.print("Output Yellow LED: "); Serial.println(newYellowLED);
  Serial.print("Output White LED: "); Serial.println(newWhiteLED);

  // Update previous state
  prevYellowLED = newYellowLED;
  prevWhiteLED  = newWhiteLED;

  // Exit fallback mode if no longer needed
  if (!isTooDark && currentYellowRatio != -1) {
    inFallbackMode = false;
  }

  delay(100);
}
