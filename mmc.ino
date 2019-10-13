#include "TinyGsmClientSIM800.h"
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "Adafruit_MCP23017.h"
#include <SPI.h>
#include "FS.h"
#include "SD.h"
#include "mmc.h"
#include "pwds.h"

#include <rom/rtc.h>
#include "ThingSpeak.h"

#define TINY_GSM_MODEM_HAS_GPRS
#define TINY_GSM_MODEM_HAS_SSL

typedef TinyGsmSim800 TinyGsm;
typedef TinyGsmSim800::GsmClient TinyGsmClient;
typedef TinyGsmSim800::GsmClientSecure TinyGsmClientSecure;

unsigned long myChannelNumber = 0;  // Replace the 0 with your channel number
const char * myWriteAPIKey = "8RBBFPUK3ON752QV";    // Paste your ThingSpeak Write API Key between the quotes
// Your GPRS credentials (leave empty, if not needed)
const char apn[]      = "internet.vodafone.net"; // APN (example: internet.vodafone.pt) use https://wiki.apnchanger.org
const char gprsUser[] = ""; // GPRS User
const char gprsPass[] = ""; // GPRS Password
// SIM card PIN (leave empty, if not defined)
const char simPIN[]   = "";

const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

volatile int interruptCounter;
unsigned char start_ADC_counter = 0;
unsigned char publish_data_counter = 0;
unsigned char current_state = PRE_STARTUP;
unsigned char switch_to_cooler_status = 0;
unsigned char switch_to_water_status = 0;
unsigned char current_state_bak = 0;
unsigned char switch_to_cooler_status_bak = 0;
unsigned char switch_to_water_status_bak = 0;
unsigned char run_time = 0;

int startup_delay = STARTUP_DELAY;

hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
//portMUX_TYPE IOMux = portMUX_INITIALIZER_UNLOCKED;

bool old_bypass_pos;
bool old_diverter_pos;
bool wait_for_div = false;
bool takeover_valves = false;
bool WD_state = false;
bool pri_flow = false;
bool sec_flow = false;
bool rr_published = false;
bool SMS_OK = true;
bool BYPASS_STUCK = false;
bool DIVERTER_STUCK = false;
bool send_SMS = false;

int ADC_timer = 0;

unsigned char WMIntCounter = 0;
unsigned int current_io_0 = 0;
unsigned int current_io_7 = 0;
unsigned int prev_io_0 = 0;
unsigned int prev_io_7 = 0;
unsigned int io_0_inputs = 0;
unsigned int io_7_inputs = 0;
unsigned int analog_inputs[16];
float bat_voltage;
const unsigned int io_0_debounce_periods[16] = {0, 0, 0, 0, 0, 0, 0, 0, 10, 10, 10, 10, 10, 10, 2000, 10000};
const unsigned int io_7_debounce_periods[16] = {0, 0, 0, 0, 0, 0, 0, 0, 10, 10, 10, 10, 10, 10, 10, 10000};

long unsigned int io_0_lastchange[16];
long unsigned int io_7_lastchange[16];
long long unsigned int prev_WM_pulse = 0;
long wait_till;
long prev_pulse;

double A = 0.0007196771796959102;
double B = 0.00027785289284365956;
double C = 9.627280482149224e-8;

String publish_info;
String info;
String command;

float water_flow = 0;
float reset_reason;
float sec_return_temp = 0;
float sec_fwd_temp = 0;
float pri_return_temp = 0;
float pri_fwd_temp = 0;

struct publish_data {
  float pri_fwd_temp;
  float pri_return_temp;
  float sec_fwd_temp;
  float sec_return_temp;
  float water_meter;
  float vbat;
  float reset_reason;
  int status_code;
};

QueueHandle_t queue;

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

Adafruit_MCP23017 io_0;
Adafruit_MCP23017 io_7;
TinyGsmClient client(modem);
//WiFiClient client;

void TaskGSM( void *pvParameters );

