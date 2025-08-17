#include <Wire.h>
#include <SD.h>
#include <RTClib.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <avr/wdt.h>  // Librería para manejar el Watchdog

// -------------------- CONFIGURACIÓN HARDWARE --------------------
// I2C Pantalla y RTC
#define SDA_PIN A4
#define SCL_PIN A5


#define PIN_DHT 2
#define TIPO_DHT AM2301
#define PIN_RELE 3
#define PIN_SD_CS 4

//pantalla
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET -1     // Reset pin # (or -1 if sharing Arduino reset pin)

// -------------------- OBJETOS DE LIBRERÍAS ----------------------
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

DHT dht(PIN_DHT, TIPO_DHT);

RTC_DS3231 rtc;


// -------------------- VARIABLES DE ESTADO -----------------------
bool estadoRTC = false;
bool estadoSD = false;
bool estadoSensor = false;
bool estadoPantalla = false;

// -------------------- VARIABLES DE CONTROL ----------------------
float temperatura = 0;
float humedad = 0;
unsigned long ultimaLectura = 0;
unsigned long ultimoLog = 0;

// Rango de humedad para riego
const float HUM_MIN = 80.0;
const float HUM_MAX = 85.0;

// Horario permitido de riego
const int HORA_INICIO_RIEGO = 10;  // 10:00
const int HORA_FIN_RIEGO = 23;     // 23:59

// -------------------- ICONOS --------------------
static const unsigned char PROGMEM iconoOK[] = { B00011000,
                                                 B00011000,
                                                 B00110100,
                                                 B00110100,
                                                 B01100110,
                                                 B01100110,
                                                 B11000011,
                                                 B11000011 };

static const unsigned char PROGMEM iconoError[] = { B11000011,
                                                    B01100110,
                                                    B00111100,
                                                    B00011000,
                                                    B00011000,
                                                    B00111100,
                                                    B01100110,
                                                    B11000011 };


extern int __heap_start, *__brkval; 
int freeMemory() {
  int v; 
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval); 
}

// -------------------- SETUP --------------------
void setup() {

  Serial.begin(9600);

  desactivarPinesNoUsados();

  pinMode(PIN_RELE, OUTPUT);
  digitalWrite(PIN_RELE, LOW);


Serial.print("Memoria libre: ");
  Serial.println(freeMemory());
  estadoSD = inicializarSD();
  digitalWrite(PIN_SD_CS, HIGH);///desactivar
  
  Wire.begin();

  estadoPantalla = inicializarPantalla();

  estadoRTC = inicializarRTC();

  
  digitalWrite(PIN_DHT, LOW);
  estadoSensor = inicializarSensor();


  wdt_enable(WDTO_8S);
}

// -------------------- LOOP --------------------
void loop() {
  unsigned long ahora = millis();

  // Leer sensor cada 2 segundos
  if (ahora - ultimaLectura >= 2000) {
    ultimaLectura = ahora;
    leerSensor();
    controlarRiego();
    actualizarPantalla();
  }

  // Registrar en log cada 5 minutos
  if (ahora - ultimoLog >= 300000) {
    ultimoLog = ahora;
    registrarLog();
  }

  // Resetear el temporizador del watchdog
  wdt_reset();
}



// -------------------- FUNCIONES --------------------

void desactivarPinesNoUsados() {
  // Pines digitales libres: 5, 6, 7, 8, 9, 10
  int pinesLibres[] = { 5, 6, 7, 8, 9, 10 };

  for (byte i = 0; i < sizeof(pinesLibres) / sizeof(pinesLibres[0]); i++) {
    Serial.println("no usado: ");
    Serial.println(pinesLibres[i]);
    pinMode(pinesLibres[i], OUTPUT);
    digitalWrite(pinesLibres[i], HIGH);
  }
}

void liberarI2C() {
   Serial.println(" I2C ..");
  pinMode(SDA_PIN, INPUT_PULLUP);
  pinMode(SCL_PIN, INPUT_PULLUP);

  if (digitalRead(SDA_PIN) == LOW) {
    Serial.println("I2C bloqueado, intentando liberar...");
    pinMode(SCL_PIN, OUTPUT);

    for (byte i = 0; i < 9; i++) {  
      digitalWrite(SCL_PIN, HIGH);
      delayMicroseconds(5);
      digitalWrite(SCL_PIN, LOW);
      delayMicroseconds(5);
    }

    // STOP manual
    pinMode(SDA_PIN, OUTPUT);
    digitalWrite(SDA_PIN, LOW);
    delayMicroseconds(5);
    digitalWrite(SCL_PIN, HIGH);
    delayMicroseconds(5);
    digitalWrite(SDA_PIN, HIGH);
    delayMicroseconds(5);

    Serial.println("Bus I2C liberado.");
  }
}



