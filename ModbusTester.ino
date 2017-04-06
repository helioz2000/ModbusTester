/*
 Modbus Tester
 Erwin Bejsta 2017
 
 The circuit:
 Modbus RX is digital pin 10 
 Modbus TX is digital pin 11
 
 */ 
#include <SoftwareSerial.h>
#include <Wire.h> 
#include "LiquidCrystal_PCF8574.h"
#include "ModbusClient.h"

#define VERSION_MAJOR 0
#define VERSION_MINOR 4

enum userinput {
  LowLow,
  Low,
  Middle,
  High,
  HighHigh,
};

#define LED_PIN 13

#define MODBUS_RX_PIN 10      // Marked D10 on nano board
#define MODBUS_TX_PIN 11      // Marked D11 on nano board
#define MODBUS_TX_ENABLE_PIN  12  // Marked D12 on nano board
#define MODBUS_BAUDRATE 19200
#define MODBUS_RX_BUF_LEN 128
#define MODBUS_RX_TIMEOUT 1000
#define MODBUS_CHAR_TIMEOUT 100

#define POLL_TIME 350

#define SERIAL_BAUDRATE 19200 // For console interface

#define LCD_I2C_ADDRESS 0x3F
#define LCD_LINES 4
#define LCD_CPL 20    // Characters per line

#define UI_INTERVAL 200
#define UI_H 712
#define UI_HH UI_H + UI_INTERVAL
#define UI_L 312
#define UI_LL UI_L - UI_INTERVAL
#define UI_INPUT_PIN A0

int txCount = 0;
int modbusSvrAddress = 50;
ModbusPacket testPacket;
unsigned char *modbusTxBuf;
unsigned char modbusRxBuf[MODBUS_RX_BUF_LEN];
int modbusTxLen;
bool runMode = true;
bool configMode = false;
userinput inputState = Middle;

SoftwareSerial modbusSerial(MODBUS_RX_PIN, MODBUS_TX_PIN); // RX, TX
LiquidCrystal_PCF8574 lcd(LCD_I2C_ADDRESS);  // set the LCD address 

// print debug output on console interface
void debug(char *sFmt, ...)
{
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
  debug("Modbus Scanner V%d.%d\n", VERSION_MAJOR, VERSION_MINOR);
}

void setup_modbus() {
    // Open Modbus port
  // set the data rate for the Modbus port
  modbusSerial.begin(MODBUS_BAUDRATE);
  pinMode(MODBUS_TX_ENABLE_PIN, OUTPUT);
  digitalWrite(MODBUS_TX_ENABLE_PIN, LOW);
  testPacket.id = modbusSvrAddress;
  testPacket.function = READ_HOLDING_REGISTERS;
  testPacket.address = 4099;
  testPacket.no_of_registers = 1;
  testPacket.register_array = NULL;
  makeModbusFrame();
}

void setup_LCD() {
  int error;
  // Check for presence of the LCD at the I2C address 
  Wire.begin();
  Wire.beginTransmission(LCD_I2C_ADDRESS);
  error = Wire.endTransmission();

  if (error != 0) {
    debug("Error: <%d> - LCD not found at I2C address 0x%X\n", error, LCD_I2C_ADDRESS );
  } // if

  lcd.begin(LCD_CPL, LCD_LINES); // initialize the lcd
  lcd.setBacklight(1);
  lcd.home();
  lcd.clear();
  lcd.print("Modbus: ");
  lcd.print(modbusSvrAddress);
}

void setup() {
  setup_serial();
  setup_LCD();
  setup_modbus();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
}

