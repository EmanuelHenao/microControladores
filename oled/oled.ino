#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1   // Reset pin # (or -1 if sharing Arduino reset pin)

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(9600);
  if(!display.begin(0x3C, true)) { // Address 0x3C, with reset
    Serial.println(F("SH1106 allocation failed"));
    for(;;); // Don't proceed, halt the code.
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println(F("Sistema Riego"));
  display.println(F("Orellanas v2.0"));
  display.display();
}

void loop() {
  // Your code here
}