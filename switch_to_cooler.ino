#if defined (mmc) || defined (szolnok)
void switch_to_cooler(void)
{

  switch (switch_to_cooler_status) {

    case STC_SWITCH_TO_COOLER:

#ifdef debug
      Serial.println("stc switch to cooler");
#endif
      io_0.digitalWrite(SSR , OFF); //SSR off
      delay(20);
      io_0.digitalWrite(BYPASS_VALVE, CLOSE); //BYPASS valve close
      io_0.digitalWrite(DIVERTER_VALVE, COOLER);

      delay(50);
      io_0.digitalWrite(SSR , ON); //SSR on
      run_time = 0;
      switch_to_cooler_status = STC_WAIT_FOR_VALVES;
      break;

    case STC_WAIT_FOR_VALVES:
#ifdef debug
      Serial.println("stc wait for valves");
#endif
      run_time ++;
      Serial.println("run time : " + String(run_time));

      if (run_time > RUN_TIME_LIMIT) {
#ifdef debug
        Serial.println("runtime > limit");
#endif


        if (BYPASS_RUN) {
          Serial.println("BYPASS szelephiba, a szelep nem állt le!");
          //info += "BYPASS szelephiba, a szelep nem allt le!\n";
        }
        if (DIVERTER_RUN) {
          Serial.println("DIVERTER szelephiba, a szelep nem állt le!");
          //info += "DIVERTER szelephiba, a szelep nem allt le!\n";
        }

        io_0.digitalWrite(SSR , OFF); //SSR off

        current_state = ERROR_;
#ifdef debug
        Serial.println("current state=stc error");
#endif
        break;
      }

      if (run_time == 3) {
#ifdef debug
        Serial.println("runtime=3");
#endif

        if (!BYPASS_RUN) {
          Serial.println("BYPASS szelephiba, a szelep nem megy!");
          //info += "BYPASS szelephiba, a szelep nem megy!\n";
          current_state = ERROR_;
#ifdef debug
          Serial.println("current state=stc error");
#endif
          break;
        }
        if (!DIVERTER_RUN) {
          Serial.println("diverter szelephiba, a szelep nem megy!");
          //info += "DIVERTER szelephiba, a szelep nem megy!";
          current_state = ERROR_;
#ifdef debug
          Serial.println("current state=stc error");
#endif
          break;
        }
      }

      if ((run_time > 3) & !BYPASS_RUN & !DIVERTER_RUN) {
        Serial.println("Szelepek leálltak, runtime : " + String(run_time));
        io_0.digitalWrite(SSR , OFF); //SSR off

        if (BYPASS_CLOSED & DIVERTER_ON_COOLER) {
          startup_delay = 7;
          switch_to_cooler_status = STC_WAIT_FOR_FLOW;
          //***********************************
#ifdef debug
          Serial.println("current state=stc run on cooler");
#endif
          break;
        }
        else {
#ifdef debug
          Serial.println("else");
#endif

          if (BYPASS_OPEN) {
            //info += "BYPASS szelep beragadt\n";
          }
          if (DIVERTER_ON_WATER) {
            //info += "DIVERTER szelep beragadt\n";
          }
          current_state = ERROR_;
#ifdef debug
          Serial.println("current state=stc error");
#endif
          break;
        }
      }

      if (run_time > RUN_TIME_LIMIT)  {
        io_0.digitalWrite(SSR , OFF); //SSR off
        Serial.println("valami bug van a kódban!");
        //info += "sw bug in STC_WAIT_FOR_BYPASS_CLOSE";
        current_state = ERROR_;
#ifdef debug
        Serial.println("current state=stc error");
#endif
        break;
      }
      break;

    case STC_WAIT_FOR_FLOW:
#ifdef debug
      Serial.println("stc wait for flow " + String(startup_delay));
#endif
      startup_delay--;
      if (startup_delay > 0) {
        break;
      }
      current_state = RUN_ON_COOLER;
      break;
  }
}

#endif
