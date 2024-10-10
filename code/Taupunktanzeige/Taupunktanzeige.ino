/*************************************************************************************************
                                      PROGRAMMINFO
**************************************************************************************************
Funktion: TFT 2.8 Taupunktanzeige 

**************************************************************************************************
Version: 09.10.2024
**************************************************************************************************
Board: ESP32 DEV Module
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
**************************************************************************************************
TFT Bibliotheken:

Adafruit_ILI9341 by Adafruit Version 1.6.00
Adafruit_GFX by Adafruit Version 1.11.9
XPT2046_Touchscreen by Paul Stoffregen Version 1.4.0
Touchevent by Gerald-Lechner Version 1.3.0

/***************************************************

Verdrahtung
TFT-Display  ESP32
VCC     3,3V
GND     GDN
CS      GPIO5
RESET   GPIO22
DC      GPIO4
SDI     GPIO23
SCK     GPIO18
LED     3,3V
MISO    GPIO19
T-CLK   GPIO18
T-CS    GPIO14
T-DIN   GPIO23
T-DO    GPIO19
T-IRQ   GPIO27

// TFT2.8 Farben
ILI9341_ORANGE
ILI9341_BLACK
ILI9341_RED
ILI9341_WHITE
ILI9341_YELLOW
ILI9341_LIGHTGREY
ILI9341_BLUE

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// ##################################################################################
// TFT_eSPI User_Setup.h
// TFT 2.8
// ##################################################################################

#define ST7735_DRIVER      // Define additional parameters below for this display
#define TFT_WIDTH  240 // ST7789 240 x 320
#define TFT_HEIGHT 320 // ST7789 240 x 320
#define ST7735_REDTAB

// For ESP32 Dev board (only tested with ILI9341 display)

#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   5   // Chip select control pin
#define TFT_DC   4   // Data Command control pin
#define TFT_RST  14  // Reset pin (could connect to RST pin)

#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:-.
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts
#define SMOOTH_FONT
#define SPI_FREQUENCY  27000000 // Actually sets it to 26.67MHz = 80/3
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
****************************************************/

#include "Adafruit_ILI9341.h"     //Display driver
#include <XPT2046_Touchscreen.h>  //Touchscreen driver
#include <TouchEvent.h>           

#define TOUCH_CS 14
#define TOUCH_IRQ 27
#define TFT_CS 5
#define TFT_DC 4
#define TFT_RST 22
#define TFT_LED 15
#define LED_ON 0

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

#include <OneWire.h>
#include <DallasTemperature.h>   
#include "DHT.h"

#define ONE_WIRE_BUS 10
#define DHTPIN 2
#define DHTTYPE DHT11
//#define DHTTYPE DHT21
//#define DHTTYPE DHT22


OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DHT dht(DHTPIN, DHTTYPE);

int LDRpin = 0;
char lightString [4];
String str; 
extern uint8_t temperatureIcon[];

int lightIntensity = 0;

#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define GREY    0x2108



#define MINPRESSURE 10
#define MAXPRESSURE 1000

// Farbschema
#define RED2RED 0
#define GREEN2GREEN 1
#define BLUE2BLUE 2
#define BLUE2RED 3
#define GREEN2RED 4
#define RED2GREEN 5

uint32_t runTime = -99999;       // nächstes Update
int reading = 0; // Anzuzeigender Wert
int d = 0; // Variable, die für die Sinuswellen-Testwellenform verwendet wird
boolean alert = 0;
int8_t ramp = 1;
int tesmod = 0;

float tempC = 0;

char TempCelciusFahrenheit[6];

float tempF = 0;

