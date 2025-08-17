// -------------------- LIBRERÍAS OPTIMIZADAS --------------------
#include <Arduino.h>
#include <U8g2lib.h> // OPTIMIZACIÓN: Librería de pantalla eficiente en memoria.
#include <Wire.h>
#include <SD.h>
#include <RTClib.h>
#include <DHT.h>
#include <avr/wdt.h>

// -------------------- CONFIGURACIÓN HARDWARE --------------------
#define PIN_DHT 2
#define TIPO_DHT AM2301
#define PIN_RELE 3
#define PIN_SD_CS 4

// -------------------- OBJETOS DE LIBRERÍAS ----------------------
// OPTIMIZACIÓN: Usamos U8g2 en modo paginado. U8G2_R0 indica sin rotación.
// Esto usa un búfer pequeño en lugar de los 1024 bytes de Adafruit_GFX.
//U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE); //usa el full buffer
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE); // usa un buffer de 128 bits

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
const int HORA_INICIO_RIEGO = 10;
const int HORA_FIN_RIEGO = 23;

// -------------------- ICONOS --------------------
// Los iconos ya usan PROGMEM, lo cual es excelente. No consumen SRAM.
static const unsigned char PROGMEM iconoOK[] = { 0x18, 0x18, 0x34, 0x34, 0x66, 0x66, 0xC3, 0xC3 };
static const unsigned char PROGMEM iconoError[] = { 0xC3, 0x66, 0x3C, 0x18, 0x18, 0x3C, 0x66, 0xC3 };

// Función para chequear memoria libre (la tuya es perfecta)
extern int __heap_start, *__brkval;
int freeMemory() {
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

// -------------------- SETUP --------------------
void setup() {
  Serial.begin(9600);
  
  // Desactivar pines no usados es una buena práctica de ahorro de energía, pero no afecta la SRAM.
  // desactivarPinesNoUsados(); 

  pinMode(PIN_RELE, OUTPUT);
  digitalWrite(PIN_RELE, LOW);

  // OPTIMIZACIÓN: Usamos F() para todas las cadenas de texto literales.
  Serial.print(F("Memoria libre al inicio: "));
  Serial.println(freeMemory());

  // Inicialización de componentes
  Wire.begin();
  estadoPantalla = inicializarPantalla();
  estadoSD = inicializarSD();
  estadoRTC = inicializarRTC();
    Serial.print(F("Memoria libre al rtc: "));
  Serial.println(freeMemory());
  estadoSensor = inicializarSensor();
      Serial.print(F("Memoria libre al zs: "));
  Serial.println(freeMemory());

  wdt_enable(WDTO_8S);
}

// -------------------- LOOP --------------------
void loop() {
  wdt_reset(); // Resetea el watchdog al inicio del loop, es más seguro.
  unsigned long ahora = millis();
      Serial.print(F("Memoria libre al zlops: "));
  Serial.println(freeMemory());
  if (ahora - ultimaLectura >= 2000) {
    ultimaLectura = ahora;
    leerSensor();
    controlarRiego();
    actualizarPantalla();
  }

  if (ahora - ultimoLog >= 300000) {
    ultimoLog = ahora;
    registrarLog();
  }
}

// -------------------- FUNCIONES DE INICIALIZACIÓN --------------------

bool inicializarPantalla() {
  estadoPantalla = u8g2.begin();
  if (!estadoPantalla) {
    Serial.println(F("Error: OLED SH1106 no detectada"));
    return false;
  }
  
  // OPTIMIZACIÓN: Dibujamos en la pantalla usando el bucle de página de U8g2.
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_ncenB08_tr); // Seleccionamos una fuente
    u8g2.setCursor(0, 12);
    u8g2.print(F("Sistema Riego"));
    u8g2.setCursor(0, 28);
    u8g2.print(F("Orellanas v2.0"));
  } while (u8g2.nextPage());
  
  delay(1500);
  Serial.println(F("OK: OLED inicializada"));
  return true;
}