void modbusWrite(unsigned char *txBuf, unsigned int txLen) { 

  digitalWrite(LED_PIN, HIGH);
  // switch 485 driver to TX
  digitalWrite(MODBUS_TX_ENABLE_PIN, HIGH);

  debug("Modbus TX %d bytes: ", txLen);
  
  for (unsigned char i = 0; i < txLen; i++) {
    modbusSerial.write(txBuf[i]);
    debug("<%02X> ", txBuf[i]);
  }
  modbusSerial.flush();
  debug("\n");
  
  // allow a frame delay to indicate end of transmission
  //delayMicroseconds(T3_5); 

  // switch 485 driver to RX
  digitalWrite(MODBUS_TX_ENABLE_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
}

int modbusRead(unsigned long timeout) {
  unsigned char rx_byte;
  bool rx_has_started = false;
  unsigned int rx_count = 0;

  unsigned long rx_timeout = millis() + timeout;

  // wait for first byte to arrive
  while (millis() < rx_timeout) {
    if (modbusSerial.available()) {
      rx_byte = modbusSerial.read();
      rx_has_started = true;
      modbusRxBuf[rx_count++] = rx_byte;
      break;
    }
  }

  // has timeout occured?
  if (!rx_has_started) {
    return -1;
  }

  unsigned long char_timeout = millis() + MODBUS_CHAR_TIMEOUT;
  
  while (millis() < char_timeout) {
    if (modbusSerial.available()) {
      rx_byte = modbusSerial.read();
      modbusRxBuf[rx_count++] = rx_byte;
      char_timeout = (millis() + MODBUS_CHAR_TIMEOUT);
    }
  }

  modbusRxBuf[rx_count] = 0;    // end marker
  
  return rx_count;
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

void inputRead() {
  int  sensorValue = analogRead(UI_INPUT_PIN);
  //Serial.println(sensorValue);

  if ( sensorValue > UI_H ) {
    if ( sensorValue > UI_HH ) {
      inputState = HighHigh;
      //Serial.println("++");
    } else {
      inputState = High;
      //Serial.println("+");
    }
  } else {
    if ( sensorValue < UI_L ) {
      if ( sensorValue < UI_LL ) {
        inputState = LowLow;
        //Serial.println("--");
      } else {
        inputState = Low;
        //Serial.println("-");
      }
    } else {
      inputState = Middle;
    }
  }
}

void configLoop() {

  if (!configMode) {   
    lcd.setBacklight(1);
    
    lcd.clear();
    lcdShowInfo(2);
    //lcd.setCursor(0, 0);
    lcd.home();
    lcd.print("Modbus:");
  } else {
    // change modbus address
    switch (inputState) {
      case Low:
      case LowLow:
        modbusSvrAddress--;
        break;
      case High:
      case HighHigh:
        modbusSvrAddress++;
        break;
      default:
        break;
    }
    
    // range check modbus address
    if (modbusSvrAddress > 127) modbusSvrAddress = 127;
    if (modbusSvrAddress < 1) modbusSvrAddress = 1;

     // display modbus address
    lcd.setCursor(8, 0);
    lcd.print(modbusSvrAddress);
    lcd.print("   ");

    // delay
    switch (inputState) {
      case Low:
      case High:
        delay(600);
        break;
      case LowLow:
      case HighHigh:
        delay(200);
        break;
      default:
        break;
    }
  }
  runMode = false;
  configMode = true;
}

void runLoop() {

  int i;
  int rxLen = 0;

  if (!runMode) {
    lcd.clear();
    lcd.home();
    lcd.print("Modbus: ");
    lcd.print(modbusSvrAddress);
    lcd.setCursor(0, 3);
    lcdPrint("Modbus Scanner V%d.%d", VERSION_MAJOR, VERSION_MINOR);
    makeModbusFrame();
  }
  
  runMode = true;
  configMode = false;

  delay(POLL_TIME);

  constructFrame(&testPacket);
  modbusTxLen = getFrame(&modbusTxBuf);

  
  if (modbusTxLen > 2) {
    lcd.setCursor(0, 1);
    lcd.print("TX");
    modbusWrite(modbusTxBuf, modbusTxLen);
    txCount++;
    lcd.setCursor(0, 1);
    lcd.print("  ");
  }

  lcd.setCursor(0, 1);
  lcd.print("RX");
  modbusRxBuf[0] = 0;
  rxLen = modbusRead(MODBUS_RX_TIMEOUT);
  lcd.setCursor(0, 1);
  lcd.print("  ");
 
  if (rxLen > 0) {
    debug("Modbus RX %d bytes: ", rxLen);
    for (i=0; i<rxLen; i++) {
      debug("<%02X> ", modbusRxBuf[i]);
    }
    debug("\n");   
  }

  lcd.setCursor(13, 0);
  if (modbusRxBuf[0] == modbusSvrAddress) {
    lcd.print("OK    ");
  }
  else {
    lcd.print("NO GO ");
    lcd.setBacklight(0);
    delay(400);
    lcd.setBacklight(1);
  } 
}


void loop() {
  inputRead();
  if (inputState == Middle) {
    runLoop();
  } else {
    configLoop();
  }

  /*
    if (show == 0) {
    lcd.setBacklight(255);
    lcd.home(); lcd.clear();
    lcd.print("Hello LCD");
    delay(1000);

    lcd.setBacklight(0);
    delay(400);
    lcd.setBacklight(255);

  } else if (show == 1) {
    lcd.clear();
    lcd.print("Cursor On");
    lcd.cursor();

  } else if (show == 2) {
    lcd.clear();
    lcd.print("Cursor Blink");
    lcd.blink();

  } else if (show == 3) {
    lcd.clear();
    lcd.print("Cursor OFF");
    lcd.noBlink();
    lcd.noCursor();

  } else if (show == 4) {
    lcd.clear();
    lcd.print("Display Off");
    lcd.noDisplay();

  } else if (show == 5) {
    lcd.clear();
    lcd.print("Display On");
    lcd.display();

  } else if (show == 7) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("*** first line.");
    lcd.setCursor(0, 1);
    lcd.print("*** second line.");

  } else if (show == 8) {
    lcd.scrollDisplayLeft();
  } else if (show == 9) {
    lcd.scrollDisplayLeft();
  } else if (show == 10) {
    lcd.scrollDisplayLeft();
  } else if (show == 11) {
    lcd.scrollDisplayRight();
  } // if

  delay(1000);
  show = (show + 1) % 12;
  */
}




