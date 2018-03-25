int tempo = 200;
int duration[] = {1, 1, 2, 1, 1, 2, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2};

void playTheShit(char note, int duration) {
  char notesName[] = { 'c', 'd', 'e', 'f', 'g' };
  int tones[] = { 261, 293, 329, 349, 392 };

  for (int i = 0; i < sizeof(tones); i++) {
    // Bind the note took from the char array to the array notesName
    if (note == notesName[i]) {
      // Bind the notesName to tones
      tone(buzzerPIN, tones[i], duration);
    }
  }
}

void play(char notes[]) {
  // Scan each char from "notes"
  for (int i = 0; i < sizeof(notes) - 1; i++) {
    if (notes[i] == ' ') {
      // If find a space it rests
      delay(duration[i] * tempo);
    } else {
      playTheShit(notes[i], duration[i] * tempo);
    }
    // Pauses between notes
    delay((tempo * 2)*duration[i]);
  }
  noTone(buzzerPIN);
}


void wifi_init() {
  //WiFiManager
  WiFiManager wifiManager;
  //reset settings - for testing
  //wifiManager.resetSettings();

  //sets timeout until configuration portal gets turned off
  wifiManager.setTimeout(180);

  // id/name, placeholder/prompt, default, length
  WiFiManagerParameter custom_bot_token("token", "telegram bot token", conf.BOTtoken, 46);
  wifiManager.addParameter(&custom_bot_token);

  //fetches ssid and pass and tries to connect
  if (!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(5000);
  }
  //if you get here you have connected to the WiFi
  Serial.println("wifi connected!");
}

void eeprom_read_settings() {
  EEPROM.begin(768);

  //reset eeprom auth. for testing puposes
  //EEPROM.write(0, 255);
  if (EEPROM.read(0) == conf.en) {
    Serial.println("reading valid EEPROM");
    EEPROM.get(0, conf);
    //total 17, 4, 1, 4, 1, 4, 1, 4, 1, +reserved = 42
    //.id
    for (byte j = 0; j < authNumber; j++) {
      for (byte i = 0; i < 17; ++i) {
        char tmp = char(EEPROM.read(100 + (42 * j) + i));
        if ((tmp == '\0') || (tmp == 255)) {
          i = 17;
        }
        else {
          auth[j].id += tmp;
        }
      }
      //Serial.println(auth[j].id);
      //.TT float 4 byte
      EEPROM.get(100 + (42 * j) + 17, auth[j].TT);
      //.TD 1 byte
      EEPROM.get(100 + (42 * j) + 21, auth[j].TD);
      //.TT float 4 byte
      EEPROM.get(100 + (42 * j) + 22, auth[j].RHT);
      //.TD 1 byte
      EEPROM.get(100 + (42 * j) + 26, auth[j].RHD);
      //.TT float 4 byte
      EEPROM.get(100 + (42 * j) + 27, auth[j].vinT);
      //.TD 1 byte
      EEPROM.get(100 + (42 * j) + 31, auth[j].vinD);
    }
  } else {
    Serial.println("config not found on EEPROM, using defaults");
    for (byte j = 0; j < authNumber; j++) {
      EEPROM.write(100 + (42 * j), 255);
      EEPROM.write(100 + (42 * j) + 21, 0);
      EEPROM.write(100 + (42 * j) + 26, 0);
      EEPROM.write(100 + (42 * j) + 31, 0);
    }
    EEPROM.commit();
  }
}

void eeprom_save_settings() {
  EEPROM.put(0, conf);
  EEPROM.commit();
}

