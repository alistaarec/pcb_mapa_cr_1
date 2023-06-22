//Program pro MAPU TVOJÍ MÁMY na zobrazení radarových dat z CHMI 
//https://github.com/tomasbrincil/pcb_mapa_cr_1
//
//Část kódu převzata od : @pesvklobouku
//https://github.com/jakubcizek/pojdmeprogramovatelektroniku/tree/master/SrazkovyRadar
//
//Podpora ESP8266 i ESP32  !!!(ESP32 není otestováno)!!!
//Stavový Oled display ESP8266 D2 SDA, D1  SCL
//                      ESP32 D21 SDA, D22 SCL
//Nastavení hodnot přes WebServer
//Zobrazení Teplot z TMEP na mapě


#include <Arduino.h>
#include <Adafruit_NeoPixel.h> //https://github.com/adafruit/Adafruit_NeoPixel
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h> //https://github.com/adafruit/Adafruit_SSD1306
#include <DNSServer.h>
#include <EEPROM.h>
#include <WiFiManager.h>  //https://github.com/tzapu/WiFiManager/releases/tag/v2.0.16-rc.2

#ifdef ESP8266
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#endif

#ifdef ESP32
#include <HTTPClient.h>
#include <WiFi.h>
#include <WebServer.h>
#endif

//Výběr zdrojové adresy s daty
//#define URL1
#define URL2

#ifdef ESP8266
#define LEDPin 14
#endif

#ifdef ESP32
#define LEDPin 25
#endif

unsigned long lastchmiupdate = 0;
unsigned long lasttmepupdate = 0;
unsigned long lastanimchmi = 0;
unsigned long lastanimtmep = 0;   
unsigned long chmiupdate = 600000;
unsigned long tmepupdate = 60000;  
unsigned long animchmi = 120000;
unsigned long animtmep = 60000;

#ifdef URL1
const char *DataURL = "http://oracle-ams.kloboukuv.cloud/radarmapa/?chcu=posledni.json";
#endif
#ifdef URL2
const char *DataURL = "http://tmep.cz/app/export/okresy-srazky-laskakit.json";
#endif
const char *DataURL10 = "http://tmep.cz/app/export/okresy-srazky-laskakit-10.json";
const char *DataURL20 = "http://tmep.cz/app/export/okresy-srazky-laskakit-20.json";
const char *DataURL30 = "http://tmep.cz/app/export/okresy-srazky-laskakit-30.json";

const char *DataTemp = "http://cdn.tmep.cz/app/export/okresy-cr-teplota.json";
const char *DataHumi = "http://cdn.tmep.cz/app/export/okresy-cr-vlhkost.json";
const char *DataPres = "http://cdn.tmep.cz/app/export/okresy-cr-tlak.json";

String R0;
String R10;
String R20;
String R30;

String TmepTemp;
String TmepHumi;
String TmepPress;

int rok;
int mes;
int den;
int hod;
int minu;

bool decodeTmep;
int tmepColor;

int lastCall = 0;

//WEB nastaveni
int chmiUpdateInterval = 10;
int tmepUpdateInterval = 5;
int chmiDispInt = 2;
int tmepDispInt = 2;
int ledBrightness = 10;

int chmiactive = 1;
int tmepactive = 1;
int tempactive = 1;
int humiactive = 1;
int presactive = 1;
int testleds = 1;

//Převod ID pro mapu
int MapaTvojiMamy[72] = {
9, 11, 12, 8, 10, 13, 6, 15, 7, 5, 3, 14, 16, 67, 66, 4, 2, 24, 17, 1,
68, 18, 65, 64 ,0, 25, 76, 20, 69, 19, 27, 23, 73, 70, 21, 29, 28, 59, 
22, 71, 61, 63, 30, 72, 31, 26, 48, 46, 33, 39, 58, 49, 51, 47, 57, 40, 
32, 35, 56 ,38, 55, 34, 45, 41, 50, 36, 54, 52 ,37, 44, 53, 43

};

//EEPROM addr
int eepaddr = 0;
int controlnumber = 223;



Adafruit_NeoPixel mapLed(77, LEDPin, NEO_GRB + NEO_KHZ800);

