/*
 * Arbeiten mit dem Cayenne Dashboard - LoRa zu MQTT Gateway
 */



 /* Das MQTT Gateway bildet ein Interface zwischen LoRa Geräten bzw. ESP Nowe Geräten 
 *  und Cayenne MQTT Dashboards. Es läuft auf ESP32 mit LoRa und OLED Display
 *  Die Konfiguration erfolgt vom Browser
 */
#include <SPI.h>
#include <LoRa.h>
#include "SSD1306.h"
#include<Arduino.h>
#include <CayenneMQTTESP32.h>
#include <CayenneLPP.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include "FS.h"
#include "SPIFFS.h"


//Die Daten für diese Einstellung erhalten wir vom Cayenne Dashboard
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_CLIENTID ""

// Zugangsdaten für das lokale WLAN
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

//NTP Server zur Zeitsynchronisation
#define NTP_SERVER "de.pool.ntp.org"
#define GMT_OFFSET_SEC 3600
#define DAYLIGHT_OFFSET_SEC 0

//Pins für den LoRa Chip
#define SS      18
#define RST     14
#define DI0     26
//Frequenz für den LoRa Chip
#define BAND    433175000

//
#define MAXCHANNELS 256 //maximale Zahl der verwalteten Kanäle
#define MAXDEVICE 32 //maximale Anzahl der verwalteten Geräte MAXCHANNELS/MAXDEVICE = 8 ergibt die maximale Anzahl von Kanälen pro Gerät

//Format Flash Filesystem wenn noch nicht geschehen
#define FORMAT_SPIFFS_IF_FAILED true

#define DEBUG 1

//Bausteine für den Web-Server
const char HTML_HEADER[] =
"<!DOCTYPE HTML>"
"<html>"
"<head>"
"<meta name = \"viewport\" content = \"width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0>\">"
"<meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">"
"<title>MQTT Gateway</title>"
"<script language=\"javascript\">"
"function reload() {"
"document.location=\"http://%s\";}"
"</script>"
"<style>"
"body { background-color: #d2f3eb; font-family: Arial, Helvetica, Sans-Serif; Color: #000000;font-size:12pt; }"
"th { background-color: #b6c0db; color: #050ed2;font-weight:lighter;font-size:10pt;}"
"table, th, td {border: 1px solid black;}"
".titel {font-size:18pt;font-weight:bold;text-align:center;}"
"</style>"
"</head>"
"<body><div style='margin-left:30px;'>";
const char HTML_END[] =
"</div><script language=\"javascript\">setTimeout(reload, 10000);</script></body>"
"</html>";
const char HTML_TAB_GERAETE[] =
"<table style=\"width:100%\"><tr><th style=\"width:20%\">ID</th><th style=\"width:10%\">Nr.</th>"
"<th style=\"width:20%\">Kanäle</th><th style=\"width:20%\">Name</th>"
"<th style=\"width:20%\">Letzte Daten</th><th style=\"width:10%\">Aktion</th></tr>";
const char HTML_TAB_END[] =
"</table>";
const char HTML_NEWDEVICE[] =
"<div style=\"margin-top:20px;\">%s Name: <input type=\"text\" style=\"width:200px\" name=\"devname\" maxlength=\"10\" value=\"\"> <button name=\"registrieren\" value=\"%s\">Registrieren</button></div>";
const char HTML_TAB_ZEILE[] =
"<tr><td>%s</td><td>%i</td><td>%i bis %i</td><td>%s</td><td>%s</td><td><button name=\"delete\" value=\"%i\">Löschen</button></td></tr>";

//Datenstrukturen
//Nachrichten Buffer
struct MSG_BUF {
  uint8_t typ;
  uint8_t neu;
  uint8_t daten[10];
};

//Gerätedefinition
struct DEVICE {
  uint8_t aktiv;
  uint8_t dienst; //0=LoRa, 1=ESP-Now
  uint8_t id[6];
  String name;
  String last;
};

//Globale Variable
//Webserver Instanz
WebServer server(80);

//OLED Display
SSD1306  display(0x3c, 4, 15);

//Buffer zum Zwischenspeichern der Nachrichten je Kanal
MSG_BUF messages[MAXCHANNELS];

//Liste der definierten Geräte
DEVICE devices[MAXDEVICE];

//Id eines nicht registrierten Gerätes
uint8_t unbekannt[6];
//Flag immer dann wahr wenn ein neues Gerät entdeckt wurde
boolean neuesGeraet = false;
//Typ des neuen Gerätes 0=LöRa 1 =ESPNow
uint8_t neuesGeraetTyp = 0;

