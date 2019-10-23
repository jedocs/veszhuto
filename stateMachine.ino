//***********************************************************************************************
//*
//*   state machine
//*
//***********************************************************************************************
void State_Machine(void) {
  if (!AC_OK) {
    current_state_bak = current_state;
    //switch_to_cooler_status_bak = switch_to_cooler_status;
    //switch_to_water_status_bak = switch_to_water_status;
    current_state = WAIT_FOR_AC;
  }

  switch (current_state)
  {
    case PRE_STARTUP:
      old_bypass_pos = BYPASS_OPEN;
      old_diverter_pos = DIVERTER_ON_WATER;
      info = "A veszhuto uraindult. RR0.RR1: " + String(reset_reason) + "\n";
      current_state = STARTUP;
      break;

    case STARTUP :
      Serial.println ("starting up " + String(startup_delay));

      startup_delay--;
      if (startup_delay > 0) {
        break;
      }

      if ((old_bypass_pos != BYPASS_OPEN) | (old_diverter_pos != DIVERTER_ON_WATER)) {
        old_bypass_pos = BYPASS_OPEN;
        old_diverter_pos = DIVERTER_ON_WATER;
        startup_delay = 10;
        break;
      }

      io_7.digitalWrite(BYPASS_VALVE, !((io_0_inputs >> BYPASS_POS) & 0x1)); //switch direction relay to actual valve pos
      io_7.digitalWrite(DIVERTER_VALVE, ((io_0_inputs >> DIVERTER_POS) & 0x1)); //switch direction relay to actual valve pos

      //kompresszor megy?
      if (!COMPR_OK) {
        Serial.println ("A kompresszor nem megy!!");
      }

      Serial.println(String((BYPASS_OPEN ? "bypass NYITVA" : "bypass ZÁRVA")));
      Serial.println(String((DIVERTER_ON_COOLER ? "váltószelep VÍZHŰTŐ" : "váltószelep CSAPVÍZ")));

      info += String((BYPASS_OPEN ? "bypass nyitva, " : "bypass zarva, ")) + String((DIVERTER_ON_COOLER ? "valtoszelep vizhuto\n" : "valtoszelep csapviz\n"));

      if (BYPASS_RUN | DIVERTER_RUN) { //szelep hiba
        if (BYPASS_RUN) {
          SerialMon.println("bypass szelep megy pedig nem kéne!");
          info += "bypass szelep megy pedig nem kene!\n";
        }
        if (DIVERTER_RUN) {
          SerialMon.println("diverter szelep megy pedig nem kéne!");
          info += "diverter szelep megy pedig nem kene!\n";
        }
        current_state = ERROR_;
        break;
      }
      // if wm ...

      takeover_valves = true;
      delay(20);
      SerialMon.println("szelepvezérlés átvéve");

      if ((pri_fwd_temp < PRI_FWD_THRESH) & (PRI_FLOW_OK)) {
        SerialMon.println("primer előremenő hőmérséklet OK, " + String(pri_fwd_temp) + "C");
        SerialMon.println("primer átfolyás OK");
        info += "primer eloremeno hom.: " + String(pri_fwd_temp) + "C, atfolyas OK\n";

        if (BYPASS_OPEN) {
          //Serial.println ("bypass open -> close bypass");
          switch_to_cooler_status = STC_CLOSE_BYPASS;
          current_state = SWITCH_TO_COOLER;
          break;
        }
        else {
          if (DIVERTER_ON_WATER) {
            //Serial.println ("bypass closed, div on water -> sw div to cooler");
            switch_to_cooler_status = STC_SWITCH_DIVERTER_TO_COOLER;
            current_state = SWITCH_TO_COOLER;
            break;
          }
          else {
            //Serial.println ("bypass closed, div on cooler -> sw div to water");
            switch_to_cooler_status = STC_SWITCH_DIVERTER;
            current_state = SWITCH_TO_COOLER;
            break;
          }
        }
      }

      else {
        info += "primer eloremeno hom.: " + String(pri_fwd_temp) + "C, atfolyas " + String((PRI_FLOW_OK ? "OK\n" : "NOK\n"));
        if (DIVERTER_ON_COOLER) {
          switch_to_water_status = STW_SWITCH_DIVERTER_TO_WATER;
          current_state = SWITCH_TO_WATER;
          break;
        }
        else {
          if (BYPASS_OPEN) {
            //Serial.println("\n\n*************info**************");
            //Serial.println(info);
            publish_info = info;
            send_SMS = true;
            publish();
            current_state = RUN_ON_WATER;
            break;
          }
          else {
            switch_to_water_status = STW_OPEN_BYPASS;
            current_state = SWITCH_TO_WATER;
            break;
          }
        }
      }
      Serial.println("SW bug");
      info += "sw bug in STARTUP\n";
      current_state = ERROR_;
      break;

    case SWITCH_TO_COOLER:
      switch (switch_to_cooler_status) {
        case STC_CLOSE_BYPASS:    //first close bypass valve to decrease pressure
          //Serial.println ("switch to cooler close bypass");
          //io_7.digitalWrite(SSR , LOW); //SSR off
          delay(20);
          io_7.digitalWrite(BYPASS_VALVE, CLOSE); //BYPASS valve close
          if (DIVERTER_ON_COOLER) {
            wait_for_div = true;
            io_7.digitalWrite(DIVERTER_VALVE, WATER);
          }
          delay(50);
          //io_7.digitalWrite(SSR , HIGH); //SSR on
          run_time = 0;
          switch_to_cooler_status = STC_WAIT_FOR_BYPASS_CLOSE;
          break;

        case STC_WAIT_FOR_BYPASS_CLOSE:
          run_time ++;
          SerialMon.println("BYPASS runtime : " + String(run_time));
          if (wait_for_div) {
            SerialMon.println("diverter runtime : " + String(run_time));
          }

          if ((run_time > RUN_TIME_LIMIT) & BYPASS_RUN) {
            SerialMon.println("BYPASS Szelephiba, a szelep nem állt le!");
            info += "BYPASS szelephiba, a szelep nem allt le!\n";
            //io_7.digitalWrite(SSR , LOW); //SSR off
            current_state = ERROR_;
            break;
          }

          if (wait_for_div & (run_time > RUN_TIME_LIMIT) & DIVERTER_RUN) {
            SerialMon.println("DIVERTER Szelephiba, a szelep nem állt le!");
            //io_7.digitalWrite(SSR , LOW); //SSR off
            info += "DIVERTER szelephiba, a szelep nem allt le!\n";
            current_state = ERROR_;
            break;

          }
          if ((run_time == 3) & !BYPASS_RUN) {
            //io_7.digitalWrite(SSR , LOW); //SSR off
            SerialMon.println("BYPASS Szelephiba, a szelep nem megy!");
            info += "BYPASS Szelephiba, a szelep nem megy!\n";
            current_state = ERROR_;
            break;
          }
          if (wait_for_div & (run_time == 3) & !DIVERTER_RUN) {
            //io_7.digitalWrite(SSR , LOW); //SSR off
            SerialMon.println("diverter Szelephiba, a szelep nem megy!");
            info += "DIVERTER szelephiba, a szelep nem megy!";
            current_state = ERROR_;
            break;
          }

          if ((run_time > 3) & !BYPASS_RUN & !DIVERTER_RUN) {
            SerialMon.println("Szelepek leálltak, runtime : " + String(run_time));
            //io_7.digitalWrite(SSR , LOW); //SSR off

            if (BYPASS_CLOSED & DIVERTER_ON_WATER) {
              switch_to_cooler_status = STC_SWITCH_DIVERTER_TO_COOLER;
              break;
            }
            else {
              if (BYPASS_OPEN) {
                info += "BYPASS szelep beragadt\n";
              }
              if (DIVERTER_ON_COOLER) {
                info += "DIVERTER szelep beragadt\n";
              }
              current_state = ERROR_;
              break;
            }
          }

          if (run_time > RUN_TIME_LIMIT)  {
            SerialMon.println("valami bug van a kódban!");
            //io_7.digitalWrite(SSR , LOW); //SSR off
            break;
          }
          break;

        case STC_SWITCH_DIVERTER:
          //io_7.digitalWrite(SSR , LOW); //SSR off
          delay(20);
          io_7.digitalWrite(DIVERTER_VALVE, WATER);
          delay(50);
          //io_7.digitalWrite(SSR , HIGH); //SSR on
          run_time = 0;
          switch_to_cooler_status = STC_WAIT_FOR_DIVERTER;
          break;

        case STC_WAIT_FOR_DIVERTER:
          run_time ++;
          SerialMon.println("diverter runtime : " + String(run_time));

          if ((run_time > RUN_TIME_LIMIT) & DIVERTER_RUN) {
            //io_7.digitalWrite(SSR , LOW); //SSR off
            info += "DIVERTER szelephiba, a szelep nem allt le!\n";
            current_state = ERROR_;
            break;
          }

          if ((run_time == 3) & !DIVERTER_RUN) {
            //io_7.digitalWrite(SSR , LOW); //SSR off
            info += "DIVERTER szelephiba, a szelep nem megy!";
            current_state = ERROR_;
            break;
          }

          if ((run_time > 3) & !DIVERTER_RUN) {
            SerialMon.println("Szelep leállt, runtime : " + String(run_time));
            //io_7.digitalWrite(SSR , LOW); //SSR off

            if (DIVERTER_ON_WATER) {
              SerialMon.println("diverter on water ");
              info += "valtoszelep atallt csapvizre\n";
              switch_to_cooler_status = STC_SWITCH_DIVERTER_TO_COOLER;
              break;
            }

            //switch_to_cooler_status = SWITCH_DIVERTER_TO_COOLER;
            else {
              info += "valtoszelep beragadt\n";
              current_state = ERROR_;
              break;
            }
          }

          if (run_time > RUN_TIME_LIMIT)  {
            SerialMon.println("valami bug van a kódban! diverter");
            //io_7.digitalWrite(SSR , LOW); //SSR off
            break;
          }
          break;

        case STC_SWITCH_DIVERTER_TO_COOLER:
          //Serial.println ("switch to cooler switch diverter to cooler");
          //io_7.digitalWrite(SSR , LOW); //SSR off
          delay(20);
          io_7.digitalWrite(DIVERTER_VALVE, COOLER); //diverter valve to cooler
          delay(50);
          //io_7.digitalWrite(SSR , HIGH); //SSR on
          run_time = 0;
          switch_to_cooler_status = STC_WAIT_FOR_DIVERTER_COOLER;
          break;

        case STC_WAIT_FOR_DIVERTER_COOLER:
          run_time ++;
          SerialMon.println("diverter runtime : " + String(run_time));

          if ((run_time > RUN_TIME_LIMIT) & DIVERTER_RUN) {
            SerialMon.println("diverter Szelephiba, a szelep nem áll le!");
            //io_7.digitalWrite(SSR , LOW); //SSR off
            info += "valtszelep hiba, a szelep nem allt le!\n";
            current_state = ERROR_;
            break;
          }

          if ((run_time == 3) & !DIVERTER_RUN) {
            //io_7.digitalWrite(SSR , LOW); //SSR off
            SerialMon.println("diverter Szelephiba, a szelep nem megy!");
            info += "valtoszelep hiba, a szelep nem megy!\n";
            current_state = ERROR_;
            break;
          }

          if ((run_time > 3) & !DIVERTER_RUN) {
            SerialMon.println("Szelep leállt, runtime : " + String(run_time));
            //io_7.digitalWrite(SSR , LOW); //SSR off

            if (DIVERTER_ON_COOLER) {
              io_7.digitalWrite(PUMP, PUMP_ON);
              info += "Aktualis uzemmod: vizhuto.\n";
              // send sms
              //Serial.println("*************info**************");
              //Serial.println(info);
              wait_till = long(millis() + PUMP_STARTUP_DELAY * 1000);
              publish_info = info;
              send_SMS = true;
              publish();
              current_state = WAIT_FOR_PUMP;
              break;
            }

            //switch_to_cooler_status = SWITCH_DIVERTER_TO_COOLER;
            else {
              info += "valtoszelep beragadt\n";
              current_state = ERROR_;
              break;
            }
          }

          if (run_time > RUN_TIME_LIMIT)  {
            SerialMon.println("valami bug van a kódban! diverter");
            //io_7.digitalWrite(SSR , LOW); //SSR off
            current_state = ERROR_;
            break;
          }
          SerialMon.println("waiting for diverter valve");
          break;
      }
      break;

    case WAIT_FOR_PUMP:
      //Serial.println("wait for pump");
      if (millis() > wait_till) {
        //Serial.println(String(millis()) + String(wait_till));
        current_state = RUN_ON_COOLER;
        break;
      }
      break;

    case RUN_ON_COOLER:
      if ((pri_fwd_temp > PRI_FWD_THRESH) | (sec_fwd_temp > SEC_FWD_THRESH) | !SEC_FLOW_OK | !PRI_FLOW_OK | !PUMP_OK) { //
        info = "Vizhuto uzemmod hiba! Primer eloremeno hom.: " + String(pri_fwd_temp) + "C, ";
        info += "szekunder eloremeno hom.: " + String(sec_fwd_temp) + "C, ";
        if (!PRI_FLOW_OK) info += "Primer atfolyas NOK\n";
        if (!SEC_FLOW_OK) info += "Szekunder atfolyas NOK\n";
        if (!PUMP_OK) info += "A szivattyú nem megy\n";
        info += "Atallas csapvizre\n";
        io_7.digitalWrite(PUMP, PUMP_OFF);
        switch_to_water_status = STW_SWITCH_DIVERTER_TO_WATER;
        current_state = SWITCH_TO_WATER;
      }
      break;

    case SWITCH_TO_WATER:
      switch (switch_to_water_status) {
        case STW_SWITCH_DIVERTER_TO_WATER:
          //io_7.digitalWrite(SSR , LOW); //SSR off
          delay(20);
          io_7.digitalWrite(DIVERTER_VALVE, WATER); //diverter valve to water
          delay(50);
          //io_7.digitalWrite(SSR , HIGH); //SSR on
          run_time = 0;
          switch_to_water_status = STW_WAIT_FOR_DIVERTER_WATER;
          break;

        case STW_WAIT_FOR_DIVERTER_WATER:
          run_time ++;
          SerialMon.println("runtime: " + String(run_time));

          if ((run_time > RUN_TIME_LIMIT) & DIVERTER_RUN) {
            SerialMon.println("diverter szelephiba, a szelep nem áll le!");
            //io_7.digitalWrite(SSR , LOW); //SSR off
            info += "valtoszelep hiba, a szelep nem allt le!\n";
            current_state = ERROR_;
            break;
          }

          if ((run_time == 3) & !DIVERTER_RUN) {
            //io_7.digitalWrite(SSR , LOW); //SSR off
            SerialMon.println("diverter Szelephiba, a szelep nem megy!");
            info += "valtoszelep hiba, a szelep nem megy!\n";
            current_state = ERROR_;
            break;
          }

          if ((run_time > 3) & !DIVERTER_RUN) {
            SerialMon.println("Szelep leállt, runtime: " + String(run_time));
            //io_7.digitalWrite(SSR , LOW); //SSR off

            if (DIVERTER_ON_WATER) {
              if (BYPASS_OPEN) {
                info += "Aktualis uzemmodd: csapviz.\n";
                //Serial.println("*************info**************");
                //Serial.println(info);
                publish_info = info;
                send_SMS = true;
                publish();
                current_state = RUN_ON_WATER;

                // send sms
                break;
              }
              else {
                switch_to_water_status = STW_OPEN_BYPASS;
                break;
              }
            }
            else {
              info += "valtoszelep beragadt\n";
              current_state = ERROR_;
              break;
            }
          }

          if (run_time > RUN_TIME_LIMIT)  {
            SerialMon.println("valami bug van a kódban! diverter");
            //io_7.digitalWrite(SSR , LOW); //SSR off
            info += "BUG!!!\n";
            current_state = ERROR_;
            break;
          }
          break;

        case STW_OPEN_BYPASS:
          //io_7.digitalWrite(SSR , LOW); //SSR off
          delay(20);
          io_7.digitalWrite(BYPASS_VALVE, OPEN); //BYPASS valve close
          delay(50);
          //io_7.digitalWrite(SSR , HIGH); //SSR on
          run_time = 0;
          switch_to_water_status = STW_WAIT_FOR_BYPASS_OPEN;
          break;

        case STW_WAIT_FOR_BYPASS_OPEN:
          run_time ++;
          SerialMon.println("BYPASS runtime : " + String(run_time));

          if ((run_time > RUN_TIME_LIMIT) & BYPASS_RUN) {
            SerialMon.println("BYPASS Szelephiba, a szelep nem állt le!");
            //io_7.digitalWrite(SSR , LOW); //SSR off
            info += "BYPASS szelephiba, a szelep nem allt le!\n";
            current_state = ERROR_;
            break;
          }

          if ((run_time == 3) & !BYPASS_RUN) {
            //io_7.digitalWrite(SSR , LOW); //SSR off
            SerialMon.println("BYPASS Szelephiba, a szelep nem megy!");
            info += "BYPASS szelephiba, a szelep nem megy!\n";
            current_state = ERROR_;
            break;
          }

          if ((run_time > 3) & !BYPASS_RUN) {
            SerialMon.println("BYPASS Szelep leállt, runtime : " + String(run_time));
            //io_7.digitalWrite(SSR , LOW); //SSR off
            if (BYPASS_OPEN) {
              info += "Aktualis uzemmod: csapviz.\n";
              //Serial.println("*************info**************");
              //Serial.println(info);
              publish_info = info;
              send_SMS = true;
              publish();
              current_state = RUN_ON_WATER;

              // send sms
              break;
            }
            else {
              info += "BYPASS szelep beragadt\n";
              current_state = ERROR_;
              break;
            }
          }

          if (run_time > RUN_TIME_LIMIT)  {
            SerialMon.println("valami bug van a kódban!");
            //io_7.digitalWrite(SSR , LOW); //SSR off
            info += "BUG!!!\n";
            current_state = ERROR_;
            break;
          }
          break;
      }
      break;

    case RUN_ON_WATER:
      //SerialMon.println("running on water");
      break;

    case ERROR_:
      Serial.println("**************ERROR******************");
      Serial.println(info);
      publish_info = info;
      send_SMS = true;
      publish();
      current_state = ERROR_WAIT;
      break;

    case ERROR_WAIT:
      break;

    case WAIT_FOR_AC:

      break;
    default :
      Serial.println ("switch - case hiba");
  }
}
