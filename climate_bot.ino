/*
  Author:   Eugene
  Date:     14.03.2018
  File:     climate_bot.ino
  Version:  v0.1

  Description:
  sensor bot with
  dht11 temp & humidity

  input voltage:
  ca.3.3V - max.5.4V

  telegram bot with customizable bot token.

  pinout:
  A0 voltage divider to 5V/Vin (gnd|100k|ADC|220k|A0|220k|5V/Vin)
  D0
  D5 DHT22 data (ext 10k pullup)
  D6 transistor beeper
  D7
  D8

  todo:
*/

//libraries
#include <ESP8266WiFi.h>          // https://github.com/esp8266/Arduino
#include <EEPROM.h>
//wifi manager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager

//ds18b20
#include <OneWire.h>
#include <DallasTemperature.h>
//dht22
#include <SimpleDHT.h>            // https://github.com/winlinvip/SimpleDHT
//mqtt
#include <PubSubClient.h>         // https://github.com/knolleary/pubsubclient
//telegram
#include <UniversalTelegramBot.h> // https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot


//pin definitions
const byte DHTPIN = D5;  // Pin which is connected to the DHT sensor.
//const byte EN_deepsleep = D6;
//library variables
const int maxaddr = 0x4A;
int Bot_mtbs = 1000; //mean time between scan messages //1000
long lastBOT;   //last time messages' scan has been done

//other library stuff
WiFiClient espClient;
PubSubClient mclient(espClient);
//SimpleDHT22 dht22;
SimpleDHT11 dht11;
WiFiClientSecure bclient;
UniversalTelegramBot *bot;

//OneWire oneWire(DHTPIN);
//// Pass our oneWire reference to Dallas Temperature.
//DallasTemperature DS18B20(&oneWire);


//program variables
//const word MQTT_INTERVAL = 15000;
const word LTH_INTERVAL = 2500;
const byte authNumber = 10;
word lowvoltage = 3300;
byte preReq, lowvoltagesent;
long lastLTH, lastMC;
word updateID = 1, vin[3] = {6000, 0, 0};
//min, current, max
float temperature[3] = {420, -127, -127};
float humidity[3] = {420, -127, -127};

//user struct. "id" is loaded from eeprom
struct authObj {
  String id;
  float TT;  // temperature threshold
  byte TD;   // direction of thershold; 0: off; 1: if temp<TT; 2: if temp>TT
  float RHT;
  byte RHD;
  float vinT;
  byte vinD;
};
authObj auth[authNumber];  //id: 52bit (arduino max 32bit) -> string ;_; (max 16 chars) = 170 bytes

//config struct
struct confObj {
  byte en;  //the enable/valid byte
  char mqtt_server[40];
  char mqtt_port[6];  //5 chars + 1 string end char
  char BOTtoken[47];  //46 chars
};  //max 94 bytes

confObj conf = {
  42,
  "192.168.11.44",
  "1883",
  "xx"
};

void setup() {
  //pinMode(EN_deepsleep, INPUT_PULLUP);
  Serial.begin(115200);

  eeprom_read_settings();

//  delay(10);
//  DS18B20.begin();

  wifi_init();

  //safe config
  eeprom_save_settings();

  //bot
  bot = new UniversalTelegramBot(conf.BOTtoken, bclient);
}



