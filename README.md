# ModbusTester

Test instrument to check for Modbus RTU server presence.

This project includes the Eagle Cad circuit which shows the MAX485 chip and the Arduino Nano.
MAX485 is used to convert the TTL signal (from the Nano) to an RS485 signal for connection to a Modbus RTU device/network.

V0.4 - 6 April 2017
At this time we use a potentiometer as a User Interface to allow adjustment of the Modbus address. The baud rate is fixed to 19200 8N1 and Modbus function is fixed to 3 (Request Holding Reg) for one register at address 4100. 
