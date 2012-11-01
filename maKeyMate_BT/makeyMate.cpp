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
  setMode(0);  // slave mode, worth trying mode 4 (auto dtr) as well 
  
  /* I'm torn on setting sleep mode. If you care about a low latency connection
   keep sleep mode set to 0000. You can save about 15mA of current consumption
   with the "80A0" setting, but I experience quite a bit more latency. */ 
  //setSleepMode("80A0");  // Deep sleep mode, 100ms sleeps - saves about 15mA when idling
  setSleepMode("0000");  // Low latency, high current usage ~ 40mA
  setSpecialConfig(16);  // optimize for low latency, short burst data
  
  /* These I wouldn't recommend changing. These settings are required for HID
     use and sending Keyboard and Mouse commands */
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

/* enterCommandMode() will get the module into command mode if it's either
   in slow-STAT-blink mode, or already in command mode. Will not disconnect
   if the module is already connected.
   returns a 1 if command mode was successful, 0 otherwise */
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

/* This function returns a 1 if the RN-42 is already in HID mode
   The module MUST BE IN COMMAND MODE for this function to work! */
uint8_t makeyMateClass::getHIDMode(void)
{
  bluetooth.flush();
  bluetooth.print("G~");  // '~' is the RN-42's HID/SPP set command
  bluetooth.write('\r');
  delay(BLUETOOTH_RESPONSE_DELAY);
  bluetoothReceive(rxBuffer);  // receive all response chars into rxBuffer

  return bluetoothCheckReceive(rxBuffer, "6", 1);	
}

/* freshStart() attempts to get the module into a known state from 
 any of these 3 possible states:
 1) Connected - Sending 0 will disconnect the module, then we'll put 
 it into command mode
 2) Disconnected - In this case, we'll just get it into command mode
 3) Disconnected, already in Command Mode - Sending 0 makes the module
 unresponsive until an undefined \r\n's are sent. I can't find any 
 reason for this in the RN-42 User manual 
 
 Once this function has completed, the RN-42 should be disconnected, 
 in non-command mode */
void makeyMateClass::freshStart(void)
{
  int timeout = 1000;  // timeout, in the rare case the module is unresponsive
  bluetooth.write((uint8_t) 0);	// Disconnects, if connected
  delay(BLUETOOTH_RESPONSE_DELAY);
  
  bluetooth.flush();  // delete buffer contents
  bluetooth.print("$$$");  // Command mode string
  do // This gets the module out of state 3
  {  // continuously send \r until there is a response, usually '?'
    bluetooth.write('\r');  
    Serial.print("-");  // Debug info for how many \r's required to get a respsonse
  } while ((!bluetooth.available()) && (timeout-- > 0));
  
  while (bluetooth.available())
    Serial.write(bluetooth.read());
  delay(BLUETOOTH_RESPONSE_DELAY);
  bluetooth.flush();  // delay and flush the receive buffer
  
  bluetooth.print("---");  // exit command mode
  bluetooth.write('\r');
  delay(BLUETOOTH_RESET_DELAY);  // longer delay
  bluetooth.flush();  // Flush the receive buffer
}

/* This command will set the RN-42 HID output to Mouse/Keyboard combo mode */
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

/* This function will set the RN-42 into HID mode, from SPP mode.
   Requires a reboot to take effect! */
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

/* This sets the connect mode to one of the following:
   0: Slave Mode - Other bluetooth devices can discover and connect to this
   1: Master mode - This device initiates the connection. Non-discoverable.
   2: Trigger Master Mode - Device will automatically connect to pre-
      configured remote slave address when a character is received on UART.
   3: Auto-Connect (Master Mode) - Device will initiate a connection to
      the stored remote address immediately upon power up. If no address
      stored, inquiry is performed and first address found will try to be
      connected to.
   4: Auto-Connect (DTR Mode) - Like mode 3, but connect/disconnect are
      controlled by PIO6.
   5: Auto-connect (ANY mode) - Like mode 4, except PIO6 control performs an
      inquiry, the first device found is connected. Address is never stored. */