StaticJsonDocument<10000> doc;

//Oled Display

#define OLED_W 128
#define OLED_H 32
#define OLED_RST -1
#define OLED_ADDR 0x3C
Adafruit_SSD1306 oled(OLED_W, OLED_H, &Wire, OLED_RST);

#ifdef ESP8266
ESP8266WebServer server(80);
#endif

#ifdef ESP32
WebServer server(80);
#endif

DNSServer dns;


//-----------Funkce------------

uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return mapLed.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return mapLed.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return mapLed.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

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
    if (decodeTmep == false) {
    JsonObject cas = doc["utc_datum"].as<JsonObject>();
      rok = cas["rok"];
      mes = cas["mesic"];
      den = cas["den"];
      hod = cas["hodina"];
      minu = cas["minuta"];
      Serial.printf ("Zobrazeny snimek : %d_%d_%d__%d:%d\r\n", rok, mes, den, hod, minu);

    mapLed.clear();
    JsonArray mesta = doc["seznam"].as<JsonArray>();
    for (JsonObject mesto : mesta) {  
      int id = mesto["id"];
      int r = mesto["r"];
      int g = mesto["g"];
      int b = mesto["b"];
       
      int idMama = MapaTvojiMamy[id];
      if (log) Serial.printf("Rozsvecuji mesto %d barvou R=%d G=%d B=%d", id, r, g, b);
      if (log) Serial.printf("  ---IDLaska %d --- IDMama %d\r\n", id, idMama);
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
  }
    else{
        for (JsonObject tmepId : doc.as<JsonArray>()) {
        int tmepLed = tmepId["id"];
        tmepLed -= 1;
        double h = tmepId["h"];
        tmepColor = map(h, -15, 40, 170, 0);
        mapLed.setPixelColor(tmepLed, Wheel(tmepColor));
      }
    }
    mapLed.show();
    decodeTmep = false;
    return 0;
  }
}

// Stazeni radarovych dat z webu
String updateData(String url) {
  #ifdef ESP8266
  WiFiClient client;
  HTTPClient http;
  http.begin(client, url);
  #endif

  #ifdef ESP32
  HTTPClient http;
  http.begin(DataURL);
  #endif
  String result;
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    result = http.getString();
    Serial.printf("OK %d\r\n", httpCode);
    oled.print("OK   ");
    oled.display();
  }
  else {
    Serial.printf("Error %d\r\n", httpCode);
    oled.print("Err  ");
    oled.display();
  }
  
  http.end();
  return result;
}


void readSettings() {
  eepaddr = 0;
  EEPROM.get(eepaddr, chmiUpdateInterval);
  eepaddr += 5;
  EEPROM.get(eepaddr, tmepUpdateInterval);
  eepaddr += 5;
  EEPROM.get(eepaddr, chmiDispInt);
  eepaddr += 5;
  EEPROM.get(eepaddr, tmepDispInt);
  eepaddr += 5;
  EEPROM.get(eepaddr, ledBrightness);
  eepaddr += 5;
  EEPROM.get(eepaddr, chmiactive);
  eepaddr += 5;
  EEPROM.get(eepaddr, tmepactive);
  eepaddr += 5;
  EEPROM.get(eepaddr, tempactive);
  eepaddr += 5;
  EEPROM.get(eepaddr, humiactive);
  eepaddr += 5;
  EEPROM.get(eepaddr, presactive);
  eepaddr += 5;
  EEPROM.get(eepaddr, testleds);


}

void saveSettings() {
  eepaddr = 0;
  EEPROM.put(eepaddr, chmiUpdateInterval);
  eepaddr += 5; 
  EEPROM.put(eepaddr, tmepUpdateInterval);
  eepaddr += 5;
  EEPROM.put(eepaddr, chmiDispInt);
  eepaddr += 5;
  EEPROM.put(eepaddr, tmepDispInt);
  eepaddr += 5;
  EEPROM.put(eepaddr, ledBrightness);
  eepaddr += 5;
  EEPROM.put(eepaddr, chmiactive);
  eepaddr += 5;
  EEPROM.put(eepaddr, tmepactive);
  eepaddr += 5;
  EEPROM.put(eepaddr, tempactive);
  eepaddr += 5;
  EEPROM.put(eepaddr, humiactive);
  eepaddr += 5;
  EEPROM.put(eepaddr, presactive);
  eepaddr += 5;
  EEPROM.put(eepaddr, testleds);
  EEPROM.commit();
}

