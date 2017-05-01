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
#include "debug.h"

#define VERSION_MAJOR 0
#define VERSION_MINOR 9

// EEPROM storage byte addresses
#define EEPROM_MODBUS_SVR_ADDR 0

#define MENU_ITEM_COUNT 4
menu_item menucontents[MENU_ITEM_COUNT] = {
  {"Baudrate", 9, 0, 8},
  {"Function", 9, 1, 4},
  {"Register", 9, 1, 49999}, 
  {"Length", 9, 1, 8}
};

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


#define DEBUG_LEVEL_DEFAULT L_OFF;

typedef struct  {
  baudrate_t  baudrate;
  byte dataFunction;            // e.g. 3=Read Holding
  int16_t dataRegister;         // first data register
  byte dataLength;              // number of registers
  byte dataFormat;              // data format
} modbusparameter;

#define BUTTON_DEBOUNCE_TIME 150UL        // ms
#define BUTTON_LONGPRESS_TIME 600UL       // ms

// Hardware definitions
#define LED_PIN LED_BUILTIN       // Internal LED
#define BUTTON_PIN 5              // D5 on Nano board

// Modbus definitions
#define MODBUS_RX_PIN 10          // D10 on nano board
#define MODBUS_TX_PIN 11          // D11 on nano board
#define MODBUS_TX_ENABLE_PIN  12  // D12 on nano board
#define MODBUS_RX_BUF_LEN 32      // Modbus RX buffer size in bytes
#define MODBUS_RX_IDLE false      // Modbus RX state
#define MODBUS_RX_ACTIVE true     // Modbus RX state
#define MODBUS_RX_TIMEOUT 500UL   // [ms] time to wait for reply
#define MODBUS_POLL_TIME 500UL    // [ms] time between modus transmissions

#define DEFAULT_MODBUS_SVR_ADDRESS 50   // used when EEPROM is invalid

#define SERIAL_BAUDRATE 19200 // For console interface

// LCD display definition
#define LCD_I2C_ADDRESS 0x3F
#define LCD_ROWS 4
#define LCD_COLUMNS 20

#define SPACE 32
#define CLR_LINE "                    "

uint8_t modbusSvrAddress = 50;
uint8_t prevModbusSvrAddress = modbusSvrAddress;
ModbusPacket testPacket;
debugLevels currentDebugLevel;
unsigned char *modbusTxBuf;
unsigned char modbusRxBuf[MODBUS_RX_BUF_LEN];
int modbusTxLen;
bool runMode = true, runModeShadow = false;               // modbus TX, RX and evaluate
bool adjustMode = false, adjustModeShadow = false;        // adjust modbus address
bool configMode = false, configModeShadow = false;        // config mode to adjust custom modbus parameters
bool customMode = false;                                  // 0 = XEnergy 1 = Custom

bool button_shadow = true;
unsigned long button_time;
unsigned long button_debounce_time = 0;
bool button_activated, button_activated_long;
unsigned long next_modbus_tx_time, modbus_rx_frame_timeout, modbus_rx_next_frame_timeout, modbus_rx_timeout;
bool modbus_rx_state;
bool modbus_rx_wait;
unsigned modbus_rx_count;

int8_t encoder_delta;   // encoder reading

modbusparameter modbusXE, modbusCustom;
modbusparameter *activeModbusConfig = &modbusXE;

SoftwareSerial modbusSerial(MODBUS_RX_PIN, MODBUS_TX_PIN); // RX, TX
LiquidCrystal_PCF8574 lcd(LCD_I2C_ADDRESS);  // set the LCD address 
LCD_Menu menu(LCD_ROWS, LCD_COLUMNS);

/*
 * ===============================================================
 * Ulitlity functions
 * ===============================================================
 */

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

// convert 4 bytes into 32bit IEEE 754 floating point
float convertToFloat (uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4 ) {

  union u_tag {
    byte b[4];
    float fval;
  } u;

  u.b[0] = b4;
  u.b[1] = b3;
  u.b[2] = b2;
  u.b[3] = b1;
  return u.fval;
}

/*
 * ===============================================================
 * Setup functions
 * ===============================================================
 */

