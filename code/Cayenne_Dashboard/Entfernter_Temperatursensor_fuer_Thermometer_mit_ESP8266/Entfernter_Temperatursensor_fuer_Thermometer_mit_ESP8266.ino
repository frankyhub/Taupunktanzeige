/*
 WLAN Temperatursensor
 ESP Now mit dem Thermometer
 Wenn noch keine MAC Adresse ermittelt wurde
 sucht der Sensor ein WLAN mit SSID Thermometer
 Wenn er den AP gefunden hat, merkt er sich die MAC Adresse 
 solange die Stromversorgung nicht unterbrochen wurde
 Das ESP Now Protokoll ist sehr schnell sodass nur sehr kurze Zeit (us)
 höherer Strom fließt. Der Sensor sendet Temperaturdaten und geht dann für 5 Minuten in den 
 Tiefschlaf sodass nur ganz wenig Energie verbraucht wird und der
 Sensor daher mit Batterien betrieben werden kann. 
*/

//Bibliothek für WiFi
#include <ESP8266WiFi.h>
//Bibliotheken für Temperatursensor DS18B20
#include <OneWire.h>

//Bibliothek für ESP Now
#include <DallasTemperature.h>
extern "C" {
  #include <espnow.h>
}

//Debug Flag wenn false werden alle Debug Meldungen unterdrückt
//um zusätzlich Strom zu sparen
#define DEBUG true

//Konstanten für WiFi
#define WIFI_CHANNEL 1
#define SEND_TIMEOUT 2450  // 245 Millisekunden timeout 

//Pins für Temperatursensor
const byte bus = 2;//pin GPIO2

//Datenstruktur für den Datenaustausch
struct DATEN_STRUKTUR {
    float temp = 0;
};

//Datenstruktur für das Rtc Memory mit Prüfsumme um die Gültigkeit
//zu überprüfen damit wird die MAC Adresse gespeichert
struct MEMORYDATA {
  uint32_t crc32;
  uint8_t mac[6];
};

//Globale daten
volatile bool callbackCalled;

MEMORYDATA statinfo;

OneWire oneWire(bus);

DallasTemperature sensoren(&oneWire);

//Array um Sensoradressen zu speichern
DeviceAddress adressen;

//Unterprogramm zum Berechnen der Prüfsumme
uint32_t calculateCRC32(const uint8_t *data, size_t length)
{
  uint32_t crc = 0xffffffff;
  while (length--) {
    uint8_t c = *data++;
    for (uint32_t i = 0x80; i > 0; i >>= 1) {
      bool bit = crc & 0x80000000;
      if (c & i) {
        bit = !bit;
      }
      crc <<= 1;
      if (bit) {
        crc ^= 0x04c11db7;
      }
    }
  }
  return crc;
}

//Schreibt die Datenstruktur statinfo mit korrekter Prüfsumme in das RTC Memory
void UpdateRtcMemory() {
    uint32_t crcOfData = calculateCRC32(((uint8_t*) &statinfo) + 4, sizeof(statinfo) - 4);
    statinfo.crc32 = crcOfData;
    ESP.rtcUserMemoryWrite(0,(uint32_t*) &statinfo, sizeof(statinfo));
}

//sucht nach einem geeigneten AccessPoint
void ScanForSlave() {
  bool slaveFound = 0;
  
  int8_t scanResults = WiFi.scanNetworks();
  // reset on each scan

  if (DEBUG) Serial.println("Scan done");
  if (scanResults == 0) {
    if (DEBUG) Serial.println("No WiFi devices in AP Mode found");
  } else {
    if (DEBUG) Serial.print("Found "); 
    if (DEBUG) Serial.print(scanResults); 
    if (DEBUG) Serial.println(" devices ");
    for (int i = 0; i < scanResults; ++i) {
      // Print SSID and RSSI for each device found
      String SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);

      if (DEBUG) {
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(SSID);
        Serial.print(" (");
        Serial.print(RSSI);
        Serial.print(")");
        Serial.println("");
      }
      delay(10);
      // Check if the current device starts with `Thermometer`
      if (SSID.indexOf("Thermometer") == 0) {
        // SSID of interest
        if (DEBUG) {
          Serial.println("Found a Slave.");
          Serial.print(i + 1); Serial.print(": "); Serial.print(SSID); Serial.print(" ["); Serial.print(BSSIDstr); Serial.print("]"); Serial.print(" ("); Serial.print(RSSI); Serial.print(")"); Serial.println("");
        }
        int mac[6];
        // wir ermitteln die MAC Adresse und speichern sie im RTC Memory
        if ( 6 == sscanf(BSSIDstr.c_str(), "%x:%x:%x:%x:%x:%x%c",  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5] ) ) {
          for (int ii = 0; ii < 6; ++ii ) {
            statinfo.mac[ii] = (uint8_t) mac[ii];
          }
          UpdateRtcMemory();
        }

        slaveFound = 1;
        //Nachdem der AP gefunden wurde können wir abbrechen
        break;
      }
    }
  }
  
  
  if (DEBUG) {
    if (slaveFound) {
      Serial.println("Slave Found, processing..");
    } else {
      Serial.println("Slave Not Found, trying again.");
    }
  }

  // RAM freigeben
  WiFi.scanDelete();
}
// function um eine Sensoradresse zu drucken
void printAddress(DeviceAddress adressen)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (adressen[i] < 16) Serial.print("0");
    Serial.print(adressen[i], HEX);
  }
}

