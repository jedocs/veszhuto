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
#define WD                  2
#define SD_CS               32
#define EOC                 39
#define AN_CS               19
#define ETH_CS              18
#define MMV_TRIG            15
#define SCK 				        14
#define MISO 				        12
#define MOSI 				        13
#define CD                  25
#define MMV_RESET           0
#define ETH_INT             33
#define I2C_SDA             21
#define I2C_SCL             22
#define VBAT                35

//IO_EXPANDER addr 0
//PORT A
#define AC_MONITOR          0
#define PUMP                1
#define CTRL_PULSE 				  2
#define SSR                 3
#define BYPASS_VALVE        4
#define BYPASS_VALVE_RUN    5
#define DIVERTER_VALVE_RUN  6
#define DIVERTER_VALVE      7

//PORT B
#define SPARE_IN 				    8
#define LIMIT4 				      9
#define LIMIT3 				      10
#define DIVERTER_POS 				11
#define BYPASS_POS 				  12
#define SEC_FLOW 				    13
#define PRI_FLOW 			      14
#define WATER_PULSE 			  15

//IO_EXPANDER addr 7
//PORT A
#define PB_LED		          0
#define IO1 				        1
#define IO2 				        2
#define IO3 				        3
#define PB_B   			        4
#define PB_R 			          5
#define PB_L 			          6
#define PB1 			          7

//PORT B
#define SELFTEST            8
#define IO8                 9
#define IO9                 10
#define IO10                11
#define RED_LED             12
#define GREEN_LED           13
#define ORANGE_LED          14
#define PB2                 15

//analog inputs
#define COMPR_CT 		    0
#define PUMP_CT         1
#define SPARE_AN_2      2
#define SPARE_AN_1      3
#define SEC_PRESS       4
#define PRI_PRESS       5
#define HE_TEMP_2       6
#define HE_TEMP_1       7
#define ROOM_TEMP       8
#define SEC_TEMP_2  		9
#define SEC_TEMP_1  		10
#define PRI_TEMP_2 			11
#define PRI_TEMP_1  		12
#define TCALL_3V3 			13
#define PSUP_5V 				14
#define TCALL_5V  			15

#define UART_BANUD_RATE     9600

#define IP5306_ADDR         0X75
#define IP5306_REG_SYS_CTL0 0x00

#define OLED_ADDR			0x3C

//#define DUMP_AT_COMMANDS



#define TINY_GSM_MODEM_SIM800      // Modem is SIM800
#define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb

#define ADC_READ_THRESHOLD 1
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

#define STARTUP_DELAY   10 //*********************************************
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
#define ON LOW
#define OFF HIGH

#define BYPASS_CLOSED !((io_0_inputs >> BYPASS_POS) & 0x1)
#define BYPASS_OPEN ((io_0_inputs >> BYPASS_POS) & 0x1)

#define DIVERTER_ON_WATER !((io_0_inputs >> DIVERTER_POS) & 0x1)
#define DIVERTER_ON_COOLER ((io_0_inputs >> DIVERTER_POS) & 0x1)

#define BYPASS_RUN !((io_0_inputs >> BYPASS_VALVE_RUN) & 0x1)
#define DIVERTER_RUN !((io_0_inputs >> DIVERTER_VALVE_RUN) & 0x1)

#define PRI_FLOW_OK !((io_0_inputs >> PRI_FLOW) & 0x1)
#define SEC_FLOW_OK !((io_0_inputs >> SEC_FLOW) & 0x1)

#define PUMP_OK_THRESHOLD 2000
#define COMPR_OK_THRESHOLD 1500

#define PUMP_OK (pump_current > PUMP_OK_THRESHOLD)
#define COMPR_OK (compressor_current > COMPR_OK_THRESHOLD)

#define AC_OK !((io_0_inputs >> AC_MONITOR) & 0x1)

#define CONTACTOR_CLOSED !((io_0_inputs >> SPARE_IN) & 0x1)