void setup_serial() {
  // Open serial communications and wait for port to open:
  Serial.begin(SERIAL_BAUDRATE);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  debug(L_OFF, "\n\nModbus Scanner V%d.%d\n(C) 2017 Erwin Bejsta\n", VERSION_MAJOR, VERSION_MINOR);
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
  currentDebugLevel = DEBUG_LEVEL_DEFAULT;
  setup_serial();
  setup_LCD();
  setup_menu();
  setup_modbus();
  setup_encoder();
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  if(!digitalRead(BUTTON_PIN)) {  // button pressed
    modeSelection();
  }
}

/*
 * ===============================================================
 * Menu functions
 * ===============================================================
 */

void setup_menu() {
  menu.setMenuItems(menucontents, MENU_ITEM_COUNT);
  menu.setCallbackValueChange(menuItemValueChange);
  menu.setCallbackValueGet(menuItemValueGet);
}
 
void menuItemValueChange(int change) {
  int selectedItem = menu.getSelectedMenuItem();
  debug(L_DEBUG, "menuItemValueChange index %d by %d\n", selectedItem, change );
  if (selectedItem < 1) return;   // noting selected
  //selectedItem--;   // convert to zero based index
  switch (selectedItem) {
    case 1:   // baudrate
      activeModbusConfig->baudrate = activeModbusConfig->baudrate + (baudrate_t)change;
      if (activeModbusConfig->baudrate > menucontents[selectedItem-1].datamax)
        activeModbusConfig->baudrate = menucontents[selectedItem-1].datamax;
      if (activeModbusConfig->baudrate < menucontents[selectedItem-1].datamin)
        activeModbusConfig->baudrate = menucontents[selectedItem-1].datamin;
      break;
    case 2:   // function
      activeModbusConfig->dataFunction += change;
      if (activeModbusConfig->dataFunction > menucontents[selectedItem-1].datamax)
        activeModbusConfig->dataFunction = menucontents[selectedItem-1].datamax;
      if (activeModbusConfig->dataFunction < menucontents[selectedItem-1].datamin)
        activeModbusConfig->dataFunction = menucontents[selectedItem-1].datamin;
      break;
    case 3:   // register
      activeModbusConfig->dataRegister += change;
      if (activeModbusConfig->dataRegister > menucontents[selectedItem-1].datamax)
        activeModbusConfig->dataRegister = menucontents[selectedItem-1].datamax;
      if (activeModbusConfig->dataRegister < menucontents[selectedItem-1].datamin)
        activeModbusConfig->dataRegister = menucontents[selectedItem-1].datamin;
      break;
    case 4:   // length
      activeModbusConfig->dataLength += change;
      if (activeModbusConfig->dataLength > menucontents[selectedItem-1].datamax)
        activeModbusConfig->dataLength = menucontents[selectedItem-1].datamax;
      if (activeModbusConfig->dataLength < menucontents[selectedItem-1].datamin)
        activeModbusConfig->dataLength = menucontents[selectedItem-1].datamin;
      break;

    default:
      break;
  }
  menu.updateSelectedMenuItemValue();
}

unsigned int menuItemValueGet(int menuItem) {
  switch (menuItem-1) {
    case 0:
      return (baud_rate[activeModbusConfig->baudrate]);
      break;
    case 1:    
      return activeModbusConfig->dataFunction;
      break;
    case 2:
      return activeModbusConfig->dataRegister;
      break;
    case 3:
      return activeModbusConfig->dataLength;
      break;
    default:
      break;
  }
  return 0;
}

/*
 * ===============================================================
 * Modbus functions
 * ===============================================================
 */

void config_modbus() {
  // Open Modbus port and set the data rate for the Modbus port
  modbusSerial.begin(baud_rate[activeModbusConfig->baudrate]);

  // get modbus address from EEPROM
  modbusSvrAddress = EEPROM.read(EEPROM_MODBUS_SVR_ADDR);
  if ( (modbusSvrAddress <1) || (modbusSvrAddress >127) ) {
    modbusSvrAddress = DEFAULT_MODBUS_SVR_ADDRESS;
  }

  modbus_rx_frame_timeout = endOfFrameTimeout(baud_rate[activeModbusConfig->baudrate]);
  modbus_rx_state = MODBUS_RX_IDLE;
}