void getdht22() {
  temperature[1] = -127;
  humidity[1] = -127;
  int err = SimpleDHTErrSuccess;
  if ((err = dht22.read2(DHTPIN, &temperature[1], &humidity[1], NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Read DHT22 failed, err="); Serial.println(err);
  }
}

//////////////////////

String SState() {
  //String msg = "ESP sensor bot\n";
  String msg = "This is demm_bot.\n";
  msg += "Temperature: " + String(temperature[1]) + " °C\n";
  msg += "Relative Humidity: " + String(humidity[1]) + " RH%\n";
  msg += "Min:\n" + String(temperature[0]) + " °C " + String(humidity[0]) + "RH% \n";
  msg += "Max:\n" + String(temperature[2]) + " °C " + String(humidity[2]) + "RH% \n";
  return msg;
}

void handleNewMessages(int numNewMessages) {
  Serial.print("handleNewMessage: ");
  //Serial.println(String(numNewMessages));
  byte is_auth;
  String msg;

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot->messages[i].chat_id);
    String text = bot->messages[i].text;

    Serial.println(text);
    String from_name = bot->messages[i].from_name;
    if (from_name == "") from_name = "Guest";

    is_auth = 0;
    for (byte j = 0; j < authNumber; j++) {
      /*
        //debug
        Serial.println(j);
        Serial.println(chat_id);
        Serial.println(chat_id.length());
        Serial.println(int(chat_id[9]));
        Serial.println(auth[j].id);
        Serial.println(auth[j].id.length());
        Serial.println(int(auth[j].id[9]));
        Serial.println(int(auth[j].id[10]));
        //   */
      if (chat_id == auth[j].id) {
        is_auth = 1;
        j = authNumber;
      }
    }

    if (!is_auth) {
      Serial.println("unauthorized");

      if ((text == "/status") || (text == "/start") || (text == "/help")) {
        //msg = "ESP sensor bot\n";
        msg = "This is demm_bot.\n";
        msg += "This Chat ID is: " + chat_id + "\n\n";
        msg += "/status : Get this Message\n";
        msg += "/test : Get Test Messages";
        bot->sendMessage(chat_id, msg, "");
      }
      if (text == "/admin_xxxx") {
        //add chat_id to conf.auth_ids
        for (byte j = 0; j < authNumber; j++) {
          if (auth[j].id == "") {
            auth[j].id = chat_id;

            //Serial.println(j);
            //Serial.println(auth[j].id);
            //Serial.println(auth[j].id.length());

            for (byte i = 0; i < chat_id.length() + 1; ++i) {
              EEPROM.write(100 + (42 * j) + i, chat_id[i]);
            }
            EEPROM.commit();

            is_auth = 1;
            text = "/help";  //respond with /help
            j = authNumber;
            //bot->sendMessage(chat_id, "You have been registered!", "");
          }
        }
      }
    }
    if (is_auth) {
      Serial.println("authorized");
      if (text == "/logout") {
        for (byte j = 0; j < authNumber; j++) {
          if (chat_id == auth[j].id) {
            auth[j].id = "";
            EEPROM.write(100 + (42 * j), 255);
            EEPROM.commit();
            is_auth = 0;
            j = authNumber;
          }
        }
        bot->sendMessage(chat_id, "You have been unregistered!", "");
      }

      if (text == "/test") {
        for (byte j = 0; j < authNumber; j++) {
          if (auth[j].id != "") {
            bot->sendMessage(auth[j].id, "Test Message", "");
          }
        }
      }

      if (text == "/admin_purge") {
        for (byte j = 0; j < authNumber; j++) {
          if (auth[j].id != "") {
            bot->sendMessage(auth[j].id, "You have been unregistered!", "");
            auth[j].id = "";
            EEPROM.write(100 + (42 * j), 255);
            is_auth = 0;
          }
        }
        EEPROM.commit();
        bot->sendMessage(chat_id, "All Admins have been unregistered!", "");
      }

      if (text == "/restart") {
        bot->sendMessage(chat_id, "restarting", "");
        delay(1000);
        ESP.restart();//this only works if the esp has been restartet at least once manually (power|button)
        delay(3000);
      }

      if (text == "/tech") {
        msg = "demm_bot Statistics.\n";
        msg += String(WiFi.SSID()) + ": " + String(WiFi.RSSI()) + " dBm\n";
        msg += "IP address: " + WiFi.localIP().toString() + "\n";
        msg += "Vin: " + String(vin[1]) + " mV\n";
        msg += "Vin min: " + String(vin[0]) + " mV\n";
        msg += "Vin max: " + String(vin[2]) + " mV\n";
        msg += "uptime : " + String(millis() / (1000 * 60 * 60)) + " h\n\n";
        msg += "Your alarm Thresholds:\n";
        for (byte j = 0; j < authNumber; j++) {
          if (chat_id == auth[j].id) {
            if (auth[j].TD) {
              msg += "T: " + String(auth[j].TT) + " °C (" + String(auth[j].TD) + ")\n";
            }
            if (auth[j].RHD) {
              msg += "RH: " + String(auth[j].RHT) + " RH% (" + String(auth[j].RHD) + ")\n";
            }
            if (auth[j].vinD) {
              msg += "vin: " + String(auth[j].vinT) + " mV (" + String(auth[j].vinD) + ")\n";
            }
            j = authNumber;
          }
        }
        msg += "\nYou are Registered!\n";
        msg += "This Chat ID is: " + chat_id + "\n";
        msg += "Registered Chat IDs:\n";
        for (byte j = 0; j < authNumber; j++) {
          msg += auth[j].id + " ";
        }
        bot->sendMessage(chat_id, msg, "");
      }

      if ((text == "/status")) {
        bot->sendMessage(chat_id, SState(), ""); //send status
      }

      if ((text == "/help") || (text == "/start")) {
        //msg = "ESP sensor bot\n";
        msg = "This is demm_bot.\n";
        msg += "You are Registered!\n\n";
        msg += "/help : Get this Message\n";
        msg += "/status : Get Sensor Status\n";
        msg += "/tech : Get Statistics\n";
        msg += "/logout : Unregister\n";
        msg += "/test : Get Test Messages\n\n";
        msg += "Threshold Notifications:\n";
        msg += "/alarm_T_ : Temperature\n";
        msg += "/alarm_RH_ : Humidity\n";
        msg += "/alarm_V_ : Supply Voltage\n";
        msg += "eg: /alarm_RH_40\n";
        bot->sendMessage(chat_id, msg, "");
      }

      if (text.startsWith("/alarm_T_")) {
        for (byte j = 0; j < authNumber; j++) {
          if (chat_id == auth[j].id) {
            if (text.substring(9) != "") {
              auth[j].TT = text.substring(9).toFloat();
              if (auth[j].TT > temperature[1]) {
                auth[j].TD = 2;
                msg = "I will notify you once the Temperature increases to:\n";
              } else {
                auth[j].TD = 1;
                msg = "I will notify you once the Temperature drops to:\n";
              }
              msg += String(auth[j].TT) + " °C";
              bot->sendMessage(chat_id, msg, "");
            } else {
              auth[j].TD = 0;
              msg = "Alarm deactivated";
              bot->sendMessage(chat_id, msg, "");
            }
            EEPROM.put(100 + (42 * j) + 17, auth[j].TT);
            EEPROM.put(100 + (42 * j) + 21, auth[j].TD);
            EEPROM.commit();
            j = authNumber;
          }
        }
      }
      if (text.startsWith("/alarm_RH_")) {
        for (byte j = 0; j < authNumber; j++) {
          if (chat_id == auth[j].id) {
            if (text.substring(10) != "") {
              auth[j].RHT = text.substring(10).toFloat();
              if (auth[j].RHT > humidity[1]) {
                auth[j].RHD = 2;
                msg = "I will notify you once the Humidity increases to:\n";
              } else {
                auth[j].RHD = 1;
                msg = "I will notify you once the Humidity drops to:\n";
              }
              msg += String(auth[j].RHT) + " RH%";
              bot->sendMessage(chat_id, msg, "");
            } else {
              auth[j].RHD = 0;
              msg = "Alarm deactivated";
              bot->sendMessage(chat_id, msg, "");
            }
            EEPROM.put(100 + (42 * j) + 22, auth[j].RHT);
            EEPROM.put(100 + (42 * j) + 26, auth[j].RHD);
            EEPROM.commit();
            j = authNumber;
          }
        }
      }
      if (text.startsWith("/alarm_V_")) {
        for (byte j = 0; j < authNumber; j++) {
          if (chat_id == auth[j].id) {
            if (text.substring(9) != "") {
              auth[j].vinT = text.substring(9).toFloat();
              if (auth[j].vinT > vin[1]) {
                auth[j].vinD = 2;
                msg = "I will notify you once the Supply Voltage increases to:\n";
              } else {
                auth[j].vinD = 1;
                msg = "I will notify you once the Supply Voltage drops to:\n";
              }
              msg += String(auth[j].vinT) + " mV";
              bot->sendMessage(chat_id, msg, "");
            } else {
              auth[j].vinD = 0;
              msg = "Alarm deactivated";
              bot->sendMessage(chat_id, msg, "");
            }
            EEPROM.put(100 + (42 * j) + 32, auth[j].vinT);
            EEPROM.put(100 + (42 * j) + 36, auth[j].vinD);
            EEPROM.commit();
            j = authNumber;
          }
        }
      }
    }
    if (text == "/test") {
      for (int i = 0; i < 5; i++) {
        bot->sendMessage(chat_id, "test", "");
        delay(0);
      }
    }
  }
}

