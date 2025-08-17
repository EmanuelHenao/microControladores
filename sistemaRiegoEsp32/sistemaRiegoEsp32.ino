// -------------------- LIBRERÍAS --------------------
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>
#include <DHT.h>

// -------------------- CONFIGURACIÓN HARDWARE (PINES ESP32-S3) --------------------
// Pines recomendados para la placa ESP32-S3
#define PIN_DHT 4      // Sensor de humedad y temperatura (puedes usar cualquier GPIO)
#define TIPO_DHT AM2301
#define PIN_RELE 5     // Módulo de relé
#define PIN_SD_CS 10   // Chip Select (CS) para la tarjeta SD

// Los pines I2C (SDA, SCL) y SPI (MOSI, MISO, SCK) suelen ser manejados
// por defecto por las librerías en el ESP32, pero puedes consultarlos en el pinout de tu placa.
// Por defecto en muchas placas S3: SDA=8, SCL=9 | SCK=12, MISO=13, MOSI=11

// -------------------- OBJETOS DE LIBRERÍAS ----------------------
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
DHT dht(PIN_DHT, TIPO_DHT);
RTC_DS3231 rtc;

// -------------------- VARIABLES DE ESTADO -----------------------
bool estadoRTC = false;
bool estadoSD = false;
bool estadoSensor = false;
bool estadoPantalla = false;

// -------------------- VARIABLES DE CONTROL ----------------------
float temperatura = 0.0;
float humedad = 0.0;
unsigned long ultimaLectura = 0;
unsigned long ultimoLog = 0;

// Rango de humedad objetivo
const float HUM_MIN = 80.0;
const float HUM_MAX = 85.0;

// Horario permitido de riego
const int HORA_INICIO_RIEGO = 10;
const int HORA_FIN_RIEGO = 23;

// -------------------- ICONOS (sin cambios) --------------------
static const unsigned char PROGMEM iconoOK[] = { 0x18, 0x18, 0x34, 0x34, 0x66, 0x66, 0xC3, 0xC3 };
static const unsigned char PROGMEM iconoError[] = { 0xC3, 0x66, 0x3C, 0x18, 0x18, 0x3C, 0x66, 0xC3 };

// -------------------- SETUP --------------------
void setup() {
  Serial.begin(115200); // El ESP32 soporta velocidades de comunicación más altas
  
  pinMode(PIN_RELE, OUTPUT);
  digitalWrite(PIN_RELE, LOW);

  Wire.begin(); // Inicia el bus I2C con los pines por defecto
  
  estadoPantalla = inicializarPantalla();
  estadoSD = inicializarSD();
  estadoRTC = inicializarRTC();
  estadoSensor = inicializarSensor();
}

// -------------------- LOOP --------------------
void loop() {
  unsigned long ahora = millis();

  // Tarea 1: Leer sensor, controlar riego y actualizar pantalla cada 2 segundos
  if (ahora - ultimaLectura >= 2000) {
    ultimaLectura = ahora;
    leerSensor();
    controlarRiego();
    actualizarPantalla();
  }

  // Tarea 2: Registrar datos en la SD cada 5 minutos
  if (ahora - ultimoLog >= 300000) {
    ultimoLog = ahora;
    registrarLog();
  }
}

// -------------------- FUNCIONES DE INICIALIZACIÓN --------------------

bool inicializarPantalla() {
  estadoPantalla = u8g2.begin();
  if (!estadoPantalla) {
    Serial.println("Error: OLED SH1106 no detectada");
    return false;
  }
  
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setCursor(0, 15);
    u8g2.print("Sistema de Riego");
    u8g2.setCursor(0, 35);
    u8g2.print("Orellanas v3.0 ESP32");
  } while (u8g2.nextPage());
  
  delay(1500);
  Serial.println("OK: OLED inicializada");
  return true;
}