void setup_modbus() {
  // Westmont modbus parameters
  modbusXE.baudrate = b19200;
  modbusXE.dataFunction = READ_HOLDING_REGISTERS;
  modbusXE.dataRegister = 4119;           // Active Energy Import
  modbusXE.dataLength = 2;                // 32bit float takes 2 16bit registers
  modbusXE.dataFormat = 0;

  // Custom modbus parameters
  modbusCustom.baudrate = b19200;
  modbusCustom.dataFunction = READ_HOLDING_REGISTERS;
  modbusCustom.dataRegister = 4119;           // Active Energy Import
  modbusCustom.dataLength = 2;                // 32bit float takes 2 16bit registers
  modbusCustom.dataFormat = 0;
 
  pinMode(MODBUS_TX_ENABLE_PIN, OUTPUT);
  digitalWrite(MODBUS_TX_ENABLE_PIN, LOW);
  
  config_modbus();
  makeModbusFrame();
}

void modbusWrite(unsigned char *txBuf, unsigned int txLen) { 

  // clear receive buffer
  for (int i=0; i < MODBUS_RX_BUF_LEN; i++) {
    modbusRxBuf[i] = 0;
  }
  
  // switch RS485 driver to TX
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

  // switch RS485 driver to RX
  digitalWrite(MODBUS_TX_ENABLE_PIN, LOW);
}

void modbusEvaluateReply() {
  int i;
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
  lcd.setCursor(0,1);
  if (activeModbusConfig == &modbusXE) {     
    if (modbusRxBuf[3]==0 && modbusRxBuf[4]==0 && modbusRxBuf[5]==0 && modbusRxBuf[6]==0) { // no reading from meter
      lcd.print("Meter not detected");
      lcd.setCursor(0,2);
      lcd.print(CLR_LINE);
    } else { // we have a reading
      lcd.print(CLR_LINE);
      lcd.setCursor(0,1);
      lcd.print("AEI: ");
      lcd.print(convertToFloat( modbusRxBuf[3], modbusRxBuf[4], modbusRxBuf[5], modbusRxBuf[6]) );
      lcd.print("kWh");
      // print raw data
      //lcd.setCursor(0,2);
      //lcdPrint("AEI: %02x %02x %02x %02x", modbusRxBuf[3], modbusRxBuf[4], modbusRxBuf[5], modbusRxBuf[6]); // display in hex format
    }
  } else { // show raw output
    for (i=0; i<activeModbusConfig->dataLength * 2; i+=2) {
      if (i < 12)
        lcdPrint("%02x %02x ", modbusRxBuf[i+3], modbusRxBuf[i+4]); // display in hex format
      if (i == 4) lcd.setCursor(0,2);   // move output to next line
    }
  }
}

bool modbusRead() { // returns true when modbus rx is completed, adds received byte to rx buffer

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

  // look for end of transmission (frame timeout)
  if ( (modbus_rx_state==MODBUS_RX_ACTIVE) && (millis() >= modbus_rx_next_frame_timeout) ) {
    modbus_rx_state = MODBUS_RX_IDLE;   // modbus back into idle mode
    digitalWrite(LED_PIN, LOW);
    return true;
  }
  
  return false;
}

void makeModbusFrame() {
  testPacket.id = modbusSvrAddress;
  testPacket.function = activeModbusConfig->dataFunction;
  testPacket.address = activeModbusConfig->dataRegister;
  testPacket.no_of_registers = activeModbusConfig->dataLength;
  testPacket.register_array = NULL;
  
  constructFrame(&testPacket);
  modbusTxLen = getFrame(&modbusTxBuf);
}

/*
 * ===============================================================
 * LCD functions
 * ===============================================================
 */

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
}

void lcdShowInfo (int lineNo) {
  lcd.setCursor(0, lineNo);
  lcdPrint("Modbus Scanner V%d.%d", VERSION_MAJOR, VERSION_MINOR);
  lcd.setCursor(0, lineNo+1);
  lcd.print("(C)2017 Erwin Bejsta");
}

/*
 * ===============================================================
 * User Input and Operating Mode 
 * ===============================================================
 */

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
  debug(L_DEBUG, "responding to button activated %s\n", button_activated_long ? "long" : " ");
  if (button_activated_long) {
    button_activated_long = false;
    if (!customMode) return;    // only for custom mode   
    if (!configMode) {
      if (!runMode) return;       // enter from run mode only
      configMode = true;
      runMode = false;
      return;
    } 
    if (configMode) {
      configMode = false;
      runMode = true;
      config_modbus();
      return;
    }
  }

  if (button_activated) {
    if (adjustMode) {
      adjustMode = false;
      runMode = true;
      // save modbus address if it has been adjusted
      if (prevModbusSvrAddress != modbusSvrAddress);{
        EEPROM.write(EEPROM_MODBUS_SVR_ADDR, modbusSvrAddress);
      }
      goto done;
    }
    if (runMode) {
      prevModbusSvrAddress = modbusSvrAddress;
      adjustMode = true;
      runMode = false;
      goto done;
    }
    if (configMode) {
      menu.uiSelect(false);
    }   
  }
  done:
  button_activated = false;
  debug(L_DEBUG, "Config: %d Run: %d Adjust: %d\n",configMode, runMode, adjustMode);
}

