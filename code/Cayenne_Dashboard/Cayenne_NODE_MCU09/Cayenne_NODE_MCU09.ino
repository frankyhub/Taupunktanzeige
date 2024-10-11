/*ESP8266 mit DHT11 Sensor und Relais gesteuert über MQTT Dashboard Cayenne
 * https://cayenne.mydevices.com/ 
 * ESP32 NodeMCU Module WLAN WiFi Dev Kit C Development Board mit CP2102 (Nachfolgermodell zum ESP8266)
 * Board: NODE MCU 0.9
 * Uploadspeed: 115200
 * CPU Freq: 80MHz
 * Flash Size 4M
 * 
 * KHF 23.10.2020
 */

 
//#define CAYENNE_DEBUG
#define CAYENNE_PRINT Serial
#include <CayenneMQTTESP8266.h>
#include <DHTesp.h>
#include <MQ135.h>

// WiFi network info.
#define SSID "R2-D2"
#define WIFIPASSWORD "xxx"

// Cayenne Zugangsdaten. Diese erhalten wir vom Cayenne Dashboard.
#define USERNAME "268e68e0-0d3f-11eb-883c-638d8ce4c23d"
#define PASSWORD "f359cce71821bb9b7824527bee3d484f4f824e73"
#define CLIENTID "8ea994e0-15be-11eb-8779-7d56e82df461"


//Kanäle zur Kommunikation mit dem Dashboard
#define KANAL_TEMPERATUR 4 //1
#define KANAL_FEUCHTE 16 //2
#define KANAL_RELAIS  17 //3
#define KANAL_STATUS 5// 4

#define PIN_DHT 19 // 5
#define PIN_RELAIS 5 //4

//globale Variablen
boolean relais_status = 0;
TempAndHumidity neueWerte;
DHTesp dht;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_RELAIS,OUTPUT);
  digitalWrite(PIN_RELAIS,relais_status);
  dht.setup(PIN_DHT, DHTesp::DHT11);
  Cayenne.begin(USERNAME, PASSWORD, CLIENTID, SSID, WIFIPASSWORD);
}

void loop() {
  Cayenne.loop();
}

// Diese Funktion wird von Cayenne.loop in regelmäßigen Abständen aufgerufen
// Hier sollten die Daten der Sensoren übertragen werden
CAYENNE_OUT_DEFAULT()
{
  neueWerte = dht.getTempAndHumidity();
  if (dht.getStatus() == 0) {
    Cayenne.celsiusWrite(KANAL_TEMPERATUR,neueWerte.temperature);
    Cayenne.virtualWrite(KANAL_FEUCHTE,neueWerte.humidity,TYPE_RELATIVE_HUMIDITY,UNIT_PERCENT);
  }
  Cayenne.digitalSensorWrite(KANAL_STATUS,relais_status);
}

//Diese Funktion wird von Cayenne.loop aufgerufen wenn Daten für Aktoren von Cayenne anstehen
//Hier sollte auf diese Daten reagiert werden
CAYENNE_IN_DEFAULT()
{
  if (request.channel == KANAL_RELAIS) {
    relais_status = getValue.asInt();
    digitalWrite(PIN_RELAIS,relais_status);
  }
}
