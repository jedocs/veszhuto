#if defined (mmc) || defined (szolnok)

//***********************************************************************************************
//*
//*   state machine
//*
//***********************************************************************************************
void State_Machine(void) {
#ifdef debug
  Serial.println("stc status" + String(switch_to_cooler_status));
  Serial.println("current state" + String(current_state));
#endif
  switch (current_state)
  {
    case PRE_STARTUP:
#ifdef debug
      Serial.println("pre-startup");
#endif
      old_bypass_pos = BYPASS_OPEN;
      old_diverter_pos = DIVERTER_ON_WATER;
      current_state = STARTUP;
#ifdef debug
      Serial.println("current state=startup");
#endif
      break;

    case STARTUP :
#ifdef debug
      Serial.println("startup");
#endif
      Serial.println ("starting up " + String(startup_delay));

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("      uzemmod:");
      lcd.setCursor(0, 1);
      lcd.print("   inicializalas");
      lcd.setCursor(0, 2);
      lcd.print("kesleltetes: " + String(startup_delay) + " sec");
      lcd.setCursor(0, 3);
      lcd.print(String(DIVERTER_ON_WATER ? "3w:csapviz," : "3w:vizhuto,") + String(BYPASS_OPEN ? "2w:nyitva" : "2w:zarva"));

      startup_delay--;
      if (startup_delay > 0) {
        break;
      }
      //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      if ((old_bypass_pos != BYPASS_OPEN) | (old_diverter_pos != DIVERTER_ON_WATER)) {
        old_bypass_pos = BYPASS_OPEN;
        old_diverter_pos = DIVERTER_ON_WATER;
        startup_delay = STARTUP_DELAY;
        break;
      }

      io_0.digitalWrite(BYPASS_VALVE, BYPASS_CLOSED); //switch bypass relay to actual valve pos
      io_0.digitalWrite(DIVERTER_VALVE, DIVERTER_ON_COOLER); //switch div relay to actual valve pos

      Serial.println(String((BYPASS_OPEN ? "bypass NYITVA" : "bypass ZÁRVA")));
      Serial.println(String((DIVERTER_ON_COOLER ? "váltószelep VÍZHŰTŐ" : "váltószelep CSAPVÍZ")));

      if (BYPASS_RUN | DIVERTER_RUN) { //szelep hiba
        if (BYPASS_RUN) {
          SerialMon.println("bypass szelep megy pedig nem kéne!");
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("       HIBA!");
          //          lcd.setCursor(0, 1);
          //          lcd.print("                    ");
          lcd.setCursor(0, 2);
          lcd.print("BYPASS szelep hiba:");
          lcd.setCursor(0, 3);
          lcd.print("a szelep mozog (?)");
        }
        if (DIVERTER_RUN) {
          SerialMon.println("diverter szelep megy pedig nem kéne!");
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("       HIBA!");
          //          lcd.setCursor(0, 1);
          //          lcd.print("                    ");
          lcd.setCursor(0, 2);
          lcd.print("DIVERT szelep hiba:");
          lcd.setCursor(0, 3);
          lcd.print("a szelep mozog (?)");
        }
        current_state = ERROR_;
#ifdef debug
        Serial.println("current state=error");
#endif
        break;
      }
      takeover_valves = true;
      delay(20);
      SerialMon.println("szelepvezérlés átvéve");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("      uzemmod:");
      lcd.setCursor(0, 1);
      lcd.print("   inicializalas");
      lcd.setCursor(0, 2);
      lcd.print("szelepvezerles aktiv");
      lcd.setCursor(0, 3);
      lcd.print(String(DIVERTER_ON_WATER ? "3w:csapviz," : "3w:vizhuto,") + String(BYPASS_OPEN ? "2w:nyitva" : "2w:zarva"));

      if (DIVERTER_ON_COOLER != BYPASS_CLOSED) {
        if (DIVERTER_ON_COOLER) {
          sync_valves_status = SV_SYNC_TO_COOLER;
        }
        else {
          sync_valves_status = SV_SYNC_TO_WATER;
        }
        current_state = SYNC_VALVES;
#ifdef debug
        Serial.println("current state=sync valves");
#endif
        break;
      }
      else {
        if (DIVERTER_ON_COOLER) {
          current_state = RUN_ON_COOLER;
#ifdef debug
          Serial.println("current state=run on cooler");
#endif
          break;
        }
        else {
          if ((pri_fwd_temp < PRI_FWD_THRESH) & (PRI_FLOW_OK)) {
            SerialMon.println("primer előremenő hőmérséklet OK, " + String(pri_fwd_temp) + "C");
            SerialMon.println("primer átfolyás OK");

            Serial.println ("átállás vízhűtőre!!");
            publish_info = "Atallas vizhutore!\n";
            publish_info += "primer atfolyas " + String((PRI_FLOW_OK ? "OK\n" : "NOK\n"));
            publish_info += "primer fwd temp: " + String(pri_fwd_temp) + "C\n";
            publish_info += SITE;
            publish_info += " tavfelugyelet";
            send_SMS = true;
            send_mail = true;
            publish();

            switch_to_cooler_status = STC_SWITCH_TO_COOLER;
            current_state = SWITCH_TO_COOLER;
#ifdef debug
            Serial.println("current state=sw to cooler");
#endif
            //info+="bypass szelep nyitva -> zárás\n";
            break;
          }
          else {
            current_state = RUN_ON_WATER;
            break;
          }
        }
      }

    //************************************************************************************************
    case SYNC_VALVES:

      sync_valves();
      break;
    //************************************************************************************************

    case SWITCH_TO_COOLER:

      switch_to_cooler();
      break;
    //*************************************************************************

    case RUN_ON_COOLER:
#ifdef debug
      Serial.println("run on cooler");
#endif
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("uzemmod: vizhuto");
      lcd.setCursor(0, 1);
      lcd.print("eloremeno: " + String(pri_fwd_temp) + "C");
      lcd.setCursor(0, 2);
      lcd.print("visszatero: " + String(pri_return_temp) + "C");
      lcd.setCursor(0, 3);
      lcd.print("helyiseg: " + String(room_temp) + "C");

      if (!(io_0.digitalRead(AC_MONITOR))  & AC_OK) {
#ifdef debug
        Serial.println("roc ac ok");
#endif
        if ((pri_fwd_temp > PRI_FWD_THRESH) | !PRI_FLOW_OK) { //
#ifdef debug
          if (pri_fwd_temp > PRI_FWD_THRESH) {
            Serial.println("temp above thresh" + String(pri_fwd_temp));
          }
          else {
            Serial.println("pri flow nok");
          }
#endif
          io_0.digitalWrite(SSR, OFF);
          Serial.println ("átállás csapvízre!!");
          publish_info = "Atallas csapvizre!\n";
          publish_info += "primer atfolyas " + String((PRI_FLOW_OK ? "OK\n" : "NOK\n"));
          publish_info += "primer fwd temp: " + String(pri_fwd_temp) + "C\n";
          publish_info += SITE;
          publish_info += " tavfelugyelet";
          send_SMS = true;
          send_mail = true;
          publish();
          switch_to_water_status = STW_SWITCH_TO_WATER;
          current_state = SWITCH_TO_WATER;
#ifdef debug
          Serial.println("current state=roc sw to water");
#endif
        }
      }
      else {
        lcd.setCursor(0, 3);
        lcdinfo = "";
        if (!((io_0_inputs >> AC_MONITOR) & 0x1)) {
          lcdinfo += ("ac ok");
        }
        else {
          lcdinfo += ("ac NOK");
        }
        if (AC_OK) {
          lcdinfo += ("AC ok");
        }
        else {
          lcdinfo += ("AC NOK");
        }

        //lcd.clear();
        lcd.print(lcdinfo);
        lcdinfo = "";
      }
      break;

    //*********************************************************************************************

    case SWITCH_TO_WATER:

      switch_to_water();
    //******************************************
    case RUN_ON_WATER:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("uzemmod: csapviz");
      lcd.setCursor(0, 1);
      lcd.print("eloremeno: " + String(sec_fwd_temp) + "C");
      lcd.setCursor(0, 2);
      lcd.print("visszatero: " + String(pri_return_temp) + "C");
      lcd.setCursor(0, 3);
      lcd.print("atfolyas: " + String(water_flow) + " l/p");
      break;

    case ERROR_:
#ifdef debug
      Serial.println("ERROR");
#endif
      Serial.println("**************ERROR******************");
      io_0.digitalWrite(PUMP, OFF);
      io_0.digitalWrite(SSR , OFF); //SSR off
      takeover_valves = false;

      publish_info += " **** ERROR ****";
      send_SMS = true;
      send_mail = true;
      publish();
      current_state = ERROR_WAIT;
#ifdef debug
      Serial.println("current state=error wait");
#endif
      break;

    case ERROR_WAIT:
      break;

    case WAIT_FOR_AC:
      if (AC_OK) {
        old_bypass_pos = BYPASS_OPEN;
        old_diverter_pos = DIVERTER_ON_WATER;
        current_state = STARTUP;
#ifdef debug
        Serial.println("current state=startup");
#endif
      }
      break;
    default :
      Serial.println ("switch - case hiba");
  }
}

#endif
