/*
 * Modbus Tester
 * Erwin Bejsta 2017
 
 * The circuit:
 * Modbus RX is digital pin 10 
 * Modbus TX is digital pin 11
 *
 * 7KT1 1531:
 * AEI - Active Energy Input Modbus Register 4119+4120
 * AEE - Active Energy Export Modbus Reegister 4161+4162
 * 32bit IEEE 754:2008 floating point format
 */ 
#include <SoftwareSerial.h>
#include <Wire.h> 
#include <EEPROM.h>

#include "LiquidCrystal_PCF8574.h"
#include "ModbusClient.h"
#include "LCD_Menu.h"
#include "Encoder.h"

#define VERSION_MAJOR 0
#define VERSION_MINOR 5

// EEPROM storage byte addresses
#define EEPROM_MODBUS_SVR_ADDR 0

enum baudrate_t {
  b300,
  b600,
  b1200,
  b2400,
  b4800,
  b9600,
  b19200,
  b38400,
  b56800
};

uint16_t baud_rate[] = { 300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 56800 };

enum debugLevels {
  L_OFF,
  L_ERROR,
  L_DEBUG,
  L_INFO
};

typedef struct  {
  baudrate_t  baudrate;
  byte dataFunction;            // e.g. 3=Read Holding
  int16_t dataRegister;
  byte dataLength;              // number of registers
  byte dataFormat;
} modbusparameter;

#define BUTTON_DEBOUNCE_TIME 150UL        // ms
#define BUTTON_LONGPRESS_TIME 600UL       // ms

// Hardware definitions
#define LED_PIN LED_BUILTIN
#define BUTTON_PIN 5          // D5

// Modbus definitions
#define MODBUS_RX_PIN 10      // Marked D10 on nano board
#define MODBUS_TX_PIN 11      // Marked D11 on nano board
#define MODBUS_TX_ENABLE_PIN  12  // Marked D12 on nano board
#define MODBUS_RX_BUF_LEN 128
#define MODBUS_RX_IDLE false
#define MODBUS_RX_ACTIVE true
#define MODBUS_RX_TIMEOUT 800UL    // ms
#define MODBUS_POLL_TIME 500UL     // ms

#define DEFAULT_MODBUS_SVR_ADDRESS 50   // used when EEPROM is invalid

#define SERIAL_BAUDRATE 19200 // For console interface

// LCD display definition
#define LCD_I2C_ADDRESS 0x3F
#define LCD_ROWS 4
#define LCD_COLUMNS 20

#define SPACE 32

//int txCount = 0;
int modbusSvrAddress = 50;
ModbusPacket testPacket;
debugLevels currentDebugLevel;
LCD_Menu menu(LCD_ROWS, LCD_COLUMNS);
unsigned char *modbusTxBuf;
unsigned char modbusRxBuf[MODBUS_RX_BUF_LEN];
int modbusTxLen;
bool runMode = true, runModeShadow = true;
bool configMode = false, configModeShadow = false;

bool button_shadow = true;
unsigned long button_time;
unsigned long button_debounce_time = 0;
bool button_activated, button_activated_long;
unsigned long next_modbus_tx_time, modbus_rx_frame_timeout, modbus_rx_next_frame_timeout, modbus_rx_timeout;
bool modbus_rx_state;
bool modbus_rx_wait;
unsigned modbus_rx_count;

modbusparameter modbusWestmont, modbusCustom;
modbusparameter *activeModbusConfig = &modbusWestmont;

SoftwareSerial modbusSerial(MODBUS_RX_PIN, MODBUS_TX_PIN); // RX, TX
LiquidCrystal_PCF8574 lcd(LCD_I2C_ADDRESS);  // set the LCD address 

// print debug output on console interface
void debug(debugLevels level, char *sFmt, ...)
{
  if (level > currentDebugLevel) return;  // bypass if level is not high enough
  char acTmp[128];       // place holder for sprintf output
  va_list args;          // args variable to hold the list of parameters
  va_start(args, sFmt);  // mandatory call to initilase args 

  vsprintf(acTmp, sFmt, args);
  Serial.print(acTmp);
  // mandatory tidy up
  va_end(args);
  return;
}

void lcdPrint(char *sFmt, ...)
{
  char acTmp[128];       // place holder for sprintf output
  va_list args;          // args variable to hold the list of parameters
  va_start(args, sFmt);  // mandatory call to initilase args 

  vsprintf(acTmp, sFmt, args);
  lcd.print(acTmp);
  // mandatory tidy up
  va_end(args);
  return;
}

void setup_serial() {
  // Open serial communications and wait for port to open:
  Serial.begin(SERIAL_BAUDRATE);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  debug(L_OFF, "\n\nModbus Scanner V%d.%d\n(C) 2017 Erwin Bejsta\n", VERSION_MAJOR, VERSION_MINOR);
}