void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter++;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void setup()
{
  int rr0 = rtc_get_reset_reason(0);
  int rr1 = rtc_get_reset_reason(1);
  reset_reason = rr0 + float(rr1) / 100;

  pinMode(PB, INPUT_PULLUP);
  pinMode(INTA_7, INPUT);
  pinMode(EOC, INPUT);
  digitalWrite(AN_CS, HIGH);  // ensure SS stays high
  pinMode(AN_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);  // ensure SS stays high
  pinMode(SD_CS, OUTPUT);

  pinMode(BUZZER, OUTPUT);
  pinMode(WD, OUTPUT);
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);

  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  SerialMon.begin(115200);
  SerialMon.println("setup begin");
  Serial.println("CPU0 reset reason: ");
  Serial.println(rr0);
  Serial.println("CPU1 reset reason: ");
  Serial.println(rr1);

  queue = xQueueCreate( 5, sizeof( struct publish_data ) );

  if (queue == NULL) {
    SerialMon.println("Error creating the queue");
  }
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000000, true);
  timerAlarmEnable(timer);

  io_0.begin();
  io_7.begin(7);

  // We mirror INTA and INTB, so that only one line is required between MCP and Arduino for int reporting
  // The INTA/B will not be Floating
  // INTs will be signaled with a LOW
  io_0.setupInterrupts(true, false, LOW);
  io_7.setupInterrupts(true, false, LOW);

  io_0.pinMode(SELFTEST_OUT, OUTPUT);

  io_0.digitalWrite(CTRL_PULSE, HIGH);
  io_0.pinMode(CTRL_PULSE, OUTPUT);

  io_0.digitalWrite(MMV_RESET, LOW);
  io_0.pinMode(MMV_RESET, OUTPUT);

  io_0.pinMode(OPTO, INPUT);
  io_0.pullUp(OPTO, HIGH);

  io_0.pinMode(PB3, INPUT);
  io_0.pullUp(PB3, HIGH);

  io_0.pinMode(DIVERTER_POS, INPUT);
  io_0.pullUp(DIVERTER_POS, HIGH);

  io_0.pinMode(BYPASS_POS, INPUT);
  io_0.pullUp(BYPASS_POS, HIGH);

  io_0.pinMode(PRI_FLOW, INPUT);
  io_0.pullUp(PRI_FLOW, HIGH);

  io_0.pinMode(SEC_FLOW, INPUT);
  io_0.pullUp(SEC_FLOW, HIGH);

  io_7.digitalWrite(SSR, HIGH); //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  io_7.pinMode(SSR , OUTPUT);

  io_7.digitalWrite(PUMP , HIGH);
  io_7.pinMode(PUMP , OUTPUT);

  io_7.digitalWrite(DIVERTER_VALVE, HIGH);
  io_7.pinMode(DIVERTER_VALVE , OUTPUT);

  io_7.digitalWrite(BYPASS_VALVE, HIGH);
  io_7.pinMode(BYPASS_VALVE , OUTPUT);

  io_7.digitalWrite(CTRL_RELAY, HIGH);
  io_7.pinMode(CTRL_RELAY , OUTPUT);

  io_7.pinMode(RED_LED , OUTPUT);
  io_7.pinMode(ORANGE_LED , OUTPUT);
  io_7.pinMode(GREEN_LED , OUTPUT);

  io_7.pinMode(PUMP_RUN, INPUT);
  io_7.pullUp(PUMP_RUN, HIGH);

  io_7.pinMode(BYPASS_VALVE_RUN, INPUT);
  io_7.pullUp(BYPASS_VALVE_RUN, HIGH);

  io_7.pinMode(DIVERTER_VALVE_RUN, INPUT);
  io_7.pullUp(DIVERTER_VALVE_RUN, HIGH);

  io_7.pinMode(AC_MONITOR, INPUT);
  io_7.pullUp(AC_MONITOR, HIGH);

  io_7.pinMode(LIMIT_SW, INPUT);
  io_7.pullUp(LIMIT_SW, HIGH);

  io_7.pinMode(WATER_PULSE, INPUT);
  io_7.pullUp(WATER_PULSE, HIGH);
  io_7.setupInterruptPin(WATER_PULSE, FALLING);

  //***********************************************************
  //*
  //*   create GSM task on core 0
  //*
  //***********************************************************

  xTaskCreatePinnedToCore(
    TaskGSM
    ,  "TaskGSM"   // A name just for humans
    ,  4096  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  0  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  NULL
    ,  0); //core

  //*********************************************************
  Wire.begin(I2C_SDA, I2C_SCL);
  bool   isOk = setPowerBoostKeepOn(1);
  info = "IP5306 KeepOn " + String((isOk ? "PASS" : "FAIL"));
  SerialMon.println(info);

  SPI.begin (SCK, MISO, MOSI, AN_CS);
  /*
    SerialMon.println(String(sizeof(long)));
    SerialMon.println(String(sizeof(long long)));
    while(1){}*/
  /*
    if (!SD.begin(SD_CS)) {
    SerialMon.println("Card Mount Failed");
    //return;
    }
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE) {
    SerialMon.println("No SD card attached");
    //return;
    }

    SerialMon.print("SD Card Type: ");
    if (cardType == CARD_MMC) {
    SerialMon.println("MMC");
    } else if (cardType == CARD_SD) {
    SerialMon.println("SDSC");
    } else if (cardType == CARD_SDHC) {
    SerialMon.println("SDHC");
    } else {
    SerialMon.println("UNKNOWN");
    }
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    SerialMon.printf("SD Card Size: %lluMB\n", cardSize);

    listDir(SD, "/", 0);
    SerialMon.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
    SerialMon.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));

    writeFile(SD, "/temp_log.txt", "temperature log\n");
  */

}
/*
  void TaskMain(void *pvParameters){
  while(1){
    delay(1500);
    Serial.println("main task alive on core "+String(xPortGetCoreID())+"\n");
    }
  }
*/