//Zähler und Aktivitaets Status für das Display
uint32_t loraCnt = 0; //Anzahl der empfangenen LoRa Nachrichten
String loraLast = ""; //Datum und Zeit der letzten empfangenen LoRa Nachricht
uint32_t nowCnt = 0; //Anzahl der empfangenen ESP Now Nachrichten
String nowLast = ""; //Datum und Zeit der letzten empfangenen LoRa Nachricht
uint32_t cayCnt = 0; //Anzahl der gesendeten MQTT Nachrichten
String cayLast = ""; //Datum und Zeit der letzten gesendeten MQTT Nachricht


//Funktion liefert Datum und Uhrzeit im Format yyyy-mm-dd hh:mm:ss als String
String getLocalTime()
{
  char sttime[20] = "";
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return sttime;
  }
  strftime(sttime, sizeof(sttime), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return sttime;
}

//Funktion liefert eine 6-Byte Geräte-Id im format xx:xx:xx:xx:xx:xx als String
String getId(uint8_t id[6])
{
  String stid;
  char tmp[4];
  sprintf(tmp,"%02x",id[0]);
  stid=tmp;
  for (uint8_t j = 1; j<6; j++) {
    sprintf(tmp,":%02x",id[j]);
    stid = stid += tmp ;
  }
  return stid;
}

//bereitet den Nachrichtenbuffer vor
//setzt alle Nachrichten auf erledigt
void initMessageBuffer() {
  for (int i = 0;i<MAXCHANNELS;i++) messages[i].neu = 0;
}

//Funktion zum Speichern der Konfiguration
void schreibeKonfiguration(const char *fn) {
  File f = SPIFFS.open(fn, FILE_WRITE);
  if (!f) {
    Serial.println(F("ERROR: SPIFFS Kann Konfiguration nicht speichern"));
    return;
  }
  for (uint8_t i = 0; i<MAXDEVICE; i++) {
    f.print(devices[i].aktiv);f.print(",");
    f.print(devices[i].dienst);f.print(",");
    f.print(getId(devices[i].id));f.print(",");
    f.print(devices[i].name);f.print(",");
    f.println(devices[i].last);
  }
}

//Funktion zum Registrieren eines neuen Gerätes
void geraetRegistrieren() {
  uint8_t i = 0;
  //suche freien Eintrag
  while ((i<MAXDEVICE) && devices[i].aktiv) i++;
  //gibt es keinen neuen Eintrag tun wir nichts
  if (i < MAXDEVICE) {
    //sonst Geraet registrieren Name = eingegebener Name 
    //oder unbekannt wenn keiner eingegeben wurde
    if (server.hasArg("devname")) {
      devices[i].name = server.arg("devname");
    } else {
      devices[i].name = "unbekannt";
    }
    for (uint8_t j = 0; j<6; j++) devices[i].id[j]=unbekannt[j];
    devices[i].aktiv = 1;
    devices[i].dienst= neuesGeraetTyp;
    devices[i].last = "";
    schreibeKonfiguration("/konfiguration.csv");
    neuesGeraet = false;
  }
}

//Service Funktion des Web Servers
void handleRoot() {
  char htmlbuf[1024];
  char tmp1[20];
  char tmp2[20];
  char tmp3[20];
  int index;
  //wurde der Lösch Knopf geklickt ?
  if (server.hasArg("delete")) {
    index = server.arg("delete").toInt();
#ifdef DEGUG
    Serial.printf("Lösche device %i =  ",index);
    Serial.println(devices[index].name);
#endif
    devices[index].aktiv=0;
    schreibeKonfiguration("/konfiguration.csv");
  }
  //wurde der Registrieren Knopf geklickt ?
  if (server.hasArg("registrieren")) {
    geraetRegistrieren();
  }
  //Aktuelle HTML Seite an Browser senden
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  //Header
  WiFi.localIP().toString().toCharArray(tmp1,20);
  sprintf(htmlbuf,HTML_HEADER,tmp1);
  server.send(200, "text/html",htmlbuf);
  //Formular Anfang
  server.sendContent("<div class=\"titel\">MQTT - Gateway</div><form>");
  //Tabelle der aktiven Geräte
  server.sendContent(HTML_TAB_GERAETE);
  for (uint8_t i = 0; i<MAXDEVICE; i++) { 
    if (devices[i].aktiv == 1) { 
      getId(devices[i].id).toCharArray(tmp1,20);
      devices[i].name.toCharArray(tmp2,20);
      devices[i].last.toCharArray(tmp3,20);
      sprintf(htmlbuf,HTML_TAB_ZEILE,tmp1,i,i*8,i*8+7,tmp2,tmp3,i);
      server.sendContent(htmlbuf);
    }
  }
  server.sendContent(HTML_TAB_END);
  //Falls ein neues Gerät gefunden wurde wird seine ID sowie ein Eingabefeld für den Namen
  // und ein Knopf zum Registrieren des neuen Gerätes angezeigt
  if (neuesGeraet) {
    getId(unbekannt).toCharArray(tmp1,20);
    sprintf(htmlbuf,HTML_NEWDEVICE,tmp1,tmp1);
    server.sendContent(htmlbuf);
  }
  server.sendContent(HTML_END);
}