void config_modbus() {
  // Open Modbus port and set the data rate for the Modbus port
  modbusSerial.begin(baud_rate[activeModbusConfig->baudrate]);

  // get modbus address from EEPROM
  modbusSvrAddress = EEPROM.read(EEPROM_MODBUS_SVR_ADDR);
  if ( (modbusSvrAddress <1) || (modbusSvrAddress >127) ) {
    modbusSvrAddress = DEFAULT_MODBUS_SVR_ADDRESS;
  }
  lcd.setCursor(8, 0);
  lcd.print(modbusSvrAddress);

  modbus_rx_frame_timeout = endOfFrameTimeout(baud_rate[activeModbusConfig->baudrate]);
  
  testPacket.id = modbusSvrAddress;
  testPacket.function = activeModbusConfig->dataFunction;
  testPacket.address = activeModbusConfig->dataRegister;
  testPacket.no_of_registers = activeModbusConfig->dataLength;
  testPacket.register_array = NULL;
  makeModbusFrame();
  modbus_rx_state = MODBUS_RX_IDLE;
}

void setup_modbus() {
  // Westmont modbus parameters
  modbusWestmont.baudrate = b19200;
  modbusWestmont.dataFunction = READ_HOLDING_REGISTERS;
  modbusWestmont.dataRegister = 4099;
  modbusWestmont.dataLength = 1;
  modbusWestmont.dataFormat = 0;

  pinMode(MODBUS_TX_ENABLE_PIN, OUTPUT);
  digitalWrite(MODBUS_TX_ENABLE_PIN, LOW);
  
  config_modbus();
}

void setup_LCD() {
  int error;
  // Check for presence of the LCD at the I2C address 
  Wire.begin();
  Wire.beginTransmission(LCD_I2C_ADDRESS);
  error = Wire.endTransmission();

  if (error != 0) {
    debug(L_ERROR, "Error: <%d> - LCD not found at I2C address 0x%X\n", error, LCD_I2C_ADDRESS );
    delay(3000);
  }

  lcd.begin(LCD_COLUMNS, LCD_ROWS); // initialize the lcd
  lcd.setBacklight(1);
  lcd.home();
  lcd.clear();
  lcd.print("Modbus: ");
  
}

void setup_encoder() {
  // Encoder Setup
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  beginEncoder();

  button_activated = false;
  button_activated_long = false;
  button_shadow = false;
}

void setup() {
  currentDebugLevel = L_OFF;
  setup_serial();
  setup_LCD();
  setup_modbus();
  setup_encoder();
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
}

void modbusWrite(unsigned char *txBuf, unsigned int txLen) { 

  // switch 485 driver to TX
  digitalWrite(MODBUS_TX_ENABLE_PIN, HIGH);

  debug(L_DEBUG, "Modbus TX %d bytes: ", txLen);
  
  for (unsigned char i = 0; i < txLen; i++) {
    modbusSerial.write(txBuf[i]);
    debug(L_DEBUG, "<%02X> ", txBuf[i]);
  }
  modbusSerial.flush();
  debug(L_DEBUG, "\n");
  
  // allow a frame delay to indicate end of transmission
  //delayMicroseconds(T3_5); 

  // switch 485 driver to RX
  digitalWrite(MODBUS_TX_ENABLE_PIN, LOW);
}

bool modbusRead() { // returns true when modbus reive is completed

  // check if a byte has arrived
    if (modbusSerial.available()) {
      // read and store received byte      
      modbusRxBuf[modbus_rx_count++] = modbusSerial.read();
      // maximum permissible time before we receive next byte
      modbus_rx_next_frame_timeout = millis() + modbus_rx_frame_timeout;
      // rx is now active
      if (modbus_rx_state != MODBUS_RX_ACTIVE) {
        modbus_rx_state = MODBUS_RX_ACTIVE;
        digitalWrite(LED_PIN, HIGH);      
      }
    }

  // look for end of transmission by frame timeout
  if ( (modbus_rx_state==MODBUS_RX_ACTIVE) && (millis() >= modbus_rx_next_frame_timeout) ) {
    modbus_rx_state = MODBUS_RX_IDLE;   // modbus back into idle mode
    digitalWrite(LED_PIN, LOW);
    return true;
  }
  
  return false;
}

void makeModbusFrame() {
  
  testPacket.id = modbusSvrAddress;
  testPacket.function = READ_HOLDING_REGISTERS;
  testPacket.address = 4099;
  testPacket.no_of_registers = 1;
  testPacket.register_array = NULL;
  
  constructFrame(&testPacket);
  modbusTxLen = getFrame(&modbusTxBuf);
}

void lcdShowInfo (int lineNo) {
  lcd.setCursor(0, lineNo);
  lcdPrint("Modbus Scanner V%d.%d", VERSION_MAJOR, VERSION_MINOR);
  lcd.setCursor(0, lineNo+1);
  lcd.print("(C)2017 Erwin Bejsta");
}

int8_t delta;

