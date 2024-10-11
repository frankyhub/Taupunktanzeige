/* Arbeiten mit dem Cayenne Dashboard - LoRa Device (Teil 4)
 */



#include <arduino.h>
#include <DHTesp.h>
#include <SPI.h>
#include <CayenneLPP.h>
#include <LoRa.h>
#include "SSD1306.h"

//LoRa Einstellungen
#define SS      18
#define RST     14
#define DI0     26
#define BAND    433175000

// Zeitintervall (Sekunden)
#define TX_INTERVAL 10

//Pin für DHT11
#define DHT_PIN 22

//POin für Relais
#define RELAIS  23

//Kanalzuordnung
#define CHANNEL_TEMP 1
#define CHANNEL_HUM 2
#define CHANNEL_RELAIS 3
#define CHANNEL_STATUS 4


//Globale Variablen
uint8_t devid[6]; //ID des Geräts = MAC Adresse
uint8_t relaisOn = 0; //Status des Relais
TempAndHumidity newValues; //Temperatur und Feuchte von DHT11


struct LPP_BLOCK {
  uint8_t typ;
  int val;
};

//Empfangsbuffer für maximal 8 Kanäle
LPP_BLOCK empBuf[8];


// Buffer CayenneLPP Datenformat.
CayenneLPP lpp(64);

//Instanz für Temperaturfühler
DHTesp dht;

//OLED Display
SSD1306  display(0x3c, 4, 15);

// Funktion zum Senden der Datenn an das Gateway
void sendLoRa() {
    int cnt;
    uint8_t sze;
    uint8_t ch;
    //Buffer löschen
    lpp.reset();
    //Datenpakete in den Buffer schreiben
    lpp.addTemperature(CHANNEL_TEMP, newValues.temperature);
    lpp.addRelativeHumidity(CHANNEL_HUM, newValues.humidity);
    lpp.addDigitalInput(CHANNEL_STATUS,relaisOn);
    lpp.addDigitalOutput(CHANNEL_RELAIS,0); //Das ist notwendig damit das Gateway einen Buffer für diesen Kanal anlegt
    //LoRa paket erstellen
    if (LoRa.beginPacket()){
      LoRa.write(devid,6); //zuerst die Geräte vID
      LoRa.write(lpp.getBuffer(),lpp.getSize()); //dann die Daten im LPP Format
      if (LoRa.endPacket()) {
        Serial.println("Lora Übertragung OK");
      } else {
        Serial.println("Lora Übertagung Fehler");
      }
    } else {
      Serial.println("Lora Übertagung Fehler");
    }
    Serial.printf("%3i Bytes gesendet\n",lpp.getSize()+6);
    //nun warten wir auf die Quittung
    cnt = 0;
    do {
      sze = LoRa.parsePacket();
      cnt++;
      delay(100);
    } while ((sze == 0) && (cnt < 100));
    if (cnt >= 100) {
      Serial.println("Keine Antwort vom Gateway");
    }
    else
    {
      Serial.printf("Daten empfangen %i Bytes\n",sze);
      if (sze >= 6){
        Serial.printf("Quittung erhalten %i Bytes\n",sze);
        cnt=0;
        //wir lesen die bersten 6 bytes und vergleichen diese mit der Geräte Id
        while ((sze > 0) && (cnt<6)) {
          sze--;
          devid[cnt++]==LoRa.read();
        }
        //wenn cnt = 6 ist war die ID richtig
        //Wir lesen den Rest in den Empfangsbuffer
        while (sze > 0) {
        //ertses Byte = Kanal 
        ch = LoRa.read();
        sze--;
        //ist der Kanal kleiner 8 speichern wir Typ und Werte
        if (ch < 8) {
          empBuf[ch].typ = LoRa.read();
          sze--;
          switch (empBuf[ch].typ) {
            //Nur Aktionstypen sind von Bedeutung              
            case LPP_DIGITAL_OUTPUT: empBuf[ch].val = LoRa.read();
              sze--;
              Serial.printf("Empfangen Kanal=%02x Typ=%02x Wert=%i \n",ch,empBuf[ch].typ,empBuf[ch].val);
              break;
            case LPP_ANALOG_OUTPUT: empBuf[ch].val = LoRa.read() * 256 + LoRa.read();
              sze-=2;
              Serial.printf("Empfangen Kanal=%02x Typ=%02x Wert=%i \n",ch,empBuf[ch].typ,empBuf[ch].val);
              break;
          }
        }
      }
      if (cnt == 6) {
        Serial.println("Quittung OK");
      } else {
        Serial.println("Ungültige Antwort");
      }
    }
  }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting");
    pinMode(RELAIS,OUTPUT);
    SPI.begin(5,19,27,18);
    esp_efuse_read_mac(devid);
    //OLED Reset
    pinMode(16,OUTPUT);
    digitalWrite(16, LOW); 
    delay(50);
    digitalWrite(16, HIGH);
    //und initialisieren
    display.init();
    display.setFont(ArialMT_Plain_10);
    display.display();    
    LoRa.setPins(SS,RST,DI0);
    Serial.println("LoRa TRX");
    if (!LoRa.begin(BAND)) {
      Serial.println("Starting LoRa failed!");
    }
    Serial.println("LoRa Initial OK!");
    LoRa.enableCrc();
    dht.setup(DHT_PIN, DHTesp::DHT11);
}

void loop() {
    uint8_t* buf;
    uint8_t len; 
    int cnt;
    uint8_t sze;
    newValues = dht.getTempAndHumidity();
    sendLoRa();
    digitalWrite(RELAIS,empBuf[3].val);
    relaisOn = empBuf[3].val;
    if (dht.getStatus() == 0) {
      display.clear();
      display.drawString(0, 0, "Temperatur: ");
      display.drawString(80, 0, String(newValues.temperature));
      display.drawString(110, 0,"°C");
      display.drawString(0, 20, "Feuchtigkeit  : ");
      display.drawString(80,20, String(newValues.humidity));
      display.drawString(110,20, "%");
      display.drawString(0, 40, "Relais :");
      if (relaisOn == 1) display.drawString(80, 40, "ein"); else display.drawString(80, 40, "aus");
      display.display();
      delay(TX_INTERVAL * 1000);  
    }
}
