/*
  makeyMate.cpp
 by: Jim Lindblom
 SparkFun Electronics
 date: October 9, 2012
 license: This code is beerware: feel free to use it, with or without attribution,
 in your own projects. If you find it helpful, buy me a beer next time you see me
 at the local pub.
 
 Definition for makeyMateClass class. This class provides for communication 
 with the RN-42 HID module, and simplifies the sending of key presses, 
 releases, and mouse clicks and movements.
 */
#include "Arduino.h"	// Needed for delay
#include "makeyMate.h"
#include <SoftwareSerial.h>

// We'll use software serial to communicate with the bluetooth module
SoftwareSerial bluetooth(14, 16);

/* makeyMateClass constructor
 initializes the keyCodes variable and modifiers */
makeyMateClass::makeyMateClass()
{
  for (int i=0; i<6; i++)
  {
    keyCodes[i] = 0x00;
  }
  modifiers = 0;
}

/* begin(name)
   This function performs all tasks required to initialize the RN-42 HID module
   for use with the MaKey MaKey.
   We set up authentication, sleep, special config settings. As well as
   seting the auto-connect mode. The name can also be configured with the 
   *name* parameter. */
uint8_t makeyMateClass::begin(char * name)
{
  bluetooth.begin(9600);  // Initialize the software serial port at 9600
  freshStart();  // Get the module into a known mode, non-command mode, not connected

  while (!enterCommandMode())  // Enter command mode
      delay(100);    
  delay(BLUETOOTH_RESPONSE_DELAY);

  setAuthentication(1);  // enable authentication
  setName(name);  // Set the module name
  
  /* I'm torn on setting sleep mode. If you care about a low latency connection
   keep sleep mode set to 0000. You can save about 15mA of current consumption
   with the "80A0" setting, but I experience quite a bit more latency. */ 
  //setSleepMode("80A0");  // Deep sleep mode, 100ms sleeps - saves about 15mA when idling
  setSleepMode("0000");  // Low latency, high current usage ~ 40mA
  setSpecialConfig(16);  // optimize for low latency, short burst data
  
  /* These I wouldn't recommend changing. These settings are required for HID
     use and sending Keyboard and Mouse commands */
  setMode(4);  // auto-connect (DTR) mode
  setKeyboardMouseMode();  // bluetooth.println("SH,0030");
  
  
  // We must reboot if we're changing the mode to HID mode.
  // If you've already have the module in HID mode, this can be commented out
  if (!getHIDMode())
  {
    setHIDMode();  // Set RN-42 to HID profile mode
    reboot();
  }
  else
  {
    Serial.println("Already in HID mode!");
  }
}

uint8_t makeyMateClass::getHIDMode(void)
{
  bluetooth.flush();
  bluetooth.print("G~");
  bluetooth.write('\r');
  delay(BLUETOOTH_RESPONSE_DELAY);
  bluetoothReceive(rxBuffer);  // receive all response chars into rxBuffer

  return bluetoothCheckReceive(rxBuffer, "6", 1);	
}

uint8_t makeyMateClass::enterCommandMode(void)
{	 	
  bluetooth.flush();  // Get rid of any characters in the buffer, we'll need to check it fresh
  bluetooth.print("$$$");  // Command mode string
  bluetooth.write('\r');  // Will give us the ?, if we're already in command mode
  delay(BLUETOOTH_RESPONSE_DELAY);
  bluetoothReceive(rxBuffer);  // receive all response chars into rxBuffer

  if (rxBuffer[0] == '?')  // RN-42 will respond with ? if we're already in cmd
  {
    return 1;
  }
  else
  {
    return bluetoothCheckReceive(rxBuffer, "CMD", 3);
  }	
}

/* freshStart() attempts to get the module into a known state from 
 any of these 3 possible states:
 1) Connected - Sending 0 will disconnect the module, then we'll put 
 it into command mode
 2) Disconnected - In this case, we'll just get it into command mode
 3) Disconnected, already in Command Mode - Sending 0 makes the module
 unresponsive until an undefined \r\n's are sent. I can't find any 
 reason for this in the RN-42 User manual 
 */
void makeyMateClass::freshStart(void)
{
  int timeout = 1000;
  bluetooth.write((uint8_t) 0);	// Disconnects, if connected
  delay(BLUETOOTH_RESPONSE_DELAY);
  
  bluetooth.flush();  // delete buffer contents
  bluetooth.print("$$$");  // Command mode string
  do
  {
    bluetooth.write('\r');
    Serial.print("-");
  } while ((!bluetooth.available()) && (timeout-- > 0));
  while (bluetooth.available())
    Serial.write(bluetooth.read());
  delay(BLUETOOTH_RESPONSE_DELAY);
  bluetooth.flush();  // Flush the receive buffer
  
  bluetooth.print("---");
  bluetooth.write('\r');
  delay(BLUETOOTH_RESET_DELAY);
  bluetooth.flush();  // Flush the receive buffer
}

uint8_t makeyMateClass::setKeyboardMouseMode(void)
{	
  if (bluetooth.available())
    bluetooth.flush();	// Get rid of any characters in the buffer, we'll need to check it fresh

    bluetooth.print("SH,0030");
  bluetooth.write('\r');
  delay(BLUETOOTH_RESPONSE_DELAY);
  bluetoothReceive(rxBuffer);

  /* Double check the setting, output results in Serial monitor */
  bluetooth.flush();
  bluetooth.print("GH");
  bluetooth.write('\r');
  delay(BLUETOOTH_RESPONSE_DELAY);
  Serial.print("HID Mode set to: ");
  while (bluetooth.available())
    Serial.write(bluetooth.read());

  return bluetoothCheckReceive(rxBuffer, "AOK", 3);
}