void modeSelection() {
  lcd.home();
  lcd.print("-- release button --");
  // select operating mode
  while(!digitalRead(BUTTON_PIN)) {}; // wait for button release
  lcd.clear();
  lcd.setCursor(0,2);
  lcd.print("Short press = change");
  lcd.setCursor(0,3);
  lcd.print("Long press = exit");
  debug(L_INFO, "Mode selection dialog entered\n");
  
  start:
  lcd.home();
  lcd.print("Mode: ");
  if (customMode) {
    lcd.print("Custom ");
    activeModbusConfig = &modbusCustom;
  } else {
    lcd.print("XEnergy");
    activeModbusConfig = &modbusXE;
  }
  
  while (!button_activated_long) {
    readButtonInput();
    if (button_activated) {
      customMode = !customMode;
      button_activated = false;
      goto start;
    }
  }

  customMode ? debug(L_INFO, "Custom mode selected\n") : debug(L_INFO, "XEnergy mode selected\n");
  button_activated = false;
  button_activated_long = false;
  lcd.clear();
}

/*
 * ===============================================================
 * Loops 
 * ===============================================================
 */

void configLoop() {
  int selectedItem;
  if (!configModeShadow) {
    configModeShadow = true;
    adjustModeShadow = false;
    runModeShadow = false;
    menu.begin();
    debug(L_DEBUG, "Config menu activated\n");
  }
  if (updateEncoders(&encoder_delta)) {
    if (menu.getEditMode()) { // change selected value
      //selectedItem = menu.getSelectedMenuItem();
      menuItemValueChange((int)encoder_delta);
    } else {    // move menu up/down
      if (encoder_delta > 0) menu.moveDown();
      if (encoder_delta < 0) menu.moveUp();  
    }  
  }
}

void adjustLoop() {
  if (!adjustModeShadow) {   
    lcd.setBacklight(1);
    
    lcd.clear();
    lcd.noCursor();
    lcdShowInfo(2);
    //lcd.setCursor(0, 0);
    lcd.home();
    lcd.print("Modbus:");
    adjustModeShadow = true;
    runModeShadow = false;
    updateEncoders(&encoder_delta);   // clears any pending encoder counts
    lcd.blink();
    lcd.setCursor(8, 0);
    lcd.print(modbusSvrAddress);
    lcd.print("   ");
    lcd.setCursor(8, 0);
  } else {
    // change modbus address
    if (updateEncoders(&encoder_delta)) {
      modbusSvrAddress += encoder_delta;
 
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
  // execute once when switching into run mode
  if (!runModeShadow) {
    lcd.noBlink();
    lcd.noCursor();
    lcd.clear();
    lcd.home();
    lcd.print("Modbus: ");
    lcd.print(modbusSvrAddress);
    lcd.setCursor(0, 3);
    lcdPrint("Modbus Scanner V%d.%d", VERSION_MAJOR, VERSION_MINOR);
    makeModbusFrame();
    runModeShadow = true;
    adjustModeShadow = false;
    configModeShadow = false;
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

  // modbus sentence received?
  if (modbusRead()) {
    modbusEvaluateReply();
  }

  // timeout
  if ((millis() > modbus_rx_timeout) && modbus_rx_wait) {
    modbus_rx_state = MODBUS_RX_IDLE;    // reset for next time
    modbus_rx_wait = false;
    lcd.setCursor(12, 0);
    lcd.print("--");
    lcd.setCursor(19, 0);
    lcd.write(SPACE);
    lcd.setCursor(0, 1);
    lcd.print("                    ");
    next_modbus_tx_time = millis() + MODBUS_POLL_TIME;
  }
}

void loop() {
  if (adjustMode) { 
    adjustLoop();
  }
  if (runMode) {
    runLoop();
  }
  if (configMode) {
    configLoop();
  }
  
  if (readButtonInput()) {
    respondToButton();
  }
}