void setup() {
  if (DEBUG) {
    Serial.begin(115200); 
    Serial.println("Start");
  }
  pinMode(bus,INPUT_PULLUP);
  //Wir ermitteln die Anzah der Sensoren am Eindraht-Bus
  sensoren.begin();
  if (DEBUG) {
    Serial.print(sensoren.getDeviceCount(), DEC);
    Serial.println(" Sensoren gefunden.");
  }
  //Nun prüfen wir ob einer der Sensoren am Bus ein Temperatur Sensor ist
  if (!sensoren.getAddress(adressen,0)) {
    if (DEBUG) Serial.println("Kein Temperatursensor vorhanden!");
  }
  //adressen anzeigen
  if (DEBUG) {
    Serial.print("Adresse: ");
    printAddress(adressen);
    Serial.println();
  }
  //Nun setzen wir noch die gewünschte Auflösung (9, 10, 11 oder 12 bit)
  sensoren.setResolution(adressen,10);
  //Zur Kontrolle lesen wir den Wert wieder aus
  if (DEBUG) {
    Serial.print("Auflösung = ");
    Serial.print(sensoren.getResolution(adressen), DEC);
    Serial.println();
  }
  sensoren.requestTemperatures(); // Commando um die Temperaturen auszulesen
  //Wir lesen aus dem RTC Memory
  ESP.rtcUserMemoryRead(0, (uint32_t*) &statinfo, sizeof(statinfo));
  if (DEBUG) Serial.println("RTC Done");
  uint32_t crcOfData = calculateCRC32(((uint8_t*) &statinfo) + 4, sizeof(statinfo) - 4);
  WiFi.mode(WIFI_STA); // Station mode for esp-now sensor node
  if (DEBUG) Serial.println("WifiMode");
  if (statinfo.crc32 != crcOfData) { //wir haben keine gültige MAC Adresse
    if (DEBUG) Serial.println("Scan vor Slave");
    ScanForSlave();
    if (DEBUG) {
      Serial.printf("This mac: %s, ", WiFi.macAddress().c_str()); 
      Serial.printf("target mac: %02x%02x%02x%02x%02x%02x", statinfo.mac[0], statinfo.mac[1], statinfo.mac[2], statinfo.mac[3], statinfo.mac[4], statinfo.mac[5]); 
      Serial.printf(", channel: %i\n", WIFI_CHANNEL); 
    }
  }
  if (esp_now_init() != 0) {
    if (DEBUG) Serial.println("*** ESP_Now init failed");
    ESP.restart();
  }
  //ESP Now Controller
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  //Peer Daten initialisieren
  esp_now_add_peer(statinfo.mac, ESP_NOW_ROLE_SLAVE, WIFI_CHANNEL, NULL, 0);
  //wir registrieren die Funktion, die nach dem Senden aufgerufen werden soll
  esp_now_register_send_cb([](uint8_t* mac, uint8_t sendStatus) {
    if (DEBUG) {
      Serial.print("send_cb, status = "); Serial.print(sendStatus); 
      Serial.print(", to mac: "); 
      char macString[50] = {0};
      sprintf(macString,"%02X:%02X:%02X:%02X:%02X:%02X", statinfo.mac[0], statinfo.mac[1], statinfo.mac[2], statinfo.mac[3], statinfo.mac[4], statinfo.mac[5]);
      Serial.println(macString);
    }
    callbackCalled = true;
  });
  
  //Flag auf false setzen
  callbackCalled = false;
  //Temperaturmessung starten
  sensoren.requestTemperatures();
  
  delay(750); //750 ms warten bis die Messung fertig ist
  //Temperaturwert holen und in Satenstruktur zum Senden speichern
  DATEN_STRUKTUR data;
  data.temp = sensoren.getTempC(adressen);
  uint8_t bs[sizeof(data)];
  //Datenstruktur in den Sendebuffer kopieren
  memcpy(bs, &data, sizeof(data));
  //Daten an Thermometer senden
  esp_now_send(NULL, bs, sizeof(data)); // NULL means send to all peers
  if (DEBUG) {
    Serial.print("Temperatur: "); Serial.print(data.temp); Serial.println("°C");
  }
}

void loop() {
  //warten bis Daten gesendet wurden
  if (callbackCalled || (millis() > SEND_TIMEOUT)) {
    if (DEBUG) Serial.println("Sleep");
    delay(100);
    //Für 300 Sekunden in den Tiefschlafmodus
    //dann erfolgt ein Reset und der ESP8266 wird neu gestartet
    //Daten im RTC Memory werden beim Reset nicht gelöscht.
    ESP.deepSleep(300E6);
  }
}
