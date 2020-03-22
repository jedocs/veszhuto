void sync_valves(void)
{
  switch (sync_valves_status) {

    case SV_SYNC_TO_COOLER:
#ifdef debug
      Serial.println("sv sync to cooler");
#endif
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("      uzemmod:");
      lcd.setCursor(0, 1);
      lcd.print("szelepek");
      lcd.setCursor(0, 2);
      lcd.print("szinkronizalasa");
      lcd.setCursor(0, 3);
      lcd.print(String(DIVERTER_ON_WATER ? "3w:csapviz," : "3w:vizhuto,") + String(BYPASS_OPEN ? "2w:nyitva" : "2w:zarva"));


      io_0.digitalWrite(SSR , OFF); //SSR off
      delay(20);
      io_0.digitalWrite(BYPASS_VALVE, CLOSE); //BYPASS valve close
      io_0.digitalWrite(DIVERTER_VALVE, COOLER);
      delay(50);
      io_0.digitalWrite(SSR , ON); //SSR on
      run_time = 0;
      sync_valves_status = SV_WAIT_FOR_COOLER;
      break;

    case SV_WAIT_FOR_COOLER:
#ifdef debug
      Serial.println("sv wait for cooler");
#endif
      run_time ++;
      SerialMon.println("run time : " + String(run_time));

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("      uzemmod:");
      lcd.setCursor(0, 1);
      lcd.print("szelepek");
      lcd.setCursor(0, 2);
      lcd.print("szinkronizalasa");
      lcd.setCursor(0, 3);
      lcd.print("futasido: " + String(run_time) + " sec    ");

      if (run_time > RUN_TIME_LIMIT) {

        if (BYPASS_RUN) {
          SerialMon.println("BYPASS szelephiba, a szelep nem állt le!");
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("       HIBA!");
          //          lcd.setCursor(0, 1);
          //          lcd.print("                    ");
          lcd.setCursor(0, 2);
          lcd.print("BYPASS szelep hiba:");
          lcd.setCursor(0, 3);
          lcd.print("a szelep nem allt le");
        }
        if (DIVERTER_RUN) {
          SerialMon.println("DIVERTER szelephiba, a szelep nem állt le!");
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("       HIBA!");
          //          lcd.setCursor(0, 1);
          //          lcd.print("                    ");
          lcd.setCursor(0, 2);
          lcd.print("DIVERT szelep hiba:");
          lcd.setCursor(0, 3);
          lcd.print("a szelep nem allt le");
        }

        io_0.digitalWrite(SSR , OFF); //SSR off
        current_state = ERROR_;
#ifdef debug
        Serial.println("current state=sv error");
#endif
        break;
      }

      if (run_time == 3) {
        if (!BYPASS_RUN) {
          SerialMon.println("BYPASS szelephiba, a szelep nem megy!");
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("       HIBA!");
          //          lcd.setCursor(0, 1);
          //          lcd.print("                    ");
          lcd.setCursor(0, 2);
          lcd.print("BYPASS szelep hiba:");
          lcd.setCursor(0, 3);
          lcd.print("a szelep nem megy");
          current_state = ERROR_;
#ifdef debug
          Serial.println("current state=sv error");
#endif
          break;
        }
      }

      if ((run_time > 3) & !BYPASS_RUN & !DIVERTER_RUN) {
        SerialMon.println("Szelepek leálltak, runtime : " + String(run_time));
        io_0.digitalWrite(SSR , OFF); //SSR off

        if (BYPASS_CLOSED & DIVERTER_ON_COOLER) {
          current_state = RUN_ON_COOLER; //***********************************
#ifdef debug
          Serial.println("current state=sv run on cooler");
#endif
          break;
        }
        else {
          if (BYPASS_OPEN) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("       HIBA!");
            //            lcd.setCursor(0, 1);
            //            lcd.print("                    ");
            lcd.setCursor(0, 2);
            lcd.print("BYPASS szelep hiba:");
            lcd.setCursor(0, 3);
            lcd.print("a szelep beszorult");
          }
          if (DIVERTER_ON_WATER) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("       HIBA!");
            //            lcd.setCursor(0, 1);
            //            lcd.print("                    ");
            lcd.setCursor(0, 2);
            lcd.print("DIVERT szelep hiba:");
            lcd.setCursor(0, 3);
            lcd.print("a szelep beszorult");
          }
          current_state = ERROR_;
#ifdef debug
          Serial.println("current state=sv error");
#endif
          break;
        }
      }

      if (run_time > RUN_TIME_LIMIT)  {
        io_0.digitalWrite(SSR , OFF); //SSR off
        SerialMon.println("valami bug van a kódban!");
        //info += "sw bug in STC_WAIT_FOR_BYPASS_CLOSE";
        current_state = ERROR_;
#ifdef debug
        Serial.println("current state=sv error");
#endif
        break;
      }

      break;

    //************************************************************************

    case SV_SYNC_TO_WATER:
