//Program pro MAPU TVOJÍ MÁMY na zobrazení radarových dat z CHMI 
//https://github.com/tomasbrincil/pcb_mapa_cr_1
//
//Část kódu převzata od : @pesvklobouku
//https://github.com/jakubcizek/pojdmeprogramovatelektroniku/tree/master/SrazkovyRadar
//
//Podpora ESP8266 i ESP32


#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

#ifdef ESP8266
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#endif

#ifdef ESP32
#include <HTTPClient.h>
#include <Wifi.h>
#endif

//SSID a Heslo k WiFi
const char *ssid = "SSID";
const char *heslo = "HESLO";

//Výběr zdrojové adresy s daty
//#define URL1
#define URL2

#ifdef ESP8266
#define LEDPin 5
#endif

#ifdef ESP32
#define LEDPin 25
#endif

unsigned long lastmill = 0;   
unsigned long timeout = 600000;  
int jas = 10;

#ifdef URL1
const char *DataURL = "http://oracle-ams.kloboukuv.cloud/radarmapa/?chcu=posledni.json";
#endif
#ifdef URL2
const char *DataURL = "http://tmep.cz/app/export/okresy-srazky-laskakit.json";
#endif

//Převod ID pro mapu
int MapaTvojiMamy[72] = {
9, 11, 12, 8, 10, 13, 6, 15, 7, 5, 3, 14, 16, 67, 66, 4, 2, 24, 17, 1,
68, 18, 65, 64 ,0, 25, 76, 20, 69, 19, 27, 23, 73, 70, 21, 29, 28, 59, 
22, 71, 61, 63, 30, 72, 31, 26, 48, 46, 33, 39, 58, 49, 51, 47, 57, 40, 
32, 35, 56 ,38, 55, 34, 45, 41, 50, 36, 54, 52 ,37, 44, 53, 43

};



Adafruit_NeoPixel mapLed(77, LEDPin, NEO_GRB + NEO_KHZ800);

StaticJsonDocument<10000> doc;


int jsonDecoder(String s, bool log) {
  DeserializationError e = deserializeJson(doc, s);
  if (e) {
    if (e == DeserializationError::InvalidInput) {
      return -1;
    } else if (e == DeserializationError::NoMemory) {
      return -2;
    } else {
      return -3;
    }
  } else {
    mapLed.clear();
    JsonArray mesta = doc["seznam"].as<JsonArray>();
    for (JsonObject mesto : mesta) {
      int id = mesto["id"];
      int r = mesto["r"];
      int g = mesto["g"];
      int b = mesto["b"];
       
      int idMama = MapaTvojiMamy[id];
      if (log) Serial.printf("Rozsvecuji mesto %d barvou R=%d G=%d B=%d", id, r, g, b);
      Serial.printf("  ---IDLaska %d --- IDMama %d\r\n", id, idMama);
      mapLed.setPixelColor(idMama, mapLed.Color(r, g, b));
      
      switch (id) {
        case 63: 
          mapLed.setPixelColor(42, mapLed.Color(r, g, b));
          break;
        case 26:
          mapLed.setPixelColor(74, mapLed.Color(r, g, b));
          mapLed.setPixelColor(75, mapLed.Color(r, g, b));
          break;
        case 40:
          mapLed.setPixelColor(60, mapLed.Color(r, g, b));
          mapLed.setPixelColor(62, mapLed.Color(r, g, b));
          break;
      }
    }
    mapLed.show();
    return 0;
  }
}

// Stazeni radarovych dat z webu
void updateData() {
  #ifdef ESP8266
  WiFiClient client;
  HTTPClient http;
  http.begin(client, DataURL);
  #endif

  #ifdef ESP32
  HTTPClient http;
  http.begin(DataURL);
  #endif

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    int err = jsonDecoder(http.getString(), true);
    switch (err) {
      case 0:
        Serial.println("Hotovo!");
        break;
      case -1:
        Serial.println("Spatny format JSONu");
        break;
      case -2:
        Serial.println("Malo pameti, navys velikost StaticJsonDocument");
        break;
      case -3:
        Serial.println("Chyba pri parsovani JSONu");
        break;
    }
  }
  http.end();
}

void ledTest() {
  for (int l = 0; l < 77; l++) {
    mapLed.setPixelColor(l, mapLed.Color(255, 255, 255));
    mapLed.show();
    delay(50);
  }
  delay(1000);
}


void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, heslo);
  Serial.println("");
  Serial.printf("Pripojuji se k %s ", ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  Serial.println("");
  Serial.print(" IP: "); Serial.println(WiFi.localIP());

  mapLed.begin();
  mapLed.setBrightness(jas);
  mapLed.clear();
  mapLed.show();

  Serial.println("Test LED");
  ledTest();
  delay(20);
  updateData();
}

void loop() {

  if (millis() - lastmill >= timeout) {
    lastmill = millis();
    updateData();
  }

}
