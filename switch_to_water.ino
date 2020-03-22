void switch_to_water(void)

{
  switch (switch_to_water_status) {

    case STW_SWITCH_TO_WATER:
#ifdef debug
      Serial.println("switch to water");
#endif
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("      uzemmod:");
      lcd.setCursor(0, 1);
      lcd.print(" atallas csapvizre");
      //      lcd.setCursor(0, 2);
      //      lcd.print("                    ");
      lcd.setCursor(0, 3);
      lcd.print(String(DIVERTER_ON_WATER ? "3w:csapviz," : "3w:vizhuto,") + String(BYPASS_OPEN ? "2w:nyitva" : "2w:zarva"));


      io_0.digitalWrite(SSR , OFF); //SSR off
      delay(20);
      io_0.digitalWrite(BYPASS_VALVE, OPEN);
      io_0.digitalWrite(DIVERTER_VALVE, WATER);

      delay(50);
      io_0.digitalWrite(SSR , ON); //SSR on
      run_time = 0;
      switch_to_water_status = STW_WAIT_FOR_VALVES;
#ifdef debug
      Serial.println("current state=stw wait walve");
#endif
      break;

    case STW_WAIT_FOR_VALVES:
#ifdef debug
      Serial.println("stw wait for valves");
#endif
      run_time ++;
      SerialMon.println("run time : " + String(run_time));
      lcd.setCursor(0, 2);
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
        Serial.println("current state=stw error");
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
          Serial.println("current state=stw error");
#endif
          break;
        }
        if (!DIVERTER_RUN) {
          SerialMon.println("diverter szelephiba, a szelep nem megy!");
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("       HIBA!");
          //          lcd.setCursor(0, 1);
          //          lcd.print("                    ");
          lcd.setCursor(0, 2);
          lcd.print("DIVERT szelep hiba:");
          lcd.setCursor(0, 3);
          lcd.print("a szelep nem megy");
          current_state = ERROR_;
#ifdef debug
          Serial.println("current state=stw error");
#endif
          break;
        }
      }

      if ((run_time > 3) & !BYPASS_RUN & !DIVERTER_RUN) {
        SerialMon.println("Szelepek leálltak, runtime : " + String(run_time));
        io_0.digitalWrite(SSR , OFF); //SSR off

        if (BYPASS_OPEN & DIVERTER_ON_WATER) {
          current_state = RUN_ON_WATER;// ***********************************
#ifdef debug
          Serial.println("current state=stw run on water");
#endif
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
          if (DIVERTER_ON_COOLER) {
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
          Serial.println("current state=stw error");
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
        Serial.println("current state=stw error");
#endif
        break;
      }
  }
}
