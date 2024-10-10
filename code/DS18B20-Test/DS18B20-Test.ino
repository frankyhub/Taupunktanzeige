/*************************************************************************************************
                                      PROGRAMMINFO
**************************************************************************************************
Funktion: DC18B20 Testprogramm

**************************************************************************************************
Version: 10.10.2024
**************************************************************************************************
Board: ESP32vn IoT UNO
**************************************************************************************************
Libraries:
https://github.com/espressif/arduino-esp32/tree/master/libraries
C:\Users\User\Documents\Arduino
D:\gittemp\Arduino II\A156_Wetterdaten_V3
**************************************************************************************************
C++ Arduino IDE V1.8.19
**************************************************************************************************
Einstellungen:
https://dl.espressif.com/dl/package_esp32_index.json
http://dan.drown.org/stm32duino/package_STM32duino_index.json
http://arduino.esp8266.com/stable/package_esp8266com_index.json
**************************************************************************************************/
//DS18B20 Temperatursensor Test
//
#include <OneWire.h>                        //OneWire Bibliothek einbinden
#include <DallasTemperature.h>              //DallasTemperatureBibliothek einbinden

#define ONE_WIRE_BUS 15                      //Data ist an Pin 2 des Arduinos angeschlossen

OneWire oneWire(ONE_WIRE_BUS);              //Start des OneWire Bus
DallasTemperature sensors(&oneWire);        //Dallas Temperature referenzieren


void setup(void) { 
 Serial.begin(115200);                        // Start der seriellen Konsole 
 Serial.println("DS18B20 Demo"); 
 sensors.begin();                           // Sensor Start
} 

void loop(void) { 
 sensors.requestTemperatures();             // Temperaturen Anfragen 
 Serial.print("Temperatur: "); 
 Serial.print(sensors.getTempCByIndex(0));  // "byIndex(0)" spricht den ersten Sensor an  
 Serial.println(" °C "); 
 delay(1000);                                // eine Sekunde warten bis zur nächsten 
}
