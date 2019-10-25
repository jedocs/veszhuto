//SIM800
// #define UART_TX           	27
// #define UART_RX           	26
// #define SIMCARD_RST         5
// #define SIMCARD_PWKEY       4
// #define SIM800_POWER_ON  	23

// TTGO T-Call pins
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26

//ESP32
#define INTA_0              36
#define INTA_7              34
#define WD                  33
#define SD_CS               32
#define EOC                 25
#define AN_CS               19
#define ETH_CS              18
#define PB                  15
#define SCK 				        14
#define MISO 				        12
#define MOSI 				        13
#define CD                  2
#define BUZZER              0
#define ETH_INT             39
#define I2C_SDA             21
#define I2C_SCL             22
#define VBAT                35

//IO_EXPANDER addr 0
//PORT A
#define SELFTEST_OUT 		0
#define JST1  				1
#define CTRL_PULSE 				2
#define MMV_RESET 				3
#define JST4 				4
#define JST5 				5
#define JST6 				6
#define JST7 				7

//PORT B
//#define NC 				8
//#define NC1 				9
#define OPTO 				10
#define PB3 				11
#define BYPASS_POS 				12
#define DIVERTER_POS 				13
#define SEC_FLOW 			14
#define PRI_FLOW 			15

//IO_EXPANDER addr 7
//PORT A
#define PUMP 			0
#define DIVERTER_VALVE 				1
#define BYPASS_VALVE 				2
#define CTRL_RELAY 				3
#define GREEN_LED 			4
#define ORANGE_LED 			5
#define RED_LED 			6
#define SSR 			7

//PORT B
#define SELFTEST_IN 		8
#define LIMIT_SW 			9
#define WATER_PULSE 		10
#define COMPR_RUN 			11
#define PUMP_RUN 			12
#define BYPASS_VALVE_RUN 			13
#define DIVERTER_VALVE_RUN 			14
#define AC_MONITOR 				15

//analog inputs
#define SEC_FLOW_AN 		0
#define SEC_FLOW_TEMP 		1
#define PRI_FLOW_TEMP 		2
#define PRI_FLOW_AN 		3
#define HE_TEMP_2 			4
#define HE_TEMP_1 			5
#define ROOM_TEMP 			6
#define SEC_PRESS 			7
#define PRI_PRESS 			8
#define SEC_TEMP_2  		9
#define SEC_TEMP_1  		10
#define PRI_TEMP_2 			11
#define PRI_TEMP_1  		12
#define PSUP_5V 			13
#define GND 				14
#define SELFTEST  			15

#define UART_BANUD_RATE     9600

#define IP5306_ADDR         0X75
#define IP5306_REG_SYS_CTL0 0x00

#define OLED_ADDR			0x3C

//#define DUMP_AT_COMMANDS



#define TINY_GSM_MODEM_SIM800      // Modem is SIM800
#define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb

#define ADC_READ_THRESHOLD 5
#define PUBLISH_DATA_THRESHOLD 60


// Set serial for debug console (to Serial Monitor, default speed 115200)
#define SerialMon Serial
// Set serial for AT commands (to SIM800 module)
#define SerialAT  Serial1

#define SMS

void listDir(fs::FS &fs, const char * dirname, uint8_t levels);
void createDir(fs::FS &fs, const char * path);
void removeDir(fs::FS &fs, const char * path);
void readFile(fs::FS &fs, const char * path);
void writeFile(fs::FS &fs, const char * path, const char * message);
void appendFile(fs::FS &fs, const char * path, const char * message);
void renameFile(fs::FS &fs, const char * path1, const char * path2);
void deleteFile(fs::FS &fs, const char * path);
void testFileIO(fs::FS &fs, const char * path);
void ReadInputs(void);
void State_Machine(void);
void command_interpreter(void);
void ADC(void);

double getTemp(int val);
double cb(double val);
bool setPowerBoostKeepOn(int en);

unsigned int read_SPI();
void i2c_scan();
#define RUN_TIME_LIMIT 20

#define STARTUP_DELAY   5 //*********************************************
#define PUMP_STARTUP_DELAY  6

//state machine
#define PRE_STARTUP 0
#define STARTUP   1
#define SWITCH_TO_COOLER  2
#define RUN_ON_COOLER 3
#define SWITCH_TO_WATER 4
#define WAIT_FOR_PUMP 5
#define RUN_ON_WATER 6
#define ERROR_  7
#define ERROR_WAIT 8
#define WAIT_FOR_AC 9

#define STC_CLOSE_BYPASS    0
#define STC_WAIT_FOR_BYPASS_CLOSE   1
#define STC_SWITCH_DIVERTER   2
#define STC_WAIT_FOR_DIVERTER 3
#define STC_SWITCH_DIVERTER_TO_COOLER   4
#define STC_WAIT_FOR_DIVERTER_COOLER  5

#define STW_SWITCH_DIVERTER_TO_WATER 0
#define STW_WAIT_FOR_DIVERTER_WATER 1
#define STW_OPEN_BYPASS    2
#define STW_WAIT_FOR_BYPASS_OPEN  3

#define PRI_FWD_THRESH  15
#define SEC_FWD_THRESH  19
#define CLOSE HIGH
#define OPEN LOW
#define COOLER HIGH
#define WATER LOW
#define PUMP_ON LOW
#define PUMP_OFF HIGH

#define BYPASS_CLOSED !((io_0_inputs >> BYPASS_POS) & 0x1)
#define BYPASS_OPEN ((io_0_inputs >> BYPASS_POS) & 0x1)

#define DIVERTER_ON_WATER !((io_0_inputs >> DIVERTER_POS) & 0x1)
#define DIVERTER_ON_COOLER ((io_0_inputs >> DIVERTER_POS) & 0x1)

#define BYPASS_RUN !((io_7_inputs >> BYPASS_VALVE_RUN) & 0x1)
#define DIVERTER_RUN !((io_7_inputs >> DIVERTER_VALVE_RUN) & 0x1)

#define PRI_FLOW_OK !((io_0_inputs >> PRI_FLOW) & 0x1)
#define SEC_FLOW_OK !((io_0_inputs >> SEC_FLOW) & 0x1)

#define PUMP_OK ((io_7_inputs >> PUMP_RUN) & 0x1)
#define COMPR_OK ((io_7_inputs >> COMPR_RUN) & 0x1)

#define AC_OK !((io_7_inputs >> AC_MONITOR) & 0x1)
