/*
  makeyMate.cpp
  by: Jim Lindblom
      SparkFun Electronics
  date: October 9, 2012
  license: This code is beerware: feel free to use it, with or without attribution,
  in your own projects. If you find it helpful, buy me a beer next time you see me
  at the local pub.
  
  Definition for makeyMateClass class.	
 */
#include "Arduino.h"	// Needed for delay
#include "makeyMate.h"
#include <SoftwareSerial.h>

SoftwareSerial bluetooth(14, 16);

makeyMateClass::makeyMateClass()
{
  for (int i=0; i<6; i++)
  {
    keyCodes[i] = 0x00;
  }
  modifiers = 0;
}

uint8_t makeyMateClass::begin(char * name)
{
  bluetooth.begin(9600);

  /* The next few lines of code try to create a fresh start. We need to get the RN-42
   into command mode, from any of these 3 states:
   	1) Connected - Sending 0 will disconnect the module, then we'll put it into command mode
   	2) Disconnected - In this case, we'll just get it into command mode
   	3) Disconnected, already in Command Mode - Sending 0 makes the module unresponsive
   	  until an undefined \r\n's are sent. I can't find any reason for this in
   	  the RN-42 User manual 
  */
  bluetooth.write((uint8_t) 0);	// Disconnects, if connected
  delay(1000);
  bluetooth.println("$$$");  // Command mode string
  while (!bluetooth.available())
    bluetooth.write('\r');
  delay(100);
  bluetooth.flush();
  
  while (!enterCommandMode())
    delay(100);
  delay(100);
  bluetooth.println("SA,1");	// enable authentication
  delay(100);
  bluetooth.println("SW,80A0");	// Deep sleep mode, 100ms sleeps - saves about 15mA when idling
  //bluetooth.println("SW,0000");  // No sleep mode, higher current
  delay(100);
  bluetooth.println("SQ,16");	// optimize for short bursts of data
  delay(100);
  bluetooth.println("SM,4"); // auto-connect (DTR) mode
  delay(100);
  bluetooth.println("ST,255");  // no config timer
  delay(100);
  bluetooth.println("S~,6");  //setHIDMode();
  delay(100);
  setKeyboardMouseMode();
  delay(100);
  setName(name);
  delay(100);
  reboot();
  delay(2000);
}

void makeyMateClass::moveMouse(uint8_t b, uint8_t x, uint8_t y)
{
  bluetooth.write(0xFD);
  bluetooth.write(5);  // length
  bluetooth.write(2);  // mouse
  bluetooth.write((byte) b);  // buttons
  bluetooth.write((byte) x);  // no x
  bluetooth.write((byte) y);  // no y
  bluetooth.write((byte) 0);  // no wheel
}

uint8_t makeyMateClass::keyPress(uint8_t k)
{
  uint8_t i;

  if (k >= 136)  // Non printing key
  {
    k = k - 136;
  }
  else if (k >= 128)  // !!! Modifiers, todo !!!
  {
  }
  else
  {
    k = asciiToScanCode[k];
    if (!k)
    {
      return 0;
    }

    if (k & 0x80)
    {
      modifiers |= 0x02;
      k &= 0x7F;
    }
  }

  if (keyCodes[0] != k && keyCodes[1] != k && 
    keyCodes[2] != k && keyCodes[3] != k &&
    keyCodes[4] != k && keyCodes[5] != k) {

    for (i=0; i<6; i++) {
      if (keyCodes[i] == 0x00) {
        keyCodes[i] = k;
        break;
      }
    }
    if (i == 6) {  // Too many characters to send
      /// !!! Return an error !!!
      return 0;
    }	
  }

  bluetooth.write(0xFE);	// Keyboard Shorthand Mode
  bluetooth.write(0x07);	// Length
  bluetooth.write(modifiers);	// Modifiers
  for (int j=0; j<6; j++)
  {
    bluetooth.write(keyCodes[j]);
  }

  return 1;
}