//***********************************************************
//*
//*   publish data to thingspeak
//*
//***********************************************************
void publish()
{
  // if (send_SMS) {
  // Serial.println("\n\n!!!!!!!!!!!!!! send SMS !!!!!!!!!!!!!!");
  // Serial.println(publish_info);
  //}
  SerialMon.println("send data to queue");
  if (queue == NULL) {
    SerialMon.println("queue is NULL in main loop");
    //reset?????
  }
  publish_data_counter = 0;
  struct  publish_data data_to_publish;
  data_to_publish.pri_fwd_temp = pri_fwd_temp;
  data_to_publish.pri_return_temp = pri_return_temp;
  data_to_publish.sec_fwd_temp = sec_fwd_temp;
  data_to_publish.sec_return_temp = sec_return_temp;
  data_to_publish.water_meter = water_flow;
  data_to_publish.reset_reason = reset_reason;
  data_to_publish.vbat = bat_voltage;
  int status_code;
  bitWrite(status_code, 0, SMS_OK);
  bitWrite(status_code, 1, BYPASS_STUCK);
  bitWrite(status_code, 2, DIVERTER_STUCK);
  bitWrite(status_code, 3, AC_OK);
  bitWrite(status_code, 4, BYPASS_CLOSED);
  bitWrite(status_code, 5, DIVERTER_ON_COOLER);
  bitWrite(status_code, 6, PRI_FLOW_OK);
  bitWrite(status_code, 7, SEC_FLOW_OK);
  bitWrite(status_code, 8, PUMP_OK);
  bitWrite(status_code, 9, COMPR_OK);
  //Serial.println("status_code" + String(status_code));
  data_to_publish.status_code = status_code;

  xQueueSend(queue, &data_to_publish, 0);
}