//Funktion zum Suchen eines Gerätes in der Geräteliste
//Rückgabe Index des Gerätes oder -1 wenn es nicht gefunden wurde
int findDevice(uint8_t dev[6]) {
  uint8_t j;
  uint8_t i = 0;
  boolean found = false;
  do {
    j = 0;
    if (devices[i].aktiv == 0) {
      i++;
    } else {
      while ((j < 6) && (dev[j] == devices[i].id[j])) {j++;}
      found = (j == 6);
      if (!found) i++; 
    } 
  } while ((i<MAXDEVICE) && (!found));
  if (found) {return i;} else {return -1;}
}

//Funktion zum Anzeigen des Status am OLED Display
void anzeige() {
  display.clear();
  display.drawString(0,0,"MQTT Gateway");
  display.drawString(0,10,getLocalTime());
  display.drawString(0,20,WiFi.localIP().toString());
  display.drawString(0,34,"MQTT: ");
  display.drawString(60,34,String(cayCnt));
  display.drawString(0,44,"LoRa: ");
  display.drawString(60,44,String(loraCnt));
  display.drawString(0,54,"NOW:  ");
  display.drawString(60,54,String(nowCnt));
  display.display();
}


//Eine Nachricht von einem LoRa Client verarbeiten
void readLoRa() {
  int devnr;
  uint8_t devid[6];
  uint8_t channel;
  uint8_t typ;
  uint8_t len;
  uint8_t dat;
  boolean output;
  //Daten holen falls vorhanden
  int packetSize = LoRa.parsePacket();
  //haben wir Daten erhalten ?
  if (packetSize > 5) {
#ifdef DEBUG    
    Serial.println(getLocalTime());
    Serial.print(" RX ");
    Serial.print(packetSize);
    Serial.println(" Bytes");
    Serial.print("Device ID");
#endif 
    //zuerst die Geräte-Id lesen   
    for (uint8_t i=0; i<6;i++){
      devid[i]=LoRa.read();
#ifdef DEBUG
      Serial.printf("-%02x",devid[i]);
#endif
    }
#ifdef DEBUG
    Serial.println();
#endif
    //Restpaket berechnen
    packetSize -= 6;
    //nachschauen ob das Gerät registriert ist
    devnr = findDevice(devid);
    if (devnr >= 0)  {
      //wenn ja setzen wir den Zeitstempel für die letzte Meldung und
      //lesen die Daten
      devices[devnr].last = getLocalTime();
      schreibeKonfiguration("/konfiguration.csv");
      while (packetSize > 0) {
        //Kanalnummer = Gerätenummer * 16 + Gerätekanal
        channel = LoRa.read() + devnr*16;
#ifdef DEBUG
        Serial.printf("Kanal: %02x ",channel);
#endif
        //typ des Kanals
        typ = LoRa.read();
#ifdef DEBUG
        Serial.printf("Typ: %02x ",typ);
#endif
        //ermitteln der Länge des Datenpakets und ob der Kanal ein Aktuator ist
        output = false;
        switch(typ) {
          case LPP_DIGITAL_INPUT : len = LPP_DIGITAL_INPUT_SIZE - 2; break;
          case LPP_DIGITAL_OUTPUT : len = LPP_DIGITAL_OUTPUT_SIZE - 2; output = true; break;
          case LPP_ANALOG_INPUT : len = LPP_ANALOG_INPUT_SIZE - 2; break;
          case LPP_ANALOG_OUTPUT : len = LPP_ANALOG_OUTPUT_SIZE - 2; output = true; break;
          case LPP_LUMINOSITY : len = LPP_LUMINOSITY_SIZE - 2; break;
          case LPP_PRESENCE : len = LPP_PRESENCE_SIZE - 2; break;
          case LPP_TEMPERATURE : len = LPP_TEMPERATURE_SIZE - 2; break;
          case LPP_RELATIVE_HUMIDITY : len = LPP_RELATIVE_HUMIDITY_SIZE - 2; break;
          case LPP_ACCELEROMETER : len = LPP_ACCELEROMETER_SIZE - 2; break;
          case LPP_BAROMETRIC_PRESSURE : len = LPP_BAROMETRIC_PRESSURE_SIZE - 2; break;
          case LPP_GYROMETER : len = LPP_GYROMETER_SIZE - 2; break;
          case LPP_GPS : len = LPP_GPS_SIZE - 2; break;
          default: len =  0;
        }
        //ist der Kanal kein Aktuator, setzen wir im Nachrichtenbuffer neu auf 1
        //damit die Daten bei nächster Gelegenheit an den MQTT Server gesendet werden
        if (!output) messages[channel].neu =1;
        messages[channel].typ = typ;
        //Restpaket = 2 weniger da Kanal und Typ gelesen wurden
        packetSize -= 2;
#ifdef DEBUG
        Serial.print("Daten:");
#endif
        //nun lesen wir die empfangenen Daten mit der ermittelten Länge
        for (uint8_t i=0; i<len; i++) {
          dat = LoRa.read();
          //für Aktuatoren merken wir uns keine Daten
          if (! output) messages[channel].daten[i] = dat;
#ifdef DEBUG
          Serial.printf("-%02x",dat);
#endif
          //Restpaket um eins vermindern
          packetSize --;
        }
#ifdef DEBUG
        Serial.println();
#endif
      }
      //Status aktualisieren
      loraCnt++;
      loraLast = getLocalTime();
      anzeige();
    } else {
      //Das Gerät ist nicht registriert 
      //wir merken uns die Geräte-Id um sie für die Registriuerung anzuzeigen
      for (uint8_t i = 0; i<6; i++) unbekannt[i] = devid[i];
      neuesGeraet = true;
      neuesGeraetTyp = 0; //LoRa Gerät
    }
    //Teil zwei Antwort an das LoRa Gerät senden
    delay(100);
    LoRa.beginPacket();
    //am Anfang die Geräte-Id
    LoRa.write(devid,6);
    // wir prüfen ob wir Output Daten für das aktuelle LoRa-Gerät haben
    int devbase = devnr*16;
    for (int i = devbase; i<devbase+8; i++) {
      //je nach typ Digital oder Analogdaten
      switch (messages[i].typ) {
          case LPP_DIGITAL_OUTPUT : LoRa.write(i-devbase);
            LoRa.write(messages[i].typ);
            LoRa.write(messages[i].daten,1);
#ifdef DEBUG
            Serial.println("Digital Ausgang");
#endif
            break;
          case LPP_ANALOG_OUTPUT :  LoRa.write(i-devbase);
            LoRa.write(messages[i].typ);
            LoRa.write(messages[i].daten,2);
#ifdef DEBUG
            Serial.println("Analog Ausgang");
#endif
            break;
      }
    }
    
    int lstatus = LoRa.endPacket();
#ifdef DEBUG
    Serial.print("Sendestatus = ");
    Serial.println(lstatus);
#endif

  }
}