void formatSettings() {

  eepaddr = 223;
  EEPROM.put(eepaddr, controlnumber);
  EEPROM.commit();
  saveSettings();
}


void oledSnimekTmep(String msg) {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(17, 0);
  oled.print("Zobrazeny Snimek");
  oled.setCursor(30, 15);
  oled.print(msg);
  oled.display();
}

void oledSnimek() {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(17, 0);
  oled.print("Zobrazeny Snimek");
  oled.setCursor(17, 15);
  oled.printf("%d/", rok);
  if(mes < 10) {
    oled.printf("0%d/", mes);
  } else {
    oled.printf("%d/", mes);
  }
  if(den < 10) {
    oled.printf("0%d/", den);
  } else {
    oled.printf("%d/", den);
  }
  if(hod < 10) {
    oled.printf("0%d:", hod);
  } else {
    oled.printf("%d:", hod);
  }
  if (minu < 10) {
    oled.printf("0%d", minu);
  } else {
    oled.print(minu);
  }
  oled.display();
  
}

void ledTest() {
  for (int l = 0; l < 77; l++) {
    mapLed.setPixelColor(l, mapLed.Color(255, 255, 255));
    mapLed.show();
    delay(50);
  }
  delay(1000);
  oled.clearDisplay();
  oled.display();
}

void downloadDataChmi() {
  oled.clearDisplay();
  oled.setCursor(10, 0);
  oled.print("Stahuji CHMI");
  oled.setCursor(5, 10);
  oled.print("Akt  -10  -20  -30");
  oled.display();
  oled.setCursor(5, 22);
  R0 = updateData(DataURL);
  delay(200);
  R10 = updateData(DataURL10);
  delay(200);
  R20 = updateData(DataURL20);
  delay(200);
  R30 = updateData(DataURL30);
  delay(2000);
  switch(lastCall)  {
    case 1:
      oledSnimek();
      break;
    case 2:
      oledSnimekTmep("TMEP Teplota");
      break;
    case 3:
      oledSnimekTmep("TMEP Vlhkost");
      break;
    case 4:
      oledSnimekTmep("TMEP Tlak");
      break;
  }
}

void downloadDataTmep() {
  oled.clearDisplay();
  oled.clearDisplay();
  oled.setCursor(10, 0);
  oled.print("Stahuji TMEP");
  oled.setCursor(5, 10);
  oled.print("Tep. Vlh. Tla.");
  oled.display();
  oled.setCursor(5, 22);
  TmepTemp = updateData(DataTemp);
  delay(200);
  TmepHumi = updateData(DataHumi);
  delay(200);
  TmepPress = updateData(DataPres);
  delay(2000);
  switch(lastCall)  {
    case 1:
      oledSnimek();
      break;
    case 2:
      oledSnimekTmep("TMEP Teplota");
      break;
    case 3:
      oledSnimekTmep("TMEP Vlhkost");
      break;
    case 4:
      oledSnimekTmep("TMEP Tlak");
      break;
  }



}

void animateRadar() {
  jsonDecoder(R30, false);
  oledSnimek();
  delay(2000);
  jsonDecoder(R20, false);
  oledSnimek();
  delay(2000);
  jsonDecoder(R10, false);
  oledSnimek();
  delay(2000);
  jsonDecoder(R0, false);
  oledSnimek();
  lastCall = 1;

}

void animateTmep() {
  if (tempactive == 1) {
    decodeTmep = true;
    oledSnimekTmep("TMEP Teplota");
    jsonDecoder(TmepTemp, false);
    lastCall = 2;
    delay(3000);
  }
  if (humiactive == 1) {
    decodeTmep = true;
    oledSnimekTmep("TMEP Vlhkost");
    jsonDecoder(TmepHumi, false);
    lastCall = 3;
    delay(3000);
  }
  if (presactive == 1) {
    decodeTmep = true;
    oledSnimekTmep("TMEP Tlak");
    jsonDecoder(TmepPress, false);
    lastCall = 4;
    delay(3000);
  }
  if (tempactive == 1) {
    decodeTmep = true;
    oledSnimekTmep("TMEP Teplota");
    jsonDecoder(TmepTemp, false);
    lastCall = 2;
    delay(3000);
  }
}

