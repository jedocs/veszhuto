
//no sec flow -> SMS

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
const char * myWriteAPIKey = APIKEY;    // Paste your ThingSpeak Write API Key between the quotes
// Your GPRS credentials (leave empty, if not needed)
const char apn[]      = "internet.telekom"; // APN (example: internet.vodafone.pt) use https://wiki.apnchanger.org internet.vodafone.net
const char gprsUser[] = ""; // GPRS User
const char gprsPass[] = ""; // GPRS Password
// SIM card PIN (leave empty, if not defined)
const char simPIN[]   = SIM_PIN;

const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

volatile int interruptCounter;
unsigned char start_ADC_counter = ADC_READ_THRESHOLD;
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

const unsigned int io_0_debounce_periods[16] = {0, 0, 0, 0, 0, 0, 0, 0, 10, 10, 10, 10, 10, 10, 2000, 2000};
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

float bat_voltage;
float water_flow = 0;
float reset_reason;
float sec_return_temp = 0;
float sec_fwd_temp = 0;
float pri_return_temp = 0;
float pri_fwd_temp = 0;
float room_temp = 0;

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

  WiFi.mode(WIFI_OFF);
  btStop();
  setCpuFrequencyMhz(80);

  pinMode(PB, INPUT_PULLUP);
  pinMode(INTA_7, INPUT);
  pinMode(EOC, INPUT);

  digitalWrite(AN_CS, HIGH);
  pinMode(AN_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
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
    //reset??????????????????????????????!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
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

  io_7.digitalWrite(SSR, LOW); //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
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
  io_7.pullUp(PUMP_RUN, LOW);

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

//  while (1) {
//    if (Serial.available()) {
//      SerialAT.println(Serial.readStringUntil('\n'));
//    }
//    if (SerialAT.available()) {
//      Serial.println(SerialAT.readStringUntil('\n'));
//    }
//  }

  Wire.begin(I2C_SDA, I2C_SCL);
  bool   isOk = setPowerBoostKeepOn(1);
  info = "IP5306 KeepOn " + String((isOk ? "PASS" : "FAIL"));
  SerialMon.println(info);

  //i2c_scan();

  SPI.begin (SCK, MISO, MOSI, AN_CS);
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

//***********************************************************
//*
//*   Main loop
//*
//***********************************************************
void loop()
{
  ReadInputs();

  io_7.digitalWrite(RED_LED, PRI_FLOW_OK);
  io_7.digitalWrite(ORANGE_LED, SEC_FLOW_OK);

  if (interruptCounter > 0) {
    portENTER_CRITICAL(&timerMux);
    interruptCounter--;
    portEXIT_CRITICAL(&timerMux);

    io_7.digitalWrite(GREEN_LED, !io_7.digitalRead(GREEN_LED));

    start_ADC_counter++;
    publish_data_counter++;

    //********************** state machine lockup prevention!!!!!!!!!!!!!!!!!!!!!!!!!!!
    if (takeover_valves) {
      io_0.digitalWrite(MMV_RESET, HIGH); //enable ctrl relay mmv
    }
    else {
      io_0.digitalWrite(MMV_RESET, LOW);
    }

    if (digitalRead(PB)) {
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
//  if (SerialMon.available()) command_interpreter();
}

//*******************************************************************
//*
//*   read inputs, debounce, measure water flow
//*
//*******************************************************************
void ReadInputs(void) {
  unsigned int mask = 0;

  current_io_0 = io_0.readGPIOAB();
  current_io_7 = io_7.readGPIOAB();

  unsigned int changed_io_0 = current_io_0 ^ prev_io_0;
  unsigned int changed_io_7 = current_io_7 ^ prev_io_7;

  if ((millis() - prev_pulse) > 60000) water_flow = 0;

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
                if ((timediff < 60000) & (timediff > 600)) {
                  water_flow = 60000.0 / timediff;
                  Serial.println("flow: " + String(water_flow));
                }
                else water_flow = 0;
                Serial.println("flow: " + String(water_flow));
              }
            }
          }
        }
      }
    }
  }
  prev_io_0 = current_io_0;
  prev_io_7 = current_io_7;
}

//************************************************************************
//*
//*   A/D conversion
//*
//************************************************************************
void ADC() {
  start_ADC_counter = 0;
  digitalWrite(AN_CS, LOW);    // SS is pin 10
  SPI.transfer (0xf8);
  digitalWrite(AN_CS, HIGH);

  if (!digitalRead(EOC)) {
    SerialMon.println("ADC hiba1");

    digitalWrite(AN_CS, LOW);    // SS is pin 10
    SPI.transfer (0x10);  //reset ADC
    digitalWrite(AN_CS, HIGH);
    info += "ADC hiba 1\n";
    return;
  }

  bat_voltage = float(analogRead(VBAT) / 564.634);
  unsigned long thedelay;
  thedelay = micros() + 200;
  while (micros() < thedelay) {
  }
  if (digitalRead(EOC)) {
    SerialMon.println("ADC hiba2");

    digitalWrite(AN_CS, LOW);    // SS is pin 10
    SPI.transfer (0x10);  //reset ADC
    digitalWrite(AN_CS, HIGH);
    info += "ADC hiba 2\n";
    return;
  }

  for (byte i = 0; i < 16; i = i + 1) {
    analog_inputs[i] = read_SPI();
  }

  sec_return_temp = getTemp(analog_inputs[9]);
  sec_fwd_temp = getTemp(analog_inputs[10]);
  pri_return_temp = getTemp(analog_inputs[11]);
  pri_fwd_temp = getTemp(analog_inputs[12]);
  room_temp = getTemp(analog_inputs[6]);
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
  }
}
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
    //reset?????!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  }
  publish_data_counter = 0; //reset publish timer
  struct  publish_data data_to_publish;
  data_to_publish.pri_fwd_temp = pri_fwd_temp;
  data_to_publish.pri_return_temp = room_temp;//
  pri_return_temp;
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
