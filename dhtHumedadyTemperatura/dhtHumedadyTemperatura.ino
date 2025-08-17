#include <DHT.h>

#define PIN_DHT 2    

DHT dht(PIN_DHT, AM2301);

//AM2301

float humedad = 0.0;
float temperatura = 0.0;
void setup() {

  Serial.begin(9600);
  Serial.print("Dht testt!");

  dht.begin();
}

void loop() {
  delay(2000);

  humedad = dht.readHumidity();
  temperatura = dht.readTemperature();

  Serial.print("Dht humedad: ");
  Serial.print(humedad);
  Serial.print("   temperatura: ");
  Serial.print(temperatura);
    Serial.println();


}
