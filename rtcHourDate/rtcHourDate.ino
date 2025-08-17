#include <Wire.h>
#include "RTClib.h"

RTC_DS3231 rtc;

void setup() {
  Serial.begin(9600);
  
  if (!rtc.begin()) {
    Serial.println("No se detecta el módulo RTC");
    while (1);
  }

  // SOLO EJECUTAR ESTA LÍNEA LA PRIMERA VEZ PARA CONFIGURAR LA HORA
   //rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Ajusta fecha/hora del PC
  
  // O puedes ajustar manualmente así:
  // rtc.adjust(DateTime(2025, 8, 7, 22, 45, 0)); // Año, mes, día, hora, minuto, segundo
}

void loop() {
  DateTime now = rtc.now();

  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();

  delay(1000);
}