void setDisplay() {

  oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  oled.clearDisplay();
  oled.display();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(0, 5);
  delay(10);
  oled.print("Mapa Tvoji Mamy");
  oled.display();
  delay(2000);
  oled.clearDisplay();
  oled.display();

}

void variablesWeb() {

chmiUpdateInterval = chmiupdate / 1000 / 60;
tmepUpdateInterval = tmepupdate / 1000 / 60;
chmiDispInt = animchmi / 1000 / 60;
tmepDispInt = animtmep / 1000 / 60;

}

void updateWeb() {

chmiupdate = chmiUpdateInterval * 60 * 1000;
tmepupdate = tmepUpdateInterval * 60 * 1000;
animchmi = chmiDispInt * 60 * 1000;
animtmep = tmepDispInt * 60 * 1000;
mapLed.setBrightness(ledBrightness);
mapLed.show();

variablesWeb();

}

void handleRoot() {
  String html = "<html><body>";
  html += "<h1>Mapa Tvoji Mamy</h1>";
  html += "<br>";
  html += "<form action='/update' method='POST'>";
  html += "Interval Aktualiazce CHMI: <input type='number' name='ChmiUpdate' value='" + String(chmiUpdateInterval) + "'>Minuty<br>";
  html += "<br>";
  html += "Interval Aktualizace TMEP: <input type='number' name='TmepUpdate' value='" + String(tmepUpdateInterval) + "'>Minuty<br>";
  html += "<br>";
  html += "Interval Zobrazeni CHMI: <input type='number' name='ChmiDispInt' value='" + String(chmiDispInt) + "'> Minuty<br>";
  html += "<br>"; 
  html += "Interval Zobrazeni TMEP: <input type='number' name='TmepDispInt' value='" + String(tmepDispInt) + "'>Minuty<br>";
  html += "<br>";
  html += "Jas Led: <input type='number' name='ledBrightness' value='" + String(ledBrightness) + "'>0 - 100 %<br>";
  html += "<br>";
  html += "<label for='chmiact'>Zobrazit CHMI Data: </label>";
  if (chmiactive == 1) {
  html += "<input type='checkbox' id='chmiact' name='chmiact' value='1' checked ><br/>";
  } else {
  html += "<input type='checkbox' id='chmiact' name='chmiact' value='1' ><br/>";  
  }
  html += "<br>";
  html += "<label for='tmepact'>Zobrazit TMEP Data: </label>";
  if(tmepactive == 1) {
  html += "<input type='checkbox' id='tmepact' name='tmepact' value='1' checked ><br/>";
  } else {
  html += "<input type='checkbox' id='tmepact' name='tmepact' value='1' ><br/>";
  }
  html += "<br>";

  html += "<label for='tempact'>Snimek Teploty: </label>";
  if (tempactive == 1) {
  html += "<input type='checkbox' id='tempact' name='tempact' value='1' checked ><br/>";
  } else {
  html += "<input type='checkbox' id='tempact' name='tempact' value='1' ><br/>";  
  }
  html += "<br>";
  html += "<label for='humiact'>Snimek Vlhkosti: </label>";
  if(humiactive == 1) {
  html += "<input type='checkbox' id='humiact' name='humiact' value='1' checked ><br/>";
  } else {
  html += "<input type='checkbox' id='humiact' name='humiact' value='1' ><br/>";
  }
  html += "<br>";
  html += "<label for='presact'>Snimek Tlaku: </label>";
  if(presactive == 1) {
  html += "<input type='checkbox' id='presact' name='presact' value='1' checked ><br/>";
  } else {
  html += "<input type='checkbox' id='presact' name='presact' value='1' ><br/>";
  }
  html += "<br>";
  html += "<label for='tsled'>Test LED  : </label>";
  if(testleds == 1) {
  html += "<input type='checkbox' id='tsled' name='tsled' value='1' checked ><br/>";
  } else {
  html += "<input type='checkbox' id='tsled' name='tsled' value='1' ><br/>";
  }
  html += "<br>";

  html += "<input type='submit' value='Ulozit'>";
  html += "</form>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleUpdate() {
  chmiUpdateInterval = server.arg("ChmiUpdate").toInt();
  tmepUpdateInterval = server.arg("TmepUpdate").toInt();
  chmiDispInt = server.arg("ChmiDispInt").toInt();
  tmepDispInt = server.arg("TmepDispInt").toInt();
  ledBrightness = server.arg("ledBrightness").toInt();
  chmiactive = server.arg("chmiact").toInt();
  tmepactive = server.arg("tmepact").toInt();
  tempactive = server.arg("tempact").toInt();
  humiactive = server.arg("humiact").toInt();
  presactive = server.arg("presact").toInt();
  testleds = server.arg("tsled").toInt();  

  Serial.printf("chmiactive %d\r\n", chmiactive);
  Serial.printf("tmepactive %d\r\n", tmepactive);
  Serial.printf("tempactive %d\r\n", tempactive);
  Serial.printf("humiactive %d\r\n", humiactive);
  Serial.printf("presactive %d\r\n", presactive);
  
  updateWeb();
  saveSettings();

  String html = "<html><body>";
  html += "<h1>Mapa Tvoji Mamy</h1>";
  html += "<h2>Nastaveni Ulozeno</h2>";
  html += "<form>";
  html += "<input type= 'button' value = '<<Zpet do nastaveni' onclick='history.back()'>";
  html += "</form>";
  html += "</body></html>";

  server.send(200, "text/html", html);
  
}