uint8_t makeyMateClass::setMode(uint8_t mode)
{
  if (bluetooth.available())
    bluetooth.flush();	// Get rid of any characters in the buffer, we'll need to check it fresh

    bluetooth.print("SM,");  // SM sets mode
  bluetooth.print(mode);  // Should automatically print ASCII  vaule
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

/* This function can send one of the 5 special configuration commands:
   0: Disable all special commands
   4: Disable reading values of GPIO3 and 6 on power-up.
   16: Configure firmware to optimize for low-latency transfers.
   128: Allow for fast reconnect.
   256: Set 2-stop bit mode on UART.
   
   Most of these are not recommended, but the low-latency is useful. */
uint8_t makeyMateClass::setSpecialConfig(uint8_t num)
{
  if (bluetooth.available())
    bluetooth.flush();	// Get rid of any characters in the buffer, we'll need to check it fresh

    bluetooth.print("SQ,");  // SQ sets special config
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

/* This function enables low power SNIFF mode. Send a 4-byte string as the 
   sleepConfig variable
   "0000" = disabled
   e.g.: "0050" = Wake up every 50ms
   "8xxx" = Enables deep sleep mode */
uint8_t makeyMateClass::setSleepMode(char * sleepConfig)
{
  if (bluetooth.available())
    bluetooth.flush();	// Get rid of any characters in the buffer, we'll need to check it fresh

    bluetooth.print("SW,");  // SW sets the sniff mode
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

/* This function enables or disables authentication (pincode pairing)
   Two options are available for Authmode:
   0: Disabled
   1: Enabled */
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

/* This function sets the name of an RN-42 module
   name should be an up to 20-character value. It MUST BE TERMINATED by a 
   \r character */
uint8_t makeyMateClass::setName(char * name)
{
  if (bluetooth.available())
    bluetooth.flush();	// Get rid of any characters in the buffer, we'll need to check it fresh

  bluetooth.print("SN,");
  for (int i=0; i<20; i++)
  {
    if (name[i] != '\r')
      bluetooth.write(name[i]);
    else
      break;
  }
  bluetooth.write('\r');
  
  delay(BLUETOOTH_RESPONSE_DELAY);
  bluetoothReceive(rxBuffer);

  /* Double check the setting, output results in Serial monitor */
  bluetooth.flush();
  bluetooth.print("GN");
  bluetooth.write('\r');
  delay(BLUETOOTH_RESPONSE_DELAY);
  Serial.print("Name set to: ");
  while (bluetooth.available())
    Serial.write(bluetooth.read());

  return bluetoothCheckReceive(rxBuffer, "AOK", 3);
}

/* This function sends the HID report for mouse movement and clicks
   You can send any of the mouse clicks in the first parameter (b)
   Mouse movement (horizontal and vertical) goes in the final two
   parameters (x and y). */
void makeyMateClass::moveMouse(uint8_t b, uint8_t x, uint8_t y)
{
  bluetooth.write(0xFD);  // Send a RAW report
  bluetooth.write(5);  // length
  bluetooth.write(2);  // indicates a Mouse raw report
  bluetooth.write((byte) b);  // buttons
  bluetooth.write((byte) x);  // x movement
  bluetooth.write((byte) y);  // y movement
  bluetooth.write((byte) 0);  // wheel movement NOT YET IMPLEMENTED
}

/* This function sends a key press down. An array of pressed keys is 
   generated and the RN-42 module is commanded to send a keyboard report.
   The k parameter should either be an HID usage value, or one of the key
   codes provided for in settings.h
   Does not release the key! */
uint8_t makeyMateClass::keyPress(uint8_t k)
{
  uint8_t i;

  if (k >= 136)  // Non printing key, these are listed in settings.h
  {
    k = k - 136;
  }
  else if (k >= 128)  // !!! Modifiers, NOT YET IMPLEMENTED
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
      modifiers |= 0x02;  // Adds the shift key to modifiers variable
      k &= 0x7F;  // k can only be a 7-bit value.
    }
  }

  /* generate the key report into the keyCodes array 
     we can send up to 6 key presses, first make sure k isn't already in there */
  if (keyCodes[0] != k && keyCodes[1] != k && 
    keyCodes[2] != k && keyCodes[3] != k &&
    keyCodes[4] != k && keyCodes[5] != k) 
  {

    for (i=0; i<6; i++) // Add k to the next available array index
    {
      if (keyCodes[i] == 0x00) 
      {
        keyCodes[i] = k;
        break;
      }
    }
    if (i == 6) // Too many characters to send
    {
      return 0;
    }	
  }

  bluetooth.write(0xFE);	// Keyboard Shorthand Mode
  bluetooth.write(0x07);	// Length
  bluetooth.write(modifiers);	// Modifiers
  for (int j=0; j<6; j++)
  {
    bluetooth.write(keyCodes[j]);  // up to six key codes, 0 is nothing
  }

  return 1;
}

/* This function releases a key press down. If it's there, k will be removed
   from the keyCodes array, then that new array is sent to the RN-42 which
   is commanded to send a keyboard report.
   The k parameter should either be an HID usage value, or one of the key
   codes provided for in settings.h */
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
      keyCodes[i] = 0x00;  // set the value that was k to 0
    }
  }
  /* send the new report: */
  bluetooth.write(0xFE);	// Keyboard Shorthand Mode
  bluetooth.write(0x07);	// Length
  bluetooth.write(modifiers);	// Modifiers
  for (int j=0; j<6; j++)
  {
    bluetooth.write(keyCodes[j]);  // 6 possible scan codes
  }

  return 1;
}

