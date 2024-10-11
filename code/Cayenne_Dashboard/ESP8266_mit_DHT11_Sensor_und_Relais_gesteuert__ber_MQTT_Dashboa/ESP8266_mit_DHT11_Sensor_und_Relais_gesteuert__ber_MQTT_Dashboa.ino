/*ESP8266 mit DHT11 Sensor und Relais gesteuert über MQTT Dashboard Cayenne
 * https://mydevices.com
 */
//#define CAYENNE_DEBUG
#define CAYENNE_PRINT Serial
#include <CayenneMQTTESP8266.h>
#include <DHTesp.h>

// WiFi network info.
#define SSID "R2-D2"
#define WIFIPASSWORD "xxx"

// Cayenne Zugangsdaten. Diese erhalten wir vom Cayenne Dashboard.
#define USERNAME "268e68e0-0d3f-11eb-883c-638d8ce4c23d"
#define PASSWORD "f359cce71821bb9b7824527bee3d484f4f824e73"
#define CLIENTID "845b1eb0-153b-11eb-a2e4-b32ea624e442"

//Kanäle zur Kommunikation mit dem Dashboard
#define KANAL_TEMPERATUR 1
#define KANAL_FEUCHTE 2
#define KANAL_RELAIS 3
#define KANAL_STATUS 4

#define PIN_DHT 5
#define PIN_RELAIS 4

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