//Funktion zum lesen der Konfiguration
void leseKonfiguration(const char *fn) {
  uint8_t i = 0;
  String tmp;
  char hex[3];
  if (!SPIFFS.exists(fn)) {
    //existiert noch nicht dann erzeugen
    schreibeKonfiguration(fn);
    return;
  }
  File f = SPIFFS.open(fn, "r");
  if (!f) {
    Serial.println(F("ERROR:: SPIFFS Kann Konfiguration nicht öffnen"));
    return;
  }
  while (f.available() && (i<MAXDEVICE)) {
    tmp = f.readStringUntil(',');
    devices[i].aktiv = (tmp == "1");
    tmp = f.readStringUntil(',');
    devices[i].dienst = tmp.toInt();
    tmp = f.readStringUntil(',');
    for (uint8_t j=0; j<6; j++){
      hex[0]=tmp[j*3];
      hex[1]=tmp[j*3+1];
      hex[2]=0;
      devices[i].id[j]= (byte) strtol(hex,NULL,16);
    }
    tmp = f.readStringUntil(',');
    devices[i].name = tmp;
    tmp = f.readStringUntil(',');
    devices[i].last = tmp;
    i++;
  }
  
}

void setup() {
  //gerätespeicher initialisieren
  for (uint8_t i =0; i<MAXDEVICE; i++) devices[i].aktiv = 0;

  // OLED Display initialisieren
  pinMode(16,OUTPUT);
  digitalWrite(16, LOW);
  delay(50); 
  digitalWrite(16, HIGH);
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  //Serielle Schnittstelle starten
  Serial.begin(115200);
  while (!Serial); 
  Serial.println("Start");

  //Flash File syastem
  if (SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) Serial.println(F("SPIFFS geladen"));
  //Konfiguration einlesen
  leseKonfiguration("/konfiguration.csv");
 

  initMessageBuffer();

  //SPI und LoRa initialisieren
  SPI.begin(5,19,27,18);
  LoRa.setPins(SS,RST,DI0);
  Serial.println("LoRa TRX");
  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  LoRa.enableCrc();
  Serial.println("LoRa Initial OK!");
  delay(2000);
  //Mit dem WLAN und MQTT Server verbinden
  Serial.println("WLAN verbinden");
  Cayenne.begin(MQTT_USER, MQTT_PASSWORD, MQTT_CLIENTID, WIFI_SSID, WIFI_PASSWORD);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  //Web Server initialisieren
  server.on("/", handleRoot);
  server.begin();
  //Uhr mit Zeitserver synchronisieren
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  //Aktuelle Uhrzeit ausgeben
  Serial.println(getLocalTime());


}