void loop() {
  String msg;

  if ((millis() - lastLTH) > LTH_INTERVAL) {
    lastLTH = millis();
    preReq++;

    Serial.print("lt");
    getdht11();
    //getds18();

    vin[1] = analogRead(A0) * 5.546;
    //min max storage
    if (temperature[1] < temperature[0]) {
      temperature[0] = temperature[1];
    }
    if (temperature[1] > temperature[2]) {
      temperature[2] = temperature[1];
    }
    if (humidity[1] < humidity[0]) {
      humidity[0] = humidity[1];
    }
    if (humidity[1] > humidity[2]) {
      humidity[2] = humidity[1];
    }
    if (vin[1] < vin[0]) {
      vin[0] = vin[1];
    }
    if (vin[1] > vin[2]) {
      vin[2] = vin[1];
    }
    Serial.print("h ");

    //low voltage alarm
    if ((vin[1] < lowvoltage) && (!lowvoltagesent)) {
      lowvoltagesent = 1;
      for (byte j = 0; j < authNumber; j++) {
        if (auth[j].id != "") {
          msg = "Low Supply Voltage:\n";
          msg += String(vin[1]) + " mV\n";
          bot->sendMessage(auth[j].id, msg, "");
          //bot->sendMessage(auth[j].id, SState(), "");  //send status
        }
      }
    }

    //alarm notifications
    for (byte j = 0; j < authNumber; j++) {
      //user threshold alarms
      if (auth[j].TD == 2) {
        if (auth[j].TT < temperature[1]) {
          auth[j].TD = 0;
          EEPROM.write(100 + (42 * j) + 21, 0);
          EEPROM.commit();
          msg = "The Temperature reached:\n";
          msg += String(auth[j].TT) + " �C\n";
          bot->sendMessage(auth[j].id, msg, "");
          bot->sendMessage(auth[j].id, SState(), "");  //send status
        }
      }
      if (auth[j].TD == 1) {
        if (auth[j].TT > temperature[1]) {
          auth[j].TD = 0;
          EEPROM.write(100 + (42 * j) + 21, 0);
          EEPROM.commit();
          msg = "The Temperature reached:\n";
          msg += String(auth[j].TT) + " �C\n";
          bot->sendMessage(auth[j].id, msg, "");
          bot->sendMessage(auth[j].id, SState(), "");  //send status
        }
      }
      if (auth[j].RHD == 2) {
        if (auth[j].RHT < humidity[1]) {
          auth[j].RHD = 0;
          EEPROM.write(100 + (42 * j) + 26, 0);
          EEPROM.commit();
          msg = "The Humidity reached:\n";
          msg += String(auth[j].RHT) + " RH%\n";
          bot->sendMessage(auth[j].id, msg, "");
          bot->sendMessage(auth[j].id, SState(), "");  //send status
        }
      }
      if (auth[j].RHD == 1) {
        if (auth[j].RHT > humidity[1]) {
          auth[j].RHD = 0;
          EEPROM.write(100 + (42 * j) + 26, 0);
          EEPROM.commit();
          msg = "The Humidity reached:\n";
          msg += String(auth[j].RHT) + " RH%\n";
          bot->sendMessage(auth[j].id, msg, "");
          bot->sendMessage(auth[j].id, SState(), "");  //send status
        }
      }
      if (auth[j].vinD == 2) {
        if (auth[j].vinT < vin[1]) {
          auth[j].vinD = 0;
          EEPROM.write(100 + (42 * j) + 36, 0);
          EEPROM.commit();
          msg = "The Supply Voltage reached:\n";
          msg += String(auth[j].vinT) + " mV\n";
          bot->sendMessage(auth[j].id, msg, "");
          bot->sendMessage(auth[j].id, SState(), "");  //send status
        }
      }
      if (auth[j].vinD == 1) {
        if (auth[j].vinT > vin[1]) {
          auth[j].vinD = 0;
          EEPROM.write(100 + (42 * j) + 36, 0);
          EEPROM.commit();
          msg = "The Supply Voltage reached:\n";
          msg += String(auth[j].vinT) + " mV\n";
          bot->sendMessage(auth[j].id, msg, "");
          bot->sendMessage(auth[j].id, SState(), "");  //send status
        }
      }
    }
  }

//  if (((millis() - lastMQTT) > MQTT_INTERVAL) && (preReq >= 2)) {  //
//    lastMQTT = millis();
//    preReq = 0;
//
//    Serial.print("DHT11 Values: ");
//    Serial.print((float)temperature[1]); Serial.print(" �C, ");
//    Serial.print((float)humidity[1]); Serial.println(" RH%");
//    Serial.println("Vin: " + String(vin[1]) + "mV");
//  }

  if (millis() > lastBOT + Bot_mtbs)  {
    int numNewMessages = bot->getUpdates(bot->last_message_received + 1);

    while (numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot->getUpdates(bot->last_message_received + 1);
    }

    lastBOT = millis();
  }

  //check for millis overflow
//  if (millis() < lastMQTT) {
//    lastMQTT = millis();
//  }
  if (millis() < lastLTH) {
    lastLTH = millis();
  }
  if (millis() < lastMC) {
    lastMC = millis();
  }
  if (millis() < lastBOT) {
    lastBOT = millis();
  }
  /*
    if(!digitalRead(EN_deepsleep)){
    Serial.print("DS ");
    delay(1000);
    //delay(2000);
    ESP.deepSleep(1000000);
    delay(1000);
    }*/
}