bool inicializarRTC() {
  bool rtcInicializado = rtc.begin();
  delay(5000);
  Serial.print("RTC-Fecha: ");
  Serial.println(rtcInicializado);

  if (!rtcInicializado) {
    Serial.println("Error: RTC no detectado");
    return false;
  }
  if (rtc.lostPower()) {
    Serial.println("RTC sin hora, configurando...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  Serial.println("OK: RTC ");
  return true;
}

bool inicializarSD() {
  digitalWrite(PIN_SD_CS, LOW);
  bool sdInicializado = SD.begin(PIN_SD_CS);
  delay(5000);
  Serial.print("SD reader: ");
  Serial.println(sdInicializado);

  if (!sdInicializado) {
    Serial.println("Error: SD no detectada");
    return false;
  }
  Serial.println("OK: SD ");
  return true;
}

bool inicializarSensor() {

  dht.begin();
  delay(5000);
  Serial.print("dht :");
  float test = dht.readTemperature();
  Serial.println(test);
  if (isnan(test)) {
    Serial.println("Error: Sensor DHT no responde");
    return false;
  }
  Serial.println("OK: dht ");
  return true;
}

bool inicializarPantalla() {
  liberarI2C();
  Serial.println("OLED.... ");
  bool pantallaInicializada = display.begin(0x3C, true);
  Serial.print("OLED: ");
  Serial.println(pantallaInicializada);
  //Wire.setWireTimeout(500, true);
  if (!pantallaInicializada) {
    Serial.println("Error: OLED SH1106 no detectada");
    return false;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println(F("Sistema Riego"));
  display.println(F("Orellanas v2.0"));
  display.display();
  delay(1500);
  Serial.println("OK: oled ");
  return true;
}

void mostrarIconosEstado() {
  int yPos = 48;
  display.drawBitmap(0, yPos, estadoRTC ? iconoOK : iconoError, 8, 8, SH110X_WHITE);
  display.drawBitmap(12, yPos, estadoSD ? iconoOK : iconoError, 8, 8, SH110X_WHITE);
  display.drawBitmap(24, yPos, estadoSensor ? iconoOK : iconoError, 8, 8, SH110X_WHITE);
  display.drawBitmap(36, yPos, estadoPantalla ? iconoOK : iconoError, 8, 8, SH110X_WHITE);
}

void actualizarPantalla() {
  if (!estadoPantalla) {
    estadoPantalla = inicializarPantalla();
    return;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  char buffer[25];

  if (estadoRTC) {
    DateTime now = rtc.now();
    sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d",
            now.day(), now.month(), now.year(),
            now.hour(), now.minute(), now.second());
    display.println(buffer);
  } else {
    display.println("RTC: X");
  }

  if (estadoSensor) {
    sprintf(buffer, "Temp: %.1f C", temperatura);
    display.println(buffer);
    sprintf(buffer, "Hum:  %.1f %%", humedad);
    display.println(buffer);
  } else {
    display.println("Sensor: X");
  }

  sprintf(buffer, "Riego: %s", digitalRead(PIN_RELE) ? "ON" : "OFF");
  display.println(buffer);

  mostrarIconosEstado();
  display.display();
}

void leerSensor() {
  if (!estadoSensor) {
    estadoSensor = inicializarSensor();
    return;
  }
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h) && !isnan(t)) {
    humedad = h;
    temperatura = t;
  } else {
    estadoSensor = false;
  }
}

bool dentroHorarioRiego(DateTime ahora) {
  return (ahora.hour() >= HORA_INICIO_RIEGO && ahora.hour() <= HORA_FIN_RIEGO);
}

void controlarRiego() {
  if (!estadoSensor || !estadoRTC) return;

  DateTime ahora = rtc.now();
  if (dentroHorarioRiego(ahora)) {
    if (humedad < HUM_MIN) {
      digitalWrite(PIN_RELE, HIGH);
    } else if (humedad >= HUM_MAX) {
      digitalWrite(PIN_RELE, LOW);
    }
  } else {
    digitalWrite(PIN_RELE, LOW);
  }
}

void registrarLog() {
  Serial.println("log");

  if (!estadoSD || !estadoRTC || !estadoSensor) return;

  DateTime now = rtc.now();
  char nombreArchivo[13];
  sprintf(nombreArchivo, "%02d%02d%04d.txt", now.day(), now.month(), now.year());

  File archivo = SD.open(nombreArchivo, FILE_WRITE);
  if (archivo) {
    char linea[60];
    sprintf(linea, "%02d/%02d/%04d;%02d:%02d:%02d;%.1f;%.1f;%d;%d;%d;%d",
            now.day(), now.month(), now.year(),
            now.hour(), now.minute(), now.second(),
            temperatura, humedad,
            estadoRTC, estadoSD, estadoSensor, estadoPantalla);
    archivo.println(linea);
    archivo.close();
  } else {
    estadoSD = false;
  }
}


