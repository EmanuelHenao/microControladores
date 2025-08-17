#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <DHT.h>

// --- Configuración de Pines ---
#define PANTALLA_ANCHO 128
#define PANTALLA_ALTO 64
#define PANTALLA_RESET -1  // -1 si comparte el pin de reset del Arduino
#define PIN_CS_SD 4        // Chip Select para la tarjeta SD
#define PIN_DHT 7          // Pin de datos para el sensor AM2301
#define PIN_RELE 8         // Pin para controlar el relé

// --- Umbrales de Humedad ---
const float UMBRAL_HUMEDAD_MIN = 80.0;
const float UMBRAL_HUMEDAD_MAX = 85.0;

// --- Horario de Riego Permitido (Formato 24 horas) ---
const int HORA_INICIO_RIEGO = 10;  // 10 AM
const int HORA_FIN_RIEGO = 23;     // 11 PM (El riego se permite hasta las 23:59)

// --- Intervalo del LOG ---
const unsigned long INTERVALO_LOG = 300000;  // 5 minutos en milisegundos

// --- Declaración de Objetos ---
RTC_DS3231 rtc;
DHT dht(PIN_DHT, AM2301);  // DHT21 es el tipo para el AM2301
Adafruit_SSD1306 pantalla(PANTALLA_ANCHO, PANTALLA_ALTO, &Wire, PANTALLA_RESET);

// --- Variables Globales ---
float humedad = 0.0;
float temperatura = 0.0;
unsigned long tiempoPrevioLog = 0;
char nombreArchivoLog[13];

// --- Banderas de Estado de los Módulos ---
bool estadoRTC = false;
bool estadoSD = false;
bool estadoSensor = false;
bool estadoPantalla = false;

// --- Funciones de Inicialización Modular ---

bool inicializarRTC() {
  if (!rtc.begin()) {
    Serial.println("Error: No se pudo encontrar el módulo RTC.");
    return false;
  }
  // Descomenta la siguiente línea la primera vez que ejecutes el código
  // para ajustar la fecha y hora del RTC a la de tu computador.
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  Serial.println("Módulo RTC inicializado correctamente.");
  return true;
}

bool inicializarSD() {
  if (!SD.begin(PIN_CS_SD)) {
    Serial.println("Error: No se pudo inicializar la tarjeta SD.");
    return false;
  }
  Serial.println("Tarjeta SD inicializada correctamente.");
  return true;
}

bool inicializarSensor() {
  dht.begin();
  // La librería DHT no tiene un método directo para verificar la conexión
  // Se validará durante la lectura de datos.
  Serial.println("Módulo Sensor DHT inicializado.");
  return true;
}

bool inicializarPantalla() {
  if (!pantalla.begin(0x3C, true)) {
    Serial.println("Error: No se pudo encontrar la pantalla OLED.");
    return false;
  }
  pantalla.clearDisplay();
  pantalla.setTextSize(1);
  pantalla.setTextColor(SSD1306_WHITE);
  pantalla.setCursor(0, 0);
  pantalla.println("Sistema de Riego");
  pantalla.println("Orellanas v1.0");
  pantalla.display();
  delay(1500);
  return true;
}

// --- Lógica Principal ---

void leerSensores() {
  if (estadoSensor) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    // La lectura del DHT puede fallar, se comprueba si los valores son válidos (no NaN)
    if (isnan(h) || isnan(t)) {
      Serial.println("Error al leer del sensor DHT.");
      estadoSensor = false;  // Se marca como desconectado si falla la lectura
    } else {
      humedad = h;
      temperatura = t;
    }
  } else {
    // Intenta reinicializar el sensor si está desconectado
    dht.begin();
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(h) && !isnan(t)) {
      estadoSensor = true;  // Se reconectó exitosamente
      humedad = h;
      temperatura = t;
    }
  }
}

/**
 * @brief Verifica si la hora actual está dentro del rango permitido para el riego.
 * @return true si está en horario de riego, false en caso contrario.
 */
bool esHorarioDeRiego() {
  if (!estadoRTC) {
    Serial.println("Advertencia: No se puede verificar el horario, RTC no disponible.");
    return false;  // Como medida de seguridad, no se riega si no se conoce la hora.
  }

  DateTime ahora = rtc.now();
  int horaActual = ahora.hour();

  if (horaActual >= HORA_INICIO_RIEGO && horaActual <= HORA_FIN_RIEGO) {
    return true;  // Está dentro del horario permitido
  } else {
    return false;  // Está fuera del horario
  }
}