bool inicializarRTC() {
  if (!rtc.begin()) {
    Serial.println(F("Error: RTC no detectado"));
    return false;
  }
  if (rtc.lostPower()) {
    Serial.println(F("RTC sin hora, configurando..."));
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  Serial.println(F("OK: RTC inicializado"));
  return true;
}

bool inicializarSD() {
  if (!SD.begin(PIN_SD_CS)) {
    Serial.println(F("Error: SD no detectada"));
    return false;
  }
  Serial.println(F("OK: SD inicializada"));
  return true;
}

bool inicializarSensor() {
  dht.begin();
  delay(2000); // El sensor DHT necesita un tiempo para estabilizarse.
  float test = dht.readTemperature();
  if (isnan(test)) {
    Serial.println(F("Error: Sensor DHT no responde"));
    return false;
  }
  Serial.println(F("OK: DHT inicializado"));
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

  // OPTIMIZACIÓN: Bucle de página de U8g2
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_ncenB08_tr);

    if (estadoRTC) {
      DateTime now = rtc.now();
      char bufferFecha[11];
      char bufferHora[9];
      sprintf(bufferFecha, "%02d/%02d/%04d", now.day(), now.month(), now.year());
      sprintf(bufferHora, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
      u8g2.setCursor(0, 10);
      u8g2.print(bufferFecha);
      u8g2.setCursor(70, 10);
      u8g2.print(bufferHora);
    } else {
      u8g2.setCursor(0, 10);
      u8g2.print(F("RTC: Error"));
    }

    if (estadoSensor) {
      // OPTIMIZACIÓN: Evitamos sprintf para ahorrar memoria
      u8g2.setCursor(0, 24);
      u8g2.print(F("T: "));
      u8g2.print(temperatura, 1); // El ", 1" indica un decimal
      u8g2.print(F(" C"));
      
      u8g2.setCursor(64, 24);
      u8g2.print(F("H: "));
      u8g2.print(humedad, 1);
      u8g2.print(F(" %"));
    } else {
      u8g2.setCursor(0, 24);
      u8g2.print(F("Sensor: Error"));
    }
    
    u8g2.setCursor(0, 38);
    u8g2.print(F("Riego: "));
    u8g2.print(digitalRead(PIN_RELE) ? F("ON") : F("OFF"));

    mostrarIconosEstado(54); // Coordenada Y para los íconos
    
  } while (u8g2.nextPage());
}


void leerSensor() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    estadoSensor = false;
    Serial.println(F("Fallo al leer del sensor DHT!"));
  } else {
    humedad = h;
    temperatura = t;
    estadoSensor = true;
  }
}

bool dentroHorarioRiego() {
  if (!estadoRTC) return false; // No regar si no sabemos la hora
  DateTime ahora = rtc.now();
  return (ahora.hour() >= HORA_INICIO_RIEGO && ahora.hour() <= HORA_FIN_RIEGO);
}

void controlarRiego() {
  if (!estadoSensor) {
    digitalWrite(PIN_RELE, LOW); // Apagar si el sensor falla
    return;
  }

  if (dentroHorarioRiego()) {
    if (humedad < HUM_MIN) {
      digitalWrite(PIN_RELE, HIGH);
    } else if (humedad >= HUM_MAX) {
      digitalWrite(PIN_RELE, LOW);
    }
  } else {
    digitalWrite(PIN_RELE, LOW); // Apagar si está fuera de horario
  }
}

void registrarLog() {
  if (!estadoSD || !estadoRTC || !estadoSensor) return;
  
  DateTime now = rtc.now();
  char nombreArchivo[13];
  // Usar snprintf es un poco más seguro que sprintf
  snprintf(nombreArchivo, sizeof(nombreArchivo), "%02d%02d%04d.TXT", now.day(), now.month(), now.year());

  // OPTIMIZACIÓN: Usar un búfer más pequeño y FILE_APPEND en lugar de FILE_WRITE.
  // FILE_APPEND es más eficiente ya que abre el archivo y va directamente al final.
  File archivo = SD.open(nombreArchivo, FILE_WRITE);
  if (archivo) {
    char linea[45]; // Búfer ajustado al tamaño necesario
    snprintf(linea, sizeof(linea), "%02d:%02d:%02d;%.1f;%.1f;%s",
            now.hour(), now.minute(), now.second(),
            temperatura, humedad,
            digitalRead(PIN_RELE) ? "ON" : "OFF");
    archivo.println(linea);
    archivo.close();
  } else {
    Serial.println(F("Error al abrir archivo de log."));
    estadoSD = false;
  }
}