/* This function will attempt a connection to the stored remote address
   The first time you connect the the RN-42 HID, the master device will
   need to initiate the connection. The first time a connection is made
   the bluetooth address of the master device will be stored on the RN-42.
   If no remote address is stored, a connection will not be made. */
uint8_t makeyMateClass::connect()
{
  freshStart();  // Get the module disconnected, and out of command mode
  
  while (!enterCommandMode())
  {  // Enter command mode
    delay(BLUETOOTH_RESPONSE_DELAY);
  }
  delay(BLUETOOTH_RESPONSE_DELAY);
  bluetooth.flush();
  
  /* get the remote address and print it in the serial monitor */
  bluetooth.print("GR");  // Get the remote address
  bluetooth.write('\r');
  delay(BLUETOOTH_RESPONSE_DELAY);
  if (bluetooth.peek() == 'N')  // Might say "No remote address stored */
  {  // (bluetooth address is hex values only, so won'te start with 'N'.
    Serial.println("Can't connect. No paired device!");
    bluetooth.flush();
    bluetooth.print("---");  // exit command mode
    bluetooth.write('\r');
    return 0;  // No connect is attempted
  }
  else if (bluetooth.available() == 0)  
  { // If we can't communicate with the module at all, print error
    Serial.println("ERROR!");
    return 0;  // return error
  }
  /* otherwise print the address we're trying to connect to */
  Serial.print("Attempting to connect to: ");
  while (bluetooth.available())
    Serial.write(bluetooth.read());
    
  /* Attempt to connect */
  bluetooth.print("C");  // The connect command
  bluetooth.write('\r');
  delay(BLUETOOTH_RESPONSE_DELAY);
  while (bluetooth.available())
    Serial.write(bluetooth.read());  // Should print "TRYING"
  
  return 1;
}

/* This function issues the reboot command, and adds a lengthy delay to 
   give the RN-42 time to restart. */
uint8_t makeyMateClass::reboot(void)
{
  if (bluetooth.available())
    bluetooth.flush();	// Get rid of any characters in the buffer, we'll need to check it fresh

  bluetooth.print("R,1");  // reboot command
  bluetooth.write('\r');
  delay(BLUETOOTH_RESPONSE_DELAY);
  bluetoothReceive(rxBuffer);

  delay(BLUETOOTH_RESET_DELAY);

  return bluetoothCheckReceive(rxBuffer, "Reboot!", 7);
}

/* This function reads all available characters on the bluetooth RX line
   into the dest array. It'll exit on either timeout, or if the 0x0A 
   (new line) character is received. */
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

/* This function checks two strings against eachother. A 1 is returned if
   they're equal, a 0 otherwise. 
   This is used to verify the response from the RN-42 module. */
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