void configLoop() {
  if (!configModeShadow) {   
    lcd.setBacklight(1);
    
    lcd.clear();
    lcdShowInfo(2);
    //lcd.setCursor(0, 0);
    lcd.home();
    lcd.print("Modbus:");
    configModeShadow = true;
    runModeShadow = false;
    updateEncoders(&delta);   // clears any pending encoder counts
    lcd.blink();
    lcd.setCursor(8, 0);
    lcd.print(modbusSvrAddress);
    lcd.print("   ");
    lcd.setCursor(8, 0);
  } else {
    // change modbus address
    if (updateEncoders(&delta)) {
      modbusSvrAddress += delta;
 
      // range check modbus address
      if (modbusSvrAddress > 127) modbusSvrAddress = 127;
      if (modbusSvrAddress < 1) modbusSvrAddress = 1;

      // display modbus address
      lcd.blink();
      lcd.setCursor(8, 0);
      lcd.print(modbusSvrAddress);
      lcd.print("   ");
      lcd.setCursor(8, 0);
    }
  }
}

void runLoop() {
  int i;

  // execute once when switching into run mode
  if (!runModeShadow) {
    lcd.noBlink();
    lcd.clear();
    lcd.home();
    lcd.print("Modbus: ");
    lcd.print(modbusSvrAddress);
    lcd.setCursor(0, 3);
    lcdPrint("Modbus Scanner V%d.%d", VERSION_MAJOR, VERSION_MINOR);
    makeModbusFrame();
    runModeShadow = true;
    configModeShadow = false;
    EEPROM.write(EEPROM_MODBUS_SVR_ADDR, modbusSvrAddress);
    next_modbus_tx_time = millis() + MODBUS_POLL_TIME;
  }

  if ((millis() >= next_modbus_tx_time) && (modbus_rx_state==MODBUS_RX_IDLE) && !modbus_rx_wait) {
    constructFrame(&testPacket);
    modbusTxLen = getFrame(&modbusTxBuf);
    if (modbusTxLen > 2) {
      modbusWrite(modbusTxBuf, modbusTxLen);
    }
    modbus_rx_timeout = millis() + MODBUS_RX_TIMEOUT;
    modbus_rx_wait= true;
    lcd.setCursor(19, 0);
    lcd.write(126);
  }

  if (modbus_rx_wait) {
    
  }
  // modbus sentence received?
  if (modbusRead()) {
    modbus_rx_wait = false;
    debug(L_DEBUG, "Modbus RX %d bytes: ", modbus_rx_count);
    for (i=0; i<modbus_rx_count; i++) {
      debug(L_DEBUG, "<%02X> ", modbusRxBuf[i]);
    }
    debug(L_DEBUG, "\n");
    lcd.setCursor(12, 0);
    if (modbusRxBuf[0] == modbusSvrAddress) {    
      lcd.print("OK");
    } else {
      lcd.print("--");
    }
    modbus_rx_count = 0;
    next_modbus_tx_time = millis() + MODBUS_POLL_TIME;
    lcd.setCursor(19, 0);
    lcd.write(SPACE);
  }

  if (modbus_rx_wait) {
    lcd.setCursor(19, 0);
    lcd.write(127);
  }

  // timeout
  if ((millis() > modbus_rx_timeout) && modbus_rx_wait) {
    modbus_rx_state = MODBUS_RX_IDLE;    // reset for next time
    modbus_rx_wait = false;
    lcd.setCursor(12, 0);
    lcd.print("--");
    lcd.setCursor(19, 0);
    lcd.write(SPACE);
    next_modbus_tx_time = millis() + MODBUS_POLL_TIME;
  }
}

bool readButtonInput() {  // returns true if button has been activated
  bool retVal = false;
  // read button input
  if (millis() < button_debounce_time ) return retVal;  // do not read button during debounce time
  if(!digitalRead(BUTTON_PIN)) {  // button pressed
    if (!button_shadow) {   // detect leading edge
      //debug(L_DEBUG, "Button activated\n");
      button_shadow = true;
      button_time = millis();
      button_debounce_time = button_time + BUTTON_DEBOUNCE_TIME;
    }
  } else {    // button not pressed
    if (button_shadow) {   // detect trailing edge
      button_shadow = false;
      if ( (millis() - button_time) > BUTTON_LONGPRESS_TIME ) {
        button_activated_long = true;
      } else {
        button_activated = true;
      }
      button_debounce_time = button_time + BUTTON_DEBOUNCE_TIME;
      retVal = true;
    }
  } 
  return retVal;   
}

void respondToButton () {
  //debug(L_DEBUG, "responding to button %d %d\n", button_activated, button_activated_long);
  if (button_activated_long) {
    button_activated_long = false;
  }

  if (button_activated) {
    if (configMode) {
      configMode = false;
      runMode = true;
    } else {
      if (runMode) {
        configMode = true;
        runMode = false;
      }
    }
  button_activated = false;  
  }
  //debug(L_DEBUG, "Config: %d Run: %d\n",configMode, runMode);
}

void loop() {
  if (configMode) { 
    configLoop();
  }
  else {
    runLoop();
  }
  if (readButtonInput()) {
    respondToButton();
  }
}