uint8_t makeyMateClass::keyRelease(uint8_t k)
{
  uint8_t i;

  if (k >= 136)  // Non printing key
  {
    k = k - 136;
  }
  else if (k >= 128)  // !!! Modifiers, todo !!!
  {
  }
  else
  {
    k = asciiToScanCode[k];
    if (!k)
    {
      return 0;
    }

    if (k & 0x80)
    {
      modifiers &= ~0x02;	// Clear the modifier
      k &= 0x7F;
    }
  }

  for (i=0; i<6; i++) 
  {
    if ((0 != k) && (keyCodes[i] == k))
    {
      keyCodes[i] = 0x00;
    }
  }

  bluetooth.write(0xFE);	// Keyboard Shorthand Mode
  bluetooth.write(0x07);	// Length
  bluetooth.write(modifiers);	// Modifiers
  for (int j=0; j<6; j++)
  {
    bluetooth.write(keyCodes[j]);
  }
  
  return 1;
}

uint8_t makeyMateClass::setName(char * name)
{
  if (bluetooth.available())
    bluetooth.flush();	// Get rid of any characters in the buffer, we'll need to check it fresh

    bluetooth.print("SN,");
  for (int i=0; i<15; i++)
  {
    if (name[i] != '\r')
      bluetooth.write(name[i]);
    else
      break;
  }
  bluetooth.write('\r');

  bluetoothReceive(rxBuffer);

  return bluetoothCheckReceive(rxBuffer, "AOK", 3);

}

uint8_t makeyMateClass::connect()
{
  while (!enterCommandMode())
    delay(100);
  delay(100);
  bluetooth.println("C");
}

uint8_t makeyMateClass::connect(char * address)
{
  if (enterCommandMode())
  {	
    bluetooth.print("C,");
    for (int i=0; i<8; i++)
      bluetooth.write(address[i]);
    bluetooth.write('\r');

    return 1;
  }
  else
    return 0;	
}

uint8_t makeyMateClass::reboot(void)
{
  if (bluetooth.available())
    bluetooth.flush();	// Get rid of any characters in the buffer, we'll need to check it fresh

    bluetooth.print("R,1");	// reboot command
  bluetooth.write('\r');

  bluetoothReceive(rxBuffer);

  return bluetoothCheckReceive(rxBuffer, "Reboot!", 7);
}

uint8_t makeyMateClass::bluetoothReceive(char * dest)
{
  int timeout = 1000;
  char c;
  int i = 0;

  while ((--timeout > 0) && (c != 0x0A))
  {
    if (bluetooth.available())
    {
      c = bluetooth.read();
      if (c != 0x0D)
        dest[i++] = c;
      timeout = 1000;	// reset timeout
    }
  }

  return timeout;
}

uint8_t makeyMateClass::bluetoothCheckReceive(char * src, char * expected, int bufferSize)
{
  int i = 0;
  char c;

  while ((src[i] != 0x0A) || (i < bufferSize))
  {
    if (src[i] != expected[i])
    {
      return 0;
    }
    i++;
  }

  return 1;
}

uint8_t makeyMateClass::setKeyboardMouseMode(void)
{	
  if (bluetooth.available())
    bluetooth.flush();	// Get rid of any characters in the buffer, we'll need to check it fresh

    bluetooth.print("sh,0030");
  bluetooth.write('\r');

  bluetoothReceive(rxBuffer);

  return bluetoothCheckReceive(rxBuffer, "AOK", 3);
}

uint8_t makeyMateClass::setHIDMode(void)
{
  if (bluetooth.available())
    bluetooth.flush();	// Get rid of any characters in the buffer, we'll need to check it fresh

    bluetooth.print("S~,6");  // Bluetooth HID Mode
  bluetooth.write('\r');

  bluetoothReceive(rxBuffer);

  return bluetoothCheckReceive(rxBuffer, "AOK", 3);
}

uint8_t makeyMateClass::enterCommandMode(void)
{	 	
  bluetooth.flush();  // Get rid of any characters in the buffer, we'll need to check it fresh
  bluetooth.println("$$$");  // Command mode string
  bluetoothReceive(rxBuffer);

  if (rxBuffer[0] == '?')
  {
    return 1;
  }
  else
  {
    return bluetoothCheckReceive(rxBuffer, "CMD", 3);
  }	
}