double dewPoint(double celsius, double humidity){
double RATIO = 373.15 / (273.15 + celsius);
double RHS = -7.90298 * (RATIO - 1);
RHS += 5.02808 * log10(RATIO);
RHS += -1.3816e-7 * (pow(10, (11.344 * (1 - 1 / RATIO ))) - 1) ;
RHS += 8.1328e-3 * (pow(10, (-3.49149 * (RATIO - 1))) - 1) ;
RHS += log10(1013.246);
double VP = pow(10, RHS - 3) * humidity;
double T = log(VP / 0.61078); // temp var
return (241.88 * T) / (17.558 - T);
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  sensors.begin();
  randomSeed(analogRead(15));
  tft.begin();
  tft.setRotation(3);

  tft.fillScreen(GREY); 
  tft.setCursor (250, 5);
  tft.setTextSize (2);
  tft.setTextColor (WHITE,GREY);
  tft.print ("rLF%");
  

  tft.setCursor (248, 57);
  tft.setTextColor (WHITE, GREY);
  tft.print ("Temp.");
  


    tft.setCursor (223,105);
    tft.setTextColor (WHITE,GREY);
    tft.print ("Taupunkt");

 /*   
    tft.setCursor (295,125);
    tft.setTextColor (GREEN,GREY);
    tft.print ("C");
    tft.setCursor (281,121);
    tft.print ("o");
*/
    tft.setCursor (245,155);
    tft.setTextColor (WHITE,GREY);
    tft.print ("Licht");

    tft.setCursor (235,205);
    tft.setTextSize (1);
    tft.setTextColor (WHITE,GREY);
    tft.print ("KHF");

    tft.setCursor (245,218);
    tft.setTextColor (WHITE,GREY);
    tft.print ("Taupunkt");

    tft.setCursor (270,230);
    tft.setTextColor (WHITE,GREY);
    tft.print ("Anzeige");

    
      
  //Felder
  tft.fillRect(0, 197, 217, 4, BLUE);
  tft.fillRect(217, 98, 320, 4, BLUE);
  tft.fillRect(217, 148, 320, 4, BLUE);
  tft.fillRect(217, 0, 4, 240, BLUE);
  tft.fillRect(221, 50, 320, 4, BLUE);
  tft.fillRect(221, 197, 320, 4, BLUE);
}


void loop() {

float tempC = 0;
float tempF = 0;
sensors.requestTemperatures();
tempC = sensors.getTempCByIndex(0);
tempF = sensors.toFahrenheit(tempC);

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float f = dht.readTemperature(true);
  float hif = dht.computeHeatIndex(f, h);

  lightIntensity = analogRead(LDRpin);
  lightIntensity = map(lightIntensity, 4096, 20, 0, 99);

//  str = String(lightIntensity);  //+"%"
//  str.toCharArray(lightString,5);
 
  //Licht
  tft.setCursor (250,175);
  tft.setTextSize(2);
  tft.setTextColor( YELLOW,GREY);
  tft.print(lightIntensity);   
  tft.print ("%");
  
   if(hif>0){  //Fahrenheit
    tft.setCursor (240,115);
    tft.setTextSize (3);
    tft.setTextColor (WHITE,GREY);
    tesmod=1;

    tft.setCursor (240,78); //Temperatur °C
    tft.setTextSize (2);
    tft.setTextColor (WHITE,GREY);
    tft.print (t,1);
    tft.print (" C");
  
    tft.setCursor (240,125);
    tft.setTextSize (2);
    tft.setTextColor (GREEN,GREY);
    tft.print (dewPoint(t, h));
    
    tft.setCursor (10,214);
    tft.setTextSize (2);
    tft.setTextColor (WHITE,GREY);
//    tft.print ("Fahrenheit");
    }

  /*  
    if(f>0){  //Fahrenheit
    tft.setCursor (150,207);
    tft.setTextSize (2);
    tft.setTextColor (WHITE,GREY);
    tft.print (f,0); 
    tesmod=1;}
  */

    
  if (millis() - runTime >= 500) { // Alle 500 ms ausführen
    runTime = millis();
    if (tesmod == 0) {
      reading = 40;
    }
    if (tesmod == 1) {
      reading = t ;
    }
    int xpos = 0, ypos = 5, gap = 4, radius = 52;
    // Große Anzeige
    xpos = 320 / 2 - 160, ypos = 0, gap = 100, radius = 100;
    ringMeter(reading, 1, 99, xpos, ypos, radius, "Temperatur", GREEN2RED); 
    if (h > 0) { //Humidity %
      tft.setCursor (245, 30); //157,208
      tft.setTextSize (2);
      tft.setTextColor (WHITE, GREY);
      tft.print (h, 1); tft.print ('%');
      tesmod = 1;
    }
  }
      tft.setTextColor (YELLOW);
 //   tft.setTextSize (6);
 //   tft.setCursor (185,60);
 //   tft.print ("C");
    tft.setTextSize (4);
    tft.setCursor (150,50);
    tft.print ("o");
}
//------------------ ENDE LOOP ------------------------------------------------------------


