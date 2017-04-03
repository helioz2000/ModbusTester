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
#define VERSION_MINOR 1

#define MODBUS_RX_PIN 10      // Marked D10 on nano board
#define MODBUS_TX_PIN 11      // Marked D11 on nano board
#define MODBUS_TX_ENABLE_PIN  12  // Marked D12 on nano board
#define MODBUS_BAUDRATE 19200
#define MODBUS_RX_BUF_LEN 128
#define MODBUS_RX_TIMEOUT 1000
#define MODBUS_CHAR_TIMEOUT 100

#define POLL_TIME 1000

#define SERIAL_BAUDRATE 19200 // For console interface

#define LCD_I2C_ADDRESS 0x3F
#define LCD_LINES 4
#define LCD_CPL 20    // Characters per line

int txCount = 0;
int modbusSvrAddress = 12;
ModbusPacket testPacket;
unsigned char *modbusTxBuf;
unsigned char modbusRxBuf[MODBUS_RX_BUF_LEN];
int modbusTxLen;

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

void setup_serial() {
  // Open serial communications and wait for port to open:
  Serial.begin(SERIAL_BAUDRATE);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  debug("Modbus Tester V%d.%d\n", VERSION_MAJOR, VERSION_MINOR);
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
  lcd.setBacklight(255);
  lcd.home();
  lcd.clear();
}

void setup() {
  setup_serial();
  setup_LCD();
  setup_modbus();
}

void modbusWrite(unsigned char *txBuf, unsigned int txLen) { 

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
  
}

int modbusRead(unsigned int timeout) {
  unsigned char rx_byte;
  bool rx_has_started = false;
  unsigned int rx_count = 0;

  unsigned int rx_timeout = millis() + timeout;

  // wait for first byte
  while (millis() < rx_timeout) {
    if (modbusSerial.available()) {
      rx_byte = modbusSerial.read();
      rx_has_started = true;
      modbusRxBuf[rx_count++] = rx_byte;
      break;
    }
  }

  // has timeout occured?
  if (!rx_has_started)
    return -1;

  unsigned int char_timeout = millis() + MODBUS_CHAR_TIMEOUT;

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

void loop() {
  int i;
  int rxLen = 0;
  //mobusSerial.println("This the Modbus port speaking");

  lcd.setBacklight(1);

  delay(POLL_TIME);
  constructFrame(&testPacket);
  modbusTxLen = getFrame(&modbusTxBuf);

  lcd.home();
  lcd.print("Modbus: ");
  lcd.print(modbusSvrAddress);
  
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

  lcd.setCursor(0, 3);
  lcd.print(txCount);
  
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
    lcd.print("FAILED");
    lcd.setBacklight(0);
    delay(400);
    lcd.setBacklight(1);
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