//***********************************************************
//*
//*   Main loop
//*
//***********************************************************
void loop()
{
  ReadInputs();

  io_7.digitalWrite(RED_LED, DIVERTER_ON_WATER);
  io_7.digitalWrite(GREEN_LED, BYPASS_OPEN);

  if (interruptCounter > 0) {
    portENTER_CRITICAL(&timerMux);
    interruptCounter--;
    portEXIT_CRITICAL(&timerMux);

    start_ADC_counter++;
    publish_data_counter++;

    
    
    //********************** state machine lockup prevention!!!!!!!!!!!!!!!!!!!!!!!!!!!
    if (takeover_valves) {
      io_0.digitalWrite(MMV_RESET, HIGH); //enable ctrl relay mmv
    }
    else {
      io_0.digitalWrite(MMV_RESET, LOW);
    }

    if (!digitalRead(PB)) {
      publish_info = "gombnyomás";
      send_SMS = true;
    }
    if (1) { //digitalRead(PB)) {
      if (WD_state) {
        digitalWrite(WD, LOW);
        WD_state = false;
        if (takeover_valves) {
          io_0.digitalWrite(CTRL_PULSE, LOW);
        }
      }
      else {
        digitalWrite(WD, HIGH);
        WD_state = true;
        if (takeover_valves) {
          io_0.digitalWrite(CTRL_PULSE, HIGH);
        }
      }
    }
    State_Machine();
  }

  if (start_ADC_counter > ADC_READ_THRESHOLD) ADC();
  if (publish_data_counter > PUBLISH_DATA_THRESHOLD) publish();
  if (SerialMon.available()) command_interpreter();
}

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
      info = "A vészhűtő újraindult. RR0.RR1: " + String(reset_reason) + "\n";
      //current_state = STARTUP;
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
      Serial.println(String((DIVERTER_ON_COOLER ? "váltószelep VÍZHŰTŐ" : "váltószelep CSPVÍZ")));

      info += String((BYPASS_OPEN ? "bypass nyitva, " : "bypass zárva, ")) + String((DIVERTER_ON_COOLER ? "váltószelep víthűtő\n" : "váltószelep csapvíz\n"));

      if (BYPASS_RUN | DIVERTER_RUN) { //szelep hiba
        SerialMon.println("szelep megy pedig nem kéne!");
        info += "szelep megy pedig nem kéne!\n";
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
        info += "primer előremenő hőm.: " + String(pri_fwd_temp) + "C, átfolyás OK\n";

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
        info += "primer előremenő hőm.: " + String(pri_fwd_temp) + "C, átfolyás " + String((PRI_FLOW_OK ? "OK\n" : "NOK\n"));
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
            info += "BYPASS szelephiba, a szelep nem állt le!\n";
            //io_7.digitalWrite(SSR , LOW); //SSR off
            current_state = ERROR_;
            break;
          }

          if (wait_for_div & (run_time > RUN_TIME_LIMIT) & DIVERTER_RUN) {
            SerialMon.println("DIVERTER Szelephiba, a szelep nem állt le!");
            //io_7.digitalWrite(SSR , LOW); //SSR off
            info += "DIVERTER szelephiba, a szelep nem állt le!\n";
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
            info += "DIVERTER szelephiba, a szelep nem állt le!\n";
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
              info += "váltószelep átállt csapvízre\n";
              switch_to_cooler_status = STC_SWITCH_DIVERTER_TO_COOLER;
              break;
            }

            //switch_to_cooler_status = SWITCH_DIVERTER_TO_COOLER;
            else {
              info += "váltó szelep beragadt\n";
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
            info += "váltószelep hiba, a szelep nem állt le!\n";
            current_state = ERROR_;
            break;
          }

          if ((run_time == 3) & !DIVERTER_RUN) {
            //io_7.digitalWrite(SSR , LOW); //SSR off
            SerialMon.println("diverter Szelephiba, a szelep nem megy!");
            info += "váltószelep hiba, a szelep nem megy!\n";
            current_state = ERROR_;
            break;
          }

          if ((run_time > 3) & !DIVERTER_RUN) {
            SerialMon.println("Szelep leállt, runtime : " + String(run_time));
            //io_7.digitalWrite(SSR , LOW); //SSR off

            if (DIVERTER_ON_COOLER) {
              io_7.digitalWrite(PUMP, PUMP_ON);
              info += "Aktuális üzemmód: vízhűtő.\n";
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
              info += "váltó szelep beragadt\n";
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
      if ((pri_fwd_temp > PRI_FWD_THRESH) | (sec_fwd_temp > SEC_FWD_THRESH) | !SEC_FLOW_OK | !PUMP_OK | !PRI_FLOW_OK) {
        info = "Vízhűtő üzemmód hiba! Primer előremenő hőm.: " + String(pri_fwd_temp) + "C, ";
        info += "szekunder előremenő hőm.: " + String(sec_fwd_temp) + "C, ";
        if (!PRI_FLOW_OK) info += "Primer átfolyás NOK\n";
        if (!SEC_FLOW_OK) info += "Szekunder átfolyás NOK\n";
        if (!PUMP_OK) info += "A szivattyú nem megy\n";
        info += "Átállás csapvízre\n";
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
            info += "váltószelep hiba, a szelep nem állt le!\n";
            current_state = ERROR_;
            break;
          }

          if ((run_time == 3) & !DIVERTER_RUN) {
            //io_7.digitalWrite(SSR , LOW); //SSR off
            SerialMon.println("diverter Szelephiba, a szelep nem megy!");
            info += "váltószelep hiba, a szelep nem megy!\n";
            current_state = ERROR_;
            break;
          }

          if ((run_time > 3) & !DIVERTER_RUN) {
            SerialMon.println("Szelep leállt, runtime: " + String(run_time));
            //io_7.digitalWrite(SSR , LOW); //SSR off

            if (DIVERTER_ON_WATER) {
              if (BYPASS_OPEN) {
                info += "Aktuális üzemmód: csapvíz.\n";
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
              info += "váltószelep beragadt\n";
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
            info += "BYPASS szelephiba, a szelep nem állt le!\n";
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
              info += "Aktuális üzemmód: csapvíz.\n";
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

//***********************************************************************************************
//*
//*   read inputs, debounce, measure water flow
//*
//***********************************************************************************************
void ReadInputs(void) {
  unsigned int mask = 0;

  current_io_0 = io_0.readGPIOAB();
  current_io_7 = io_7.readGPIOAB();

  unsigned int changed_io_0 = current_io_0 ^ prev_io_0;
  unsigned int changed_io_7 = current_io_7 ^ prev_io_7;

  for (byte i = 0; i < 16; i = i + 1) {
    mask = 1 << i;

    if (io_0_debounce_periods[i] != 0) {
      if (changed_io_0 & mask) {
        io_0_lastchange[i] = millis();
      }

      else {
        if ((current_io_0 &  mask) != (io_0_inputs &  mask)) {
          if ((millis() - io_0_lastchange[i]) > io_0_debounce_periods[i]) {
            bitWrite(io_0_inputs, i, (current_io_0 &  mask));
            //Serial.println("io0." + String(i) + " changed to " + String((io_0_inputs &  mask) >> i) + "\n");
          }
        }
      }
    }

    if (io_7_debounce_periods[i] != 0) {
      if (changed_io_7 & mask) {
        io_7_lastchange[i] = millis();
      }

      else {
        if ((current_io_7 &  mask) != (io_7_inputs &  mask)) {
          if ((millis() - io_7_lastchange[i]) > io_7_debounce_periods[i]) {
            bitWrite(io_7_inputs, i, (current_io_7 &  mask));
            //Serial.println("io7." + String(i) + " changed to " + String((io_7_inputs &  mask) >> i) + "\n");
            if (i == WATER_PULSE) {
              long timestamp = millis();
              if ((io_7_inputs >> WATER_PULSE) & 1) { //water meter pulse detected
                long timediff = timestamp - prev_pulse;
                prev_pulse = timestamp;
                if ((timediff < 60000) & timediff > 600) {
                  water_flow = 60000.0 / timediff;
                  Serial.println("flow: " + String(water_flow));
                }
                else water_flow = 0;
              }
              else if ((timestamp - prev_pulse) > 60000) water_flow = 0;
            }
          }
        }
      }
    }
  }
  prev_io_0 = current_io_0;
  prev_io_7 = current_io_7;
}
//***********************************************************************************************
//*
//*   GSM task
//*
//***********************************************************************************************
void TaskGSM(void *pvParameters) {
  struct  publish_data data_for_publish;

  // Set GSM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  SerialMon.println("Initializing modem...");
  modem.restart();

  // Unlock your SIM card with a PIN if needed
  //if (strlen(simPIN) && modem.getSimStatus() != 3 ) {
  // modem.simUnlock(simPIN);
  //}




  /*
    WiFi.begin(ssid, password);
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
    if (Serial.available()) {
      SerialAT.println(Serial.readStringUntil('\n'));
    }
    if (SerialAT.available()) {
      Serial.println(SerialAT.readStringUntil('\n'));
    }
    if (xQueueReceive(queue, &data_for_publish, 0)) {
      //Serial.println("   time: " + modem.getGSMDateTime(DATE_TIME));
      /* if (send_SMS) {
         Serial.println("sending sms from gsm task");
         if (modem.sendSMS(SMS_TARGET, publish_info)) {
           SerialMon.println(publish_info);
           send_SMS = false;
         }
         else {
           SerialMon.println("SMS failed to send");
           send_SMS = false;
         }
        }*/

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
    }

    delay(1000);
    //Serial.println("GSM task alive on core " + String(xPortGetCoreID()) + "\n");
  }
}
//***********************************************************************************************
//*
//*   A/D conversion
//*
//***********************************************************************************************
void ADC() {
  start_ADC_counter = 0;
  digitalWrite(AN_CS, LOW);    // SS is pin 10
  SPI.transfer (0xf8);
  digitalWrite(AN_CS, HIGH);

  if (!digitalRead(EOC)) {
    SerialMon.println("ADC hiba1");
  }

  bat_voltage = float(analogRead(VBAT)/564.634);
  unsigned long thedelay;
  thedelay = micros() + 140;
  while (micros() < thedelay) {
  }
  if (digitalRead(EOC)) {
    SerialMon.println("ADC hiba2");
  }

  for (byte i = 0; i < 16; i = i + 1) {
    analog_inputs[i] = read_SPI();
  }

  sec_return_temp = getTemp(analog_inputs[9]);
  sec_fwd_temp = getTemp(analog_inputs[10]);
  pri_return_temp = getTemp(analog_inputs[11]);
  pri_fwd_temp = getTemp(analog_inputs[12]);

  //info = "sec1: " + String(sec_fwd_temp) + ", ";
  //info += "pri1: " + String(pri_fwd_temp) + "\n";

  //SerialMon.println("sec1: " + String(sec_fwd_temp) + ", pri1: " + String(pri_fwd_temp) + "\n");

  //appendFile(SD, "/temp_log.txt", info.c_str());
}
//***********************************************************************************************
//*
//*   command interpreter
//*
//***********************************************************************************************
void command_interpreter() {
  bool temp;
  command = SerialMon.readStringUntil('\n');
  // SerialAT.println(command);
  //Serial.println("cmd  " +command);
  /*
    if (command.equals("pump")) {

    temp = io_7.digitalRead(PUMP);
    io_7.digitalWrite(PUMP, !temp);
    }

    else if (command.equals("2w")) {
    //io_7.digitalWrite(SSR, HIGH);
    delay(300);
    temp = io_7.digitalRead(BYPASS_VALVE);
    SerialMon.println("BYPASS " + String(temp));
    io_7.digitalWrite(BYPASS_VALVE, !temp);
    delay(100);
    //io_7.digitalWrite(SSR, HIGH);

    }

    else if (command.equals("3w")) {

    temp = io_7.digitalRead(DIVERTER_VALVE);
    SerialMon.println("3W " + String(temp));
    io_7.digitalWrite(DIVERTER_VALVE, !temp);
    }

    else if (command.equals("ssr")) {

    temp = io_7.digitalRead(SSR);
    SerialMon.println("SSR " + String(temp));
    io_7.digitalWrite(SSR, !temp);
    }

    else if (command.equals("ctrl")) {

    temp = io_0.digitalRead(CTRL_PULSE);
    SerialMon.println("ctrl relay " + String(temp));
    takeover_valves = !takeover_valves;
    }

    else if (command.equals("mmv")) {

    temp = io_0.digitalRead(MMV_RESET);
    SerialMon.println("ctrl relay " + String(temp));
    io_0.digitalWrite(MMV_RESET, !temp);
    }

    else {
    SerialMon.println("Invalid command");
    }*/
}
//***********************************************************************************************
//*
//*   list dir
//*
//***********************************************************************************************
void listDir(fs::FS & fs, const char * dirname, uint8_t levels) {
  SerialMon.printf("Listing directory: % s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    SerialMon.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    SerialMon.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      SerialMon.print("  DIR : ");
      SerialMon.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      SerialMon.print("  FILE: ");
      SerialMon.print(file.name());
      SerialMon.print("  SIZE: ");
      SerialMon.println(file.size());
    }
    file = root.openNextFile();
  }
}
//***********************************************************************************************
//*
//*   create dir
//*
//***********************************************************************************************
void createDir(fs::FS & fs, const char * path) {
  SerialMon.printf("Creating Dir: % s\n", path);
  if (fs.mkdir(path)) {
    SerialMon.println("Dir created");
  } else {
    SerialMon.println("mkdir failed");
  }
}
//***********************************************************************************************
//*
//*   remove dir
//*
//***********************************************************************************************
void removeDir(fs::FS & fs, const char * path) {
  SerialMon.printf("Removing Dir: % s\n", path);
  if (fs.rmdir(path)) {
    SerialMon.println("Dir removed");
  } else {
    SerialMon.println("rmdir failed");
  }
}
//***********************************************************************************************
//*
//*   read file
//*
//***********************************************************************************************
void readFile(fs::FS & fs, const char * path) {
  SerialMon.printf("Reading file: % s\n", path);

  File file = fs.open(path);
  if (!file) {
    SerialMon.println("Failed to open file for reading");
    return;
  }

  SerialMon.print("Read from file : ");
  while (file.available()) {
    SerialMon.write(file.read());
  }
  file.close();
}
//***********************************************************************************************
//*
//*   write file
//*
//***********************************************************************************************
void writeFile(fs::FS & fs, const char * path, const char * message) {
  SerialMon.printf("Writing file : % s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    SerialMon.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    SerialMon.println("File written");
  } else {
    SerialMon.println("Write failed");
  }
  file.close();
}
//***********************************************************************************************
//*
//*   append file
//*
//***********************************************************************************************
void appendFile(fs::FS & fs, const char * path, const char * message) {
  SerialMon.printf("Appending to file : % s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    SerialMon.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    SerialMon.println("Message appended");
  } else {
    SerialMon.println("Append failed");
  }
  file.close();
}
//***********************************************************************************************
//*
//*   rename file
//*
//***********************************************************************************************
void renameFile(fs::FS & fs, const char * path1, const char * path2) {
  SerialMon.printf("Renaming file % s to % s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    SerialMon.println("File renamed");
  } else {
    SerialMon.println("Rename failed");
  }
}
//***********************************************************************************************
//*
//*   delete file
//*
//***********************************************************************************************
void deleteFile(fs::FS & fs, const char * path) {
  SerialMon.printf("Deleting file : % s\n", path);
  if (fs.remove(path)) {
    SerialMon.println("File deleted");
  } else {
    SerialMon.println("Delete failed");
  }
}
//***********************************************************************************************
//*
//*   get temperature
//*
//***********************************************************************************************
double getTemp(int val) {

  float Vout = 2.5 * ((float)(val / 4096.0));
  double res = (10000 * Vout / (3.3 - Vout));

  return (1.0 / (A + B * (double)log(res) + C * (double)cb((double)log(res)))) - 273.15; //natural log in arduino
  //Use the steinhart-Hart equation with the resistance from analog input
}
//***********************************************************************************************
//*
//*
//*
//***********************************************************************************************
double cb(double val) {
  return val * val * val;
  // cube function, cuz why not
}
//***********************************************************************************************
//*
//*   read a/d results
//*
//***********************************************************************************************
unsigned int read_SPI() {
  byte inByte = 0;           // incoming byte from the SPI
  unsigned int result = 0;   // result to return

  digitalWrite(AN_CS, LOW);
  result = SPI.transfer(0x00);
  result = result << 8;
  inByte = SPI.transfer(0x00);
  result = result | inByte;
  digitalWrite(AN_CS, HIGH);

  return (result);
}
//***********************************************************************************************
//*
//*   I2C scan
//*
//***********************************************************************************************
void i2c_scan()
{
  byte error, address;
  int nDevices;

  SerialMon.println("Scanning...");

  nDevices = 0;
  for (address = 1; address < 127; address++ )
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      SerialMon.print("I2C device found at address 0x");
      if (address < 16)
        SerialMon.print("0");
      SerialMon.print(address, HEX);
      SerialMon.println("  !");

      nDevices++;
    }
    else if (error == 4)
    {
      SerialMon.print("Unknown error at address 0x");
      if (address < 16)
        SerialMon.print("0");
      SerialMon.println(address, HEX);
    }
  }
  if (nDevices == 0)
    SerialMon.println("No I2C devices found\n");
  else
    SerialMon.println("done\n");

  delay(5000);           // wait 5 seconds for next scan
}
//***********************************************************************************************
//*
//*   power keep on
//*
//***********************************************************************************************
bool setPowerBoostKeepOn(int en)
{
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  if (en)
    Wire.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
  else
    Wire.write(0x35); // 0x37 is default reg value
  return Wire.endTransmission() == 0;
}
