/*ESP8266 mit DHT11 Sensor und Relais gesteuert über MQTT Dashboard Cayenne
 * https://mydevices.com
 * https://www.az-delivery.de/blogs/azdelivery-blog-fur-arduino-und-raspberry-pi/arbeiten-mit-dem-cayenne-dashboard-teil-1
 * Arbeiten mit dem Cayenne Dashboard - einfacher Sensor (Teil 1)
 * Cayenne Login:
 * https://accounts.mydevices.com/auth/realms/cayenne/protocol/openid-connect/auth?response_type=code&scope=email+profile&client_id=cayenne-web-app&state=5taigvIe4wpHj7u3j1Qi1FOT3078XaHSOKW73aQO&redirect_uri=https%3A%2F%2Fcayenne.mydevices.com%2Fauth%2Fcallback
 * 
 * Board-Verwalter: http://arduino.esp8266.com/stable/package_esp8266com_ind
ex.json
 */
#define CAYENNE_DEBUG
#define CAYENNE_PRINT Serial
//#include <  >
#include <DHTesp.h>
#include <MQ135.h>


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
#define KANAL_GAS 6

#define PIN_DHT 5
#define PIN_RELAIS 4

//globale Variablen
boolean relais_status = 0;
TempAndHumidity neueWerte;
DHTesp dht;

float ppm;
MQ135 gassensor = analogRead(KANAL_GAS);


void setup() {
  Serial.begin(115200);
  pinMode(PIN_RELAIS,OUTPUT);
  pinMode(KANAL_GAS, INPUT);

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
  ppm = gassensor.getPPM();
  neueWerte = dht.getTempAndHumidity();
  if (dht.getStatus() == 0) {
    Cayenne.celsiusWrite(KANAL_TEMPERATUR,neueWerte.temperature);
    Cayenne.virtualWrite(KANAL_FEUCHTE,neueWerte.humidity,TYPE_RELATIVE_HUMIDITY,UNIT_PERCENT);
    Cayenne.virtualWrite(KANAL_GAS,ppm.humidity,TYPE_RELATIVE_HUMIDITY,UNIT_PERCENT);
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