void controlarRiego() {
  if (!estadoSensor) return;  // Requiere el sensor para funcionar

  // Primero, se verifica si el sistema está dentro del horario permitido
  if (esHorarioDeRiego()) {
    // Si está en horario, se procede a verificar la humedad
    if (humedad < UMBRAL_HUMEDAD_MIN) {
      digitalWrite(PIN_RELE, HIGH);  // Activa el relé (y la bomba)
    } else if (humedad >= UMBRAL_HUMEDAD_MAX) {
      digitalWrite(PIN_RELE, LOW);  // Desactiva el relé
    }
  } else {
    // Si no es horario de riego, nos aseguramos que el relé esté apagado.
    digitalWrite(PIN_RELE, LOW);
  }
}

void registrarDatos() {
  // Requiere RTC y SD para funcionar
  if (!estadoRTC || !estadoSD) return;

  unsigned long tiempoActual = millis();
  if (tiempoActual - tiempoPrevioLog >= INTERVALO_LOG) {
    tiempoPrevioLog = tiempoActual;
    DateTime now = rtc.now();

    // Crear nombre del archivo basado en la fecha actual (ej: 05082025.txt)
    sprintf(nombreArchivoLog, "%02d%02d%d.txt", now.day(), now.month(), now.year() % 100);

    File archivoLog = SD.open(nombreArchivoLog, FILE_WRITE);

    if (archivoLog) {
      char dataString[60];
      // Formato: HH:MM:SS, Temp: XX.X C, Hum: XX.X %, Riego: ON/OFF
      sprintf(dataString, "%02d:%02d:%02d, Temp: %.1f, Hum: %.1f, Riego: %s",
              now.hour(), now.minute(), now.second(),
              temperatura, humedad,
              digitalRead(PIN_RELE) == HIGH ? "ON" : "OFF");
      archivoLog.println(dataString);
      archivoLog.close();
      Serial.print("Dato guardado en log: ");
      Serial.println(dataString);
    } else {
      Serial.println("Error al abrir el archivo de log en la SD.");
      estadoSD = false;  // Asume que la SD falló si no puede abrir el archivo
    }
  }
}

void actualizarPantalla() {
  if (!estadoPantalla) {
    // Intenta reconectar la pantalla si está desconectada
    estadoPantalla = inicializarPantalla();
    return;
  }

  pantalla.clearDisplay();
  pantalla.setTextSize(1);
  pantalla.setTextColor(SSD1306_WHITE);
  pantalla.setCursor(0, 0);

  // --- Información de Fecha y Hora ---
  if (estadoRTC) {
    DateTime now = rtc.now();
    char buffer[20];
    sprintf(buffer, "%02d/%02d/%d  %02d:%02d:%02d", now.day(), now.month(), now.year(), now.hour(), now.minute(), now.second());
    pantalla.println(buffer);
  } else {
    pantalla.println("RTC Desconectado");
  }

  pantalla.println("");  // Espacio

  // --- Información de Sensores ---
  if (estadoSensor) {
    char bufferTemp[16];
    sprintf(bufferTemp, "Temp: %.1f C", temperatura);
    pantalla.println(bufferTemp);

    char bufferHum[16];
    sprintf(bufferHum, "Hum:  %.1f %%", humedad);
    pantalla.println(bufferHum);
  } else {
    pantalla.println("Sensor Desconectado");
  }

  // --- Estado del Sistema de Riego ---
  pantalla.print("Riego: ");
  pantalla.println(digitalRead(PIN_RELE) == HIGH ? "ACTIVO" : "INACTIVO");


  // --- Iconos de Estado de Módulos ---
  pantalla.setCursor(0, 56);  // Posición en la última fila
  pantalla.print(estadoRTC ? "C:OK" : "C:ER");
  pantalla.print(estadoSD ? " S:OK" : " S:ER");
  pantalla.print(estadoSensor ? " H:OK" : " H:ER");

  pantalla.display();
}

void setup() {
  Serial.begin(9600);
  pinMode(PIN_RELE, OUTPUT);
  digitalWrite(PIN_RELE, LOW);  // Asegurarse que el relé inicie apagado

  // Inicializar todos los módulos
  estadoPantalla = inicializarPantalla();
  estadoRTC = inicializarRTC();
  estadoSD = inicializarSD();
  estadoSensor = inicializarSensor();
  Serial.println(humedad);

}

void loop() {
  leerSensores();
  Serial.println(humedad);
  //controlarRiego();
  registrarDatos();
  //actualizarPantalla();
  Serial.println(humedad);
  delay(2000);  // Pausa de 2 segundos entre ciclos para estabilizar lecturas
}