bool inicializarRTC() {
  if (!rtc.begin()) {
    Serial.println("Error: RTC no detectado");
    return false;
  }
  if (rtc.lostPower()) {
    Serial.println("RTC sin hora, configurando con hora de compilacion...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  Serial.println("OK: RTC inicializado");
  return true;
}

bool inicializarSD() {
  if (!SD.begin(PIN_SD_CS)) {
    Serial.println("Error: SD no detectada");
    return false;
  }
  Serial.println("OK: SD inicializada");
  return true;
}

bool inicializarSensor() {
  dht.begin();
  delay(2000); // Tiempo para que el sensor se estabilice
  if (isnan(dht.readTemperature())) {
    Serial.println("Error: Sensor DHT no responde");
    return false;
  }
  Serial.println("OK: DHT inicializado");
  return true;
}

// -------------------- FUNCIONES PRINCIPALES --------------------

void mostrarIconosEstado(int yPos) {
  u8g2.drawXBMP(0, yPos, 8, 8, estadoRTC ? iconoOK : iconoError);
  u8g2.drawXBMP(12, yPos, 8, 8, estadoSD ? iconoOK : iconoError);
  u8g2.drawXBMP(24, yPos, 8, 8, estadoSensor ? iconoOK : iconoError);
  u8g2.drawXBMP(36, yPos, 8, 8, estadoPantalla ? iconoOK : iconoError);
}

void actualizarPantalla() {
  if (!estadoPantalla) return;

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_ncenB08_tr);

    // Dibuja la Fecha y Hora
    if (estadoRTC) {
      DateTime now = rtc.now();
      char buffer[20];
      sprintf(buffer, "%02d/%02d/%d", now.day(), now.month(), now.year());
      u8g2.setCursor(0, 10); u8g2.print(buffer);
      sprintf(buffer, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
      u8g2.setCursor(70, 10); u8g2.print(buffer);
    } else {
      u8g2.setCursor(0, 10); u8g2.print("RTC: Error");
    }

    // Dibuja Temperatura y Humedad
    if (estadoSensor) {
      char buffer[15];
      sprintf(buffer, "T: %.1f C", temperatura);
      u8g2.setCursor(0, 24); u8g2.print(buffer);
      sprintf(buffer, "H: %.1f %%", humedad);
      u8g2.setCursor(64, 24); u8g2.print(buffer);
    } else {
      u8g2.setCursor(0, 24); u8g2.print("Sensor: Error");
    }
    
    // Dibuja el estado del riego
    u8g2.setCursor(0, 38);
    u8g2.print("Riego: ");
    u8g2.print(digitalRead(PIN_RELE) ? "ON" : "OFF");

    // Dibuja los íconos de estado del sistema
    mostrarIconosEstado(54);
    
  } while (u8g2.nextPage());
}

void leerSensor() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  if (isnan(h) || isnan(t)) {
    if (estadoSensor) { // Si el fallo es nuevo, informa
      Serial.println("Fallo al leer del sensor DHT!");
      estadoSensor = false;
    }
  } else {
    humedad = h;
    temperatura = t;
    if (!estadoSensor) { // Si el sensor se recupera
      Serial.println("OK: Sensor DHT recuperado.");
      estadoSensor = true;
    }
  }
}

bool dentroHorarioRiego() {
  if (!estadoRTC) return false;
  DateTime ahora = rtc.now();
  // Corregido para incluir la hora de inicio y excluir la hora de fin
  return (ahora.hour() >= HORA_INICIO_RIEGO && ahora.hour() < HORA_FIN_RIEGO);
}

void controlarRiego() {
  // Apagar el riego si hay un fallo en el sensor
  if (!estadoSensor) {
    digitalWrite(PIN_RELE, LOW);
    return;
  }

  if (dentroHorarioRiego()) {
    if (humedad < HUM_MIN) {
      digitalWrite(PIN_RELE, HIGH); // Encender si la humedad es muy baja
    } else if (humedad >= HUM_MAX) {
      digitalWrite(PIN_RELE, LOW); // Apagar si la humedad es suficiente
    }
    // Si la humedad está en el rango [80, 85), el estado del relé no cambia.
  } else {
    digitalWrite(PIN_RELE, LOW); // Apagar fuera del horario permitido
  }
}

void registrarLog() {
  if (!estadoSD || !estadoRTC) return;
  
  DateTime now = rtc.now();
  char nombreArchivo[15];
  sprintf(nombreArchivo, "/%04d%02d%02d.csv", now.year(), now.month(), now.day());

  File archivo = SD.open(nombreArchivo, FILE_APPEND);
  if (archivo) {
    // Si el archivo está vacío, escribe una cabecera
    if (archivo.size() == 0) {
      archivo.println("Hora;Temperatura;Humedad;Riego");
    }
    char linea[50];
    sprintf(linea, "%02d:%02d:%02d;%.1f;%.1f;%s",
            now.hour(), now.minute(), now.second(),
            temperatura, humedad,
            digitalRead(PIN_RELE) ? "ON" : "OFF");
    archivo.println(linea);
    archivo.close();
    estadoSD = true;
  } else {
    Serial.println("Error al abrir archivo de log.");
    estadoSD = false;
  }
}