void loop() {
  anzeige();
  //LoRa Interface auf Daten prüfen
  readLoRa();
  //mit Cayenne MQTT Server kommunizieren
  Cayenne.loop(1);
  //Web Server bedienen
  server.handleClient();

}

//Daten aus dem Nachrichtenbuffer an den MQTT Server senden
CAYENNE_OUT_DEFAULT()
{
  boolean output = false;
  boolean sentData = false;
#ifdef DEBUG
  Serial.println(getLocalTime());
  Serial.println("Cayenne send");
#endif
  for (int i = 0; i<MAXCHANNELS; i++) {
    //nur neue Nachrichten senden
    if (messages[i].neu == 1) {
#ifdef DEBUG
      Serial.printf("Sende MQTT Typ %i\n",messages[i].typ);
#endif
      //je nach Typ Daten senden
      switch (messages[i].typ) {
          case LPP_DIGITAL_INPUT : Cayenne.digitalSensorWrite(i,messages[i].daten[0]); break;
          case LPP_DIGITAL_OUTPUT : output = true; break;
          //case LPP_ANALOG_INPUT : Cayenne.virtualWrite(i,(messages[i].daten[0]*256 + messages[i].daten[1])/100,"analog_sensor",UNIT_UNDEFINED); break; break;
          case LPP_ANALOG_OUTPUT : output = true; break;
          case LPP_LUMINOSITY : Cayenne.luxWrite(i,messages[i].daten[0]*256 + messages[i].daten[1]); break;
          case LPP_PRESENCE : Cayenne.digitalSensorWrite(i,messages[i].daten[0]); break;
          case LPP_TEMPERATURE : Cayenne.celsiusWrite(i,(messages[i].daten[0]*256 + messages[i].daten[1])/10); break;
          case LPP_RELATIVE_HUMIDITY : Cayenne.virtualWrite(i,messages[i].daten[0]/2,TYPE_RELATIVE_HUMIDITY,UNIT_PERCENT); break;
          case LPP_ACCELEROMETER : Cayenne.virtualWrite(i,(messages[i].daten[0]*256 + messages[i].daten[1])/1000,"gx","g"); break;
          case LPP_BAROMETRIC_PRESSURE : Cayenne.hectoPascalWrite(i,(messages[i].daten[0]*256 + messages[i].daten[1])/10); break;
          //case LPP_GYROMETER : len = LPP_GYROMETER_SIZE - 2; break;
          //case LPP_GPS : len = LPP_GPS_SIZE - 2; break;
      }
      if (!output) {
        messages[i].neu = 0;
        sentData = true;
      }
      
    }
  }
  if (sentData) {
    //Status aktualisieren
    cayCnt++;
    cayLast = getLocalTime();
    anzeige();
  }

}

CAYENNE_IN_DEFAULT()
{
  uint8_t * pData;
  int val;
  int ch = request.channel;
#ifdef DEBUG
  Serial.println("Cayenne recive");
  Serial.printf("MQTT Daten für Kanal %i = %s\n",ch,getValue.asString());
#endif
  switch (messages[ch].typ) {
      case LPP_DIGITAL_OUTPUT : messages[ch].daten[0] = getValue.asInt();
        messages[ch].neu = 1;
        break;
      case LPP_ANALOG_OUTPUT :  val = round(getValue.asDouble()*100);
        messages[ch].daten[0] = val / 256;
        messages[ch].daten[1] = val % 256;
        messages[ch].neu = 1;
        break;
  }

  
}