uint8_t makeyMateClass::setHIDMode(void)
{
  if (bluetooth.available())
    bluetooth.flush();	// Get rid of any characters in the buffer, we'll need to check it fresh

    bluetooth.print("S~,6");  // Bluetooth HID Mode
  bluetooth.write('\r');
  delay(BLUETOOTH_RESPONSE_DELAY);
  bluetoothReceive(rxBuffer);

  /* Double check the setting, output results in Serial monitor */
  bluetooth.flush();
  bluetooth.print("G~");
  bluetooth.write('\r');
  delay(BLUETOOTH_RESPONSE_DELAY);
  Serial.print("Profile set to: ");
  while (bluetooth.available())
    Serial.write(bluetooth.read());

  return bluetoothCheckReceive(rxBuffer, "AOK", 3);
}

uint8_t makeyMateClass::setMode(uint8_t mode)
{
  if (bluetooth.available())
    bluetooth.flush();	// Get rid of any characters in the buffer, we'll need to check it fresh

    bluetooth.print("SM,");  // SA sets the authentication
  bluetooth.print(mode);  // Should print ASCII  vaule
  bluetooth.write('\r');

  delay(BLUETOOTH_RESPONSE_DELAY);  // Response will go to software serial buffer
  bluetoothReceive(rxBuffer);

  /* Double check the setting, output results in Serial monitor */
  bluetooth.flush();
  bluetooth.print("GM");
  bluetooth.write('\r');
  delay(BLUETOOTH_RESPONSE_DELAY);
  Serial.print("Mode set to: ");
  while (bluetooth.available())
    Serial.write(bluetooth.read());

  return bluetoothCheckReceive(rxBuffer, "AOK", 3);  
}

uint8_t makeyMateClass::setSpecialConfig(uint8_t num)
{
  if (bluetooth.available())
    bluetooth.flush();	// Get rid of any characters in the buffer, we'll need to check it fresh

    bluetooth.print("SQ,");  // SA sets the authentication
  bluetooth.print(num);  // Should print ASCII decimal vaule
  bluetooth.write('\r');

  delay(BLUETOOTH_RESPONSE_DELAY);  // Response will go to software serial buffer
  bluetoothReceive(rxBuffer);

  /* Double check the setting, output results in Serial monitor */
  bluetooth.flush();
  bluetooth.print("GQ");
  bluetooth.write('\r');
  delay(BLUETOOTH_RESPONSE_DELAY);
  Serial.print("Special Config set to: ");
  while (bluetooth.available())
    Serial.write(bluetooth.read());

  return bluetoothCheckReceive(rxBuffer, "AOK", 3);  
}

uint8_t makeyMateClass::setSleepMode(char * sleepConfig)
{
  if (bluetooth.available())
    bluetooth.flush();	// Get rid of any characters in the buffer, we'll need to check it fresh

    bluetooth.print("SW,");  // SA sets the authentication
  bluetooth.print(sleepConfig);  // Should print ASCII vaule
  bluetooth.write('\r');

  delay(BLUETOOTH_RESPONSE_DELAY);  // Response will go to software serial buffer
  bluetoothReceive(rxBuffer);

  /* Double check the setting, output results in Serial monitor */
  bluetooth.flush();
  bluetooth.print("GW");
  bluetooth.write('\r');
  delay(BLUETOOTH_RESPONSE_DELAY);
  Serial.print("Deep Sleep Mode set to: ");
  while (bluetooth.available())
    Serial.write(bluetooth.read());

  return bluetoothCheckReceive(rxBuffer, "AOK", 3);
}

uint8_t makeyMateClass::setAuthentication(uint8_t authMode)
{
  if (bluetooth.available())
    bluetooth.flush();	// Get rid of any characters in the buffer, we'll need to check it fresh

    bluetooth.print("SA,");  // SA sets the authentication
  bluetooth.print(authMode);  // Should print ASCII vaule
  bluetooth.write('\r');

  delay(BLUETOOTH_RESPONSE_DELAY);  // Response will go to software serial buffer
  bluetoothReceive(rxBuffer);

  /* Double check the setting, output results in Serial monitor */
  bluetooth.flush();
  bluetooth.print("GA");
  bluetooth.write('\r');
  delay(BLUETOOTH_RESPONSE_DELAY);
  Serial.print("Authentication Mode set to: ");
  while (bluetooth.available())
    Serial.write(bluetooth.read());

  return bluetoothCheckReceive(rxBuffer, "AOK", 3);
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
  freshStart();
  
  while (!enterCommandMode())
    delay(100);
  delay(100);

  bluetooth.flush();
  bluetooth.print("GR");
  bluetooth.write('\r');
  delay(BLUETOOTH_RESPONSE_DELAY);
  if (bluetooth.peek() == 'N')
  {
    Serial.println("Can't connect. No paired device!");
    bluetooth.flush();
    bluetooth.print("---");  // exit command mode
    bluetooth.write('\r');
    return 0;
  }
  else if (bluetooth.available() == 0)
  {
    Serial.println("ERROR!");
    return 0;
  }
  Serial.print("Attempting to connect to: ");
  while (bluetooth.available())
    Serial.write(bluetooth.read());

  bluetooth.print("C");  // The connect command
  bluetooth.write('\r');
  delay(BLUETOOTH_RESPONSE_DELAY);
  while (bluetooth.available())
    Serial.write(bluetooth.read());
  
  return 1;
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

  bluetooth.print("R,1");  // reboot command
  bluetooth.write('\r');

  bluetoothReceive(rxBuffer);

  delay(BLUETOOTH_RESET_DELAY);

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