void setup() {
  Serial.begin(115200);

  WiFiManager wifiManager;

  EEPROM.begin(512);
  eepaddr = 223;
  byte contnum = 0;
  EEPROM.get(eepaddr, contnum);

  if(contnum == controlnumber) {
    readSettings();
  } else {
    formatSettings();
  }

  setDisplay();

  oled.clearDisplay();
  oled.setCursor(5, 0);
  oled.print("Nastav WiFi");
  oled.setCursor(5, 10);
  oled.print("AP: MapaTvojiMamy");
  oled.setCursor(5, 22);
  oled.print("IP: 192.168.4.1");
  oled.display();


  wifiManager.autoConnect("MapaTvojiMamy");


  oled.display();

  Serial.println("");
  Serial.print(" IP: "); Serial.println(WiFi.localIP());
  oled.clearDisplay();
  oled.setCursor(0, 5);
  oled.print("IP: "); oled.print(WiFi.localIP());
  oled.display();
  delay(2000);

  mapLed.begin();
  mapLed.setBrightness(ledBrightness);
  mapLed.clear();
  mapLed.show();

  if (testleds == 1) {
  Serial.println("Test LED");
  oled.clearDisplay();
  oled.setCursor(0, 5);
  oled.print("TEST LED");
  oled.display();
  ledTest();
  }
  delay(20);
  downloadDataChmi();
  delay(20);
  downloadDataTmep();
  animateRadar();
  animateTmep();


  lastchmiupdate = millis();
  lasttmepupdate = millis();
  lastanimchmi = millis();
  lastanimtmep = millis();

  updateWeb();
  variablesWeb();

  server.on("/", handleRoot);
  server.on("/update", handleUpdate);

  server.begin();



}

void loop() {

  server.handleClient();

  if (millis() - lastchmiupdate >= chmiupdate) {
    lastchmiupdate = millis();
    if (chmiactive == 1) {
    downloadDataChmi();
    }
  }

  if (millis() - lasttmepupdate >= tmepupdate) {
    lasttmepupdate = millis();
    if (tmepactive == 1) {
    downloadDataTmep();
    }
  }

  if (millis() - lastanimchmi >= animchmi) {
    lastanimchmi = millis();
    if (chmiactive == 1) {
    animateRadar();
    }
  }

  if (millis() - lastanimtmep >= animtmep) {
    lastanimtmep = millis();
    if (tmepactive == 1) {
    animateTmep();
    }
  }

}