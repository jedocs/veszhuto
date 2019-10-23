//Serial.findUntil()
//Serial.setTimeout()


//***********************************************************************************************
//*
//*   GSM task
//*
//***********************************************************************************************
void TaskGSM(void *pvParameters) {
  struct  publish_data data_for_publish;
  unsigned char cnt = 0;
  String email = "Teszt email, szám:";

  // Set GSM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  SerialMon.println("Initializing modem...");
  modem.restart();

  // Unlock your SIM card with a PIN if needed
  if (strlen(simPIN) && modem.getSimStatus() != 3 ) {
    modem.simUnlock(simPIN);
  }
  delay(5000);
  /*
    WiFi.begin(ss45did, password);
    Serial.println("Connecting");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      //Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to WiFi network with IP Address: ");
    Serial.println(WiFi.localIP());
  */

  while (1) {

    if (xQueueReceive(queue, &data_for_publish, 0)) {
      // modem.restart();
      //Serial.println("   time: " + modem.getGSMDateTime(DATE_TIME));
      if (send_SMS) {
        Serial.println("sending sms from gsm task");
        if (modem.sendSMS(SMS_TARGET, publish_info)) {
          SerialMon.println(publish_info);
          send_SMS = false;
        }
        else {
          SerialMon.println("SMS failed to send");
          send_SMS = false;
        }
      }

      SerialMon.print("Connecting to APN: ");
      SerialMon.print(apn);
      if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println(" fail");
      }
      else {
        SerialMon.println(" connected");
      }
      ThingSpeak.begin(client);
      // set the fields with the values
      ThingSpeak.setField(1, data_for_publish.pri_fwd_temp);
      ThingSpeak.setField(2, data_for_publish.pri_return_temp);
      ThingSpeak.setField(3, data_for_publish.sec_fwd_temp);
      ThingSpeak.setField(4, data_for_publish.sec_return_temp);
      ThingSpeak.setField(5, data_for_publish.water_meter);
      ThingSpeak.setField(6, data_for_publish.vbat);
      if (!rr_published) ThingSpeak.setField(7, data_for_publish.reset_reason);
      else ThingSpeak.setField(7, 0);
      ThingSpeak.setField(8, data_for_publish.status_code);

      if (publish_info) ThingSpeak.setStatus(publish_info);
      Serial.println("start publishing data");
      int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
      if (x == 200) {
        Serial.println("Channel update successful.");
        rr_published = true;
        publish_info = "";
      }
      else {
        Serial.println("Problem updating channel. HTTP error code " + String(x));
        if (x == 0) {
          rr_published = true;
          publish_info = "";
        }
        /*if (x != 0) {
          // Restart SIM800 module, it takes quite some time
          // To skip it, call init() instead of restart()
          SerialMon.println("Initializing modem...");
          modem.restart();
          // Unlock your SIM card with a PIN if needed
          //if (strlen(simPIN) && modem.getSimStatus() != 3 ) {
          //modem.simUnlock(simPIN);
          //}
          SerialMon.print("Connecting to APN: ");
          SerialMon.print(apn);
          if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
            SerialMon.println(" fail");
          }
          else {
            SerialMon.println(" connected");
          }
          }
          else {
          rr_published = true;
          publish_info = "";
          }*/
      }
      client.stop();
      SerialMon.println(F("Server disconnected"));
      modem.gprsDisconnect();
      SerialMon.println(F("GPRS disconnected"));
      //modem.radioOff();
    }
    modem.SendEmail(email+String(cnt));
    cnt+=1;
    delay(20000);
    //Serial.println("GSM task alive on core " + String(xPortGetCoreID()) + "\n");
  }
}