String alarm_notifications() {
  String msg;
  //alarm notifications
  for (byte j = 0; j < authNumber; j++) {
    //user threshold alarms
    if (auth[j].TD == 2) {
      if (auth[j].TT < temperature[1]) {
        //        auth[j].TD = 0;
        //          EEPROM.write(100 + (42 * j) + 21, 0);
        //          EEPROM.commit();
        msg = "The Temperature reached:\n";
        msg += String(auth[j].TT) + " °C\n";
        bot->sendMessage(auth[j].id, msg, "");
        bot->sendMessage(auth[j].id, SState(), "");  //send status
        play(notes_alarm);
      }
    }
    if (auth[j].TD == 1) {
      if (auth[j].TT > temperature[1]) {
        //        auth[j].TD = 0;
        //          EEPROM.write(100 + (42 * j) + 21, 0);
        //          EEPROM.commit();
        msg = "The Temperature reached:\n";
        msg += String(auth[j].TT) + " °C\n";
        bot->sendMessage(auth[j].id, msg, "");
        bot->sendMessage(auth[j].id, SState(), "");  //send status
        play(notes_alarm);
      }
    }
    if (auth[j].RHD == 2) {
      if (auth[j].RHT < humidity[1]) {
        auth[j].RHD = 0;
        //          EEPROM.write(100 + (42 * j) + 26, 0);
        //          EEPROM.commit();
        msg = "The Humidity reached:\n";
        msg += String(auth[j].RHT) + " RH%\n";
        bot->sendMessage(auth[j].id, msg, "");
        bot->sendMessage(auth[j].id, SState(), "");  //send status
      }
    }
    if (auth[j].RHD == 1) {
      if (auth[j].RHT > humidity[1]) {
        auth[j].RHD = 0;
        //          EEPROM.write(100 + (42 * j) + 26, 0);
        //          EEPROM.commit();
        msg = "The Humidity reached:\n";
        msg += String(auth[j].RHT) + " RH%\n";
        bot->sendMessage(auth[j].id, msg, "");
        bot->sendMessage(auth[j].id, SState(), "");  //send status
      }
    }
    if (auth[j].vinD == 2) {
      if (auth[j].vinT < vin[1]) {
        auth[j].vinD = 0;
        //          EEPROM.write(100 + (42 * j) + 36, 0);
        //          EEPROM.commit();
        msg = "The Supply Voltage reached:\n";
        msg += String(auth[j].vinT) + " mV\n";
        bot->sendMessage(auth[j].id, msg, "");
        bot->sendMessage(auth[j].id, SState(), "");  //send status
      }
    }
    if (auth[j].vinD == 1) {
      if (auth[j].vinT > vin[1]) {
        auth[j].vinD = 0;
        //          EEPROM.write(100 + (42 * j) + 36, 0);
        //          EEPROM.commit();
        msg = "The Supply Voltage reached:\n";
        msg += String(auth[j].vinT) + " mV\n";
        bot->sendMessage(auth[j].id, msg, "");
        bot->sendMessage(auth[j].id, SState(), "");  //send status
      }
    }
  }
  return msg;
}