int ringMeter(int value, int vmin, int vmax, int x, int y, int r, char *units, byte scheme)
{
  
  x += r; y += r;   
  int w = r / 3;    
  int angle = 150;  
  int v = map(value, vmin, vmax, -angle, angle); 
  byte seg = 3; 
  byte inc = 6; 
  
  int colour = GREY;
  
  for (int i = -angle + inc / 2; i < angle - inc / 2; i += inc) {
    
    float sx = cos((i - 90) * 0.0174532925);
    float sy = sin((i - 90) * 0.0174532925);
    uint16_t x0 = sx * (r - w) + x;
    uint16_t y0 = sy * (r - w) + y;
    uint16_t x1 = sx * r + x;
    uint16_t y1 = sy * r + y;
    // Koordinatenpaar für das Segmentende berechnen
    float sx2 = cos((i + seg - 90) * 0.0174532925);
    float sy2 = sin((i + seg - 90) * 0.0174532925);
    int x2 = sx2 * (r - w) + x;
    int y2 = sy2 * (r - w) + y;
    int x3 = sx2 * r + x;
    int y3 = sy2 * r + y;
    if (i < v) { // Fülle farbige Segmente mit 2 Dreiecken aus
      switch (scheme) {
        case 0: colour = RED; break; // Fixed colour
        case 1: colour = GREEN; break; // Fixed colour
        case 2: colour = BLUE; break; // Fixed colour
        case 3: colour = rainbow(map(i, -angle, angle, 0, 127)); break; // Full spectrum blue to red
        case 4: colour = rainbow(map(i, -angle, angle, 70, 127)); break; // Green to red (high temperature etc)
        case 5: colour = rainbow(map(i, -angle, angle, 127, 63)); break; // Red to green (low battery etc)
        default: colour = BLUE; break; // Fixed colour
      }
      tft.fillTriangle(x0, y0, x1, y1, x2, y2, colour);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, colour);
      //text_colour = colour; // Speichert die zuletzt gezeichnete Farbe
    }
    else // Leere Segmente ausfüllen
    {
      tft.fillTriangle(x0, y0, x1, y1, x2, y2, GREY);
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, GREY);
    }
  }
  // Konvertieren eines Werts in eine Zeichenfolge
  char buf[10];
  byte len = 0;  if (value > 999) len = 5;
  dtostrf(value, 1, len, buf);
  //buf[len] = 1; 
  //buf[len] = 2; // Füge Leerzeichen und Terminator hinzu, hilft auch bei der Zentrierung des Textes!
  // Lege die Textfarbe auf die Standardeinstellung fest
  if (value > 7) {
    tft.setTextColor(colour, GREY);
    tft.setCursor(x - 30, y - 40); tft.setTextSize(6);
    tft.print(buf);
  }
  if (value < 10) {
    tft.setTextColor(WHITE, GREY);
    tft.setCursor(x - 30, y - 40); tft.setTextSize(6);
    tft.print(buf);
  }
  tft.setTextColor(GREEN, GREY);
  tft.setCursor(x - 39, y + 75); tft.setTextSize(2);
  tft.print(units); // Units display
  // Berechnen und zurückgeben der x-Koordinate auf der rechten Seite
  return x + r;
}

unsigned int rainbow(byte value)
{
  // Es wird erwartet, dass der Wert im Bereich von 0 bis 127 liegt
  // Der Wert wird in eine Spektrumsfarbe von 0 = Blau bis 127 = Rot umgewandelt
  byte red = 0; // Rot sind die oberen 5 Bit eines 16-Bit-Farbwerts
  byte green = 0;// Grün sind die mittleren 6 Bit
  byte blue = 0; // Blau sind die unteren 5 Bits
  byte quadrant = value / 32;
  if (quadrant == 0) {
    blue = 31;
    green = 2 * (value % 32);
    red = 0;
  }
  if (quadrant == 1) {
    blue = 31 - (value % 32);
    green = 63;
    red = 0;
  }
  if (quadrant == 2) {
    blue = 0;
    green = 63;
    red = value % 32;
  }
  if (quadrant == 3) {
    blue = 0;
    green = 63 - 2 * (value % 32);
    red = 31;
  }
  return (red << 11) + (green << 5) + blue;
}

// Gibt einen Wert im Bereich von -1 bis +1 für einen gegebenen Phasenwinkel in Grad zurück
float sineWave(int phase) {
  return sin(phase * 0.0174532925);
}