#ifdef debug
      Serial.println("sv sync to water");
#endif
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("      uzemmod:");
      lcd.setCursor(0, 1);
      lcd.print("szelepek");
      lcd.setCursor(0, 2);
      lcd.print("szinkronizalasa");
      lcd.setCursor(0, 3);
      lcd.print(String(DIVERTER_ON_WATER ? "3w:csapviz," : "3w:vizhuto,") + String(BYPASS_OPEN ? "2w:nyitva" : "2w:zarva"));


      io_0.digitalWrite(SSR , OFF); //SSR off
      delay(20);
      io_0.digitalWrite(BYPASS_VALVE, OPEN); //BYPASS valve open
      io_0.digitalWrite(DIVERTER_VALVE, WATER);
      delay(50);
      io_0.digitalWrite(SSR , ON); //SSR on
      run_time = 0;
      sync_valves_status = SV_WAIT_FOR_WATER;
      break;


    case SV_WAIT_FOR_WATER:
#ifdef debug
      Serial.println("sv wait for water");
#endif
      run_time ++;
      SerialMon.println("run time : " + String(run_time));

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("      uzemmod:");
      lcd.setCursor(0, 1);
      lcd.print("szelepek");
      lcd.setCursor(0, 2);
      lcd.print("szinkronizalasa");
      lcd.setCursor(0, 3);
      lcd.print("futasido: " + String(run_time) + " sec    ");


      if (run_time > RUN_TIME_LIMIT) {

        if (BYPASS_RUN) {
          SerialMon.println("BYPASS szelephiba, a szelep nem állt le!");
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("       HIBA!");
          //          lcd.setCursor(0, 1);
          //          lcd.print("                    ");
          lcd.setCursor(0, 2);
          lcd.print("BYPASS szelep hiba:");
          lcd.setCursor(0, 3);
          lcd.print("a szelep nem allt le");
        }
        if (DIVERTER_RUN) {
          SerialMon.println("DIVERTER szelephiba, a szelep nem állt le!");
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("       HIBA!");
          //          lcd.setCursor(0, 1);
          //          lcd.print("                    ");
          lcd.setCursor(0, 2);
          lcd.print("DIVERT szelep hiba:");
          lcd.setCursor(0, 3);
          lcd.print("a szelep nem allt le");
        }

        io_0.digitalWrite(SSR , OFF); //SSR off

        current_state = ERROR_;
#ifdef debug
        Serial.println("current state=sv error");
#endif
        break;
      }

      if (run_time == 3) {
        if (!BYPASS_RUN) {
          SerialMon.println("BYPASS szelephiba, a szelep nem megy!");
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("       HIBA!");
          //          lcd.setCursor(0, 1);
          //          lcd.print("                    ");
          lcd.setCursor(0, 2);
          lcd.print("BYPASS szelep hiba:");
          lcd.setCursor(0, 3);
          lcd.print("a szelep nem megy");
          current_state = ERROR_;
#ifdef debug
          Serial.println("current state=sv error");
#endif
          break;
        }
      }

      if ((run_time > 3) & !BYPASS_RUN & !DIVERTER_RUN) {
        SerialMon.println("Szelepek leálltak, runtime : " + String(run_time));
        io_0.digitalWrite(SSR , OFF); //SSR off

        if (BYPASS_OPEN & DIVERTER_ON_WATER) {
          startup_delay = 7;
          sync_valves_status = SV_WAIT_FOR_FLOW; //***********************************
          break;
        }
        else {
          if (BYPASS_CLOSED) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("       HIBA!");
            //            lcd.setCursor(0, 1);
            //            lcd.print("                    ");
            lcd.setCursor(0, 2);
            lcd.print("BYPASS szelep hiba:");
            lcd.setCursor(0, 3);
            lcd.print("a szelep beszorult");
          }
          break;
        }
      }
      break;
    case SV_WAIT_FOR_FLOW:

#ifdef debug
      Serial.println("sv wait for flow " + String(startup_delay));
#endif
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("      uzemmod:");
      lcd.setCursor(0, 1);
      lcd.print("    wait for flow");
      lcd.setCursor(0, 2);
      lcd.print("kesleltetes: " + String(startup_delay) + "sec");
      lcd.setCursor(0, 3);
      lcd.print(String(PRI_FLOW_OK ? "aramlas ok" : "nincs aramlas"));

      startup_delay--;
      if (startup_delay > 0) {
        break;
      }
      if ((pri_fwd_temp < PRI_FWD_THRESH) & (PRI_FLOW_OK)) {
#ifdef debug
        Serial.println("sv wait for flow ok, temp ok, switch to cooler");
#endif
        SerialMon.println("primer előremenő hőmérséklet OK, " + String(pri_fwd_temp) + "C");
        SerialMon.println("primer átfolyás OK");
        //info += "primer eloremeno hom.: " + String(pri_fwd_temp) + "C, atfolyas OK\n";
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
