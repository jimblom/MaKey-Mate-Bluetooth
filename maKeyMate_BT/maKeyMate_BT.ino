/*
 ************************************************
 ********* MaKeY Mate Bluetooth *****************
 ************************************************
 
 /////////////////////////////////////////////////
 /////////////HOW TO EDIT THE KEYS ///////////////
 /////////////////////////////////////////////////
 - Edit keys in the settings.h file
 - that file should be open in a tab above (in Arduino IDE)
 - more instructions are in that file
 
 ////////////////////////////////////////////////
 //////// MaKeyMate FIRMWARE v1.1 ///////////////
 ////////////////////////////////////////////////
 based on MaKey MaKey v1.4.1 by: Eric Rosenbaum, Jay Silver, 
 and Jim Lindblom (MIT Media Lab & SparkFun Electronics)
 by Jim Lindblom (SparkFun Electronics)
 start date: 8/16/2012 
 current release: 10/5/2012
 */

////////////////////////
// DEFINED CONSTANTS////
////////////////////////

#define BUFFER_LENGTH    3     // 3 bytes gives us 24 samples
#define NUM_INPUTS       18    // 6 on the front + 12 on the back
//#define TARGET_LOOP_TIME 694   // (1/60 seconds) / 24 samples = 694 microseconds per sample 
//#define TARGET_LOOP_TIME 758  // (1/55 seconds) / 24 samples = 758 microseconds per sample 
#define TARGET_LOOP_TIME 744  // (1/56 seconds) / 24 samples = 744 microseconds per sample 

// id numbers for mouse movement inputs (used in settings.h)
#define MOUSE_MOVE_UP       -1 
#define MOUSE_MOVE_DOWN     -2
#define MOUSE_MOVE_LEFT     -3
#define MOUSE_MOVE_RIGHT    -4

#include "settings.h"
#include <SoftwareSerial.h>
#include "makeyMate.h"

/////////////////////////
// STRUCT ///////////////
/////////////////////////
typedef struct {
  byte pinNumber;
  int keyCode;
  byte measurementBuffer[BUFFER_LENGTH]; 
  boolean oldestMeasurement;
  byte bufferSum;
  boolean pressed;
  boolean prevPressed;
  boolean isMouseMotion;
  boolean isMouseButton;
  boolean isKey;
} 
MakeyMakeyInput;
MakeyMakeyInput inputs[NUM_INPUTS];

///////////////////////////////////
// VARIABLES //////////////////////
///////////////////////////////////
int bufferIndex = 0;
byte byteCounter = 0;
byte bitCounter = 0;
int mouseMovementCounter = 0; // for sending mouse movement events at a slower interval

int pressThreshold;
int releaseThreshold;
boolean inputChanged;

int mouseHoldCount[NUM_INPUTS]; // used to store mouse movement hold data

// Pin Numbers
// input pin numbers for kickstarter production board
const int pinNumbers[NUM_INPUTS] = 
{
  12, 8, 13, 15, 7, 6,     // top of makey makey board up, down, left, right, space, click
  5, 4, 3, 2, 1, 0,        // left side of female header, KEBYBOARD - w, a, s, d, f, g
  23, 22, 21, 20, 19, 18   // right side of female header, MOUSE - up, down, left, right, left click, right click
};

// input status LED pin numbers
const int inputLED_a = 9;
const int inputLED_b = 10;
const int inputLED_c = 11;
byte ledCycleCounter = 0;

// timing
int loopTime = 0;
int prevTime = 0;
int loopCounter = 0;

///////////////////////////
// FUNCTIONS //////////////
///////////////////////////
void initializeArduino();
void initializeInputs();
void updateMeasurementBuffers();
void updateBufferSums();
void updateBufferIndex();
void updateInputStates();
void sendMouseButtonEvents();
void sendMouseMovementEvents();
void addDelay();
void cycleLEDs();
void danceLeds();
void updateOutLEDs();

///////////////////////////
// Bluetooth Mate Stuff ///
///////////////////////////
makeyMateClass makeyMate;  // create a makeyMateClass object named makeyMate
// This defines the name of the Bluetooth Mate. This string
// can be up to 20 characters long, but it must be terminated
// with a \r character.
char makeyMateName[] = "MaKeyMate\r";

// button logger stuff:
const byte SEQUENCE_LENGTH = 6;
int buttonSequence[SEQUENCE_LENGTH] = {0, 0, 0, 0, 0, 0};
int sequenceIndex = 0;
int sequenceInterval = 1000;  // in ms
int resetSequence[SEQUENCE_LENGTH] = {0, 1, 2, 3, 4, 5};  // u->d->l->r->space->click

//////////////////////
// SETUP /////////////
//////////////////////

void setup() 
{
  initializeArduino();
  initializeInputs();
  danceLeds();
  
  makeyMate.begin(makeyMateName);  // Initialize the bluetooth mate
  makeyMate.connect();  // Attempt to connect to a stored remote address
}

////////////////////
// MAIN LOOP ///////
////////////////////
void loop() 
{
  updateMeasurementBuffers();  // Step 1: read inputs, update measurementBuffer
  updateBufferSums();  // Step 2: update bufferSum, remove old measruement, add new
  updateBufferIndex();  // Step 3: update bitCounter and byteCounter
  updateInputStates();  // Step 4: check/update pressed/released states, send button presses/releases
  sendMouseButtonEvents();  // Step 5: Send mouse button click/releases
  sendMouseMovementEvents(); // Step 6: Send mouse movement
  cycleLEDs();  // Step 7: Update U/D/L/R/Space/Click LEDs
  updateOutLEDs();  // Step 8: Update output LEDs (K/M)
  addDelay();
}


//////////////////////////
// INITIALIZE ARDUINO ////
/////// Setup ////////////
//////////////////////////
void initializeArduino() 
{
  /* Set up input pins 
   DEactivate the internal pull-ups, since we're using external resistors */
  for (int i=0; i<NUM_INPUTS; i++)
  {
    pinMode(pinNumbers[i], INPUT);
    digitalWrite(pinNumbers[i], LOW);
  }

  pinMode(inputLED_a, INPUT);
  pinMode(inputLED_b, INPUT);
  pinMode(inputLED_c, INPUT);
  digitalWrite(inputLED_a, LOW);
  digitalWrite(inputLED_b, LOW);
  digitalWrite(inputLED_c, LOW);
}

///////////////////////////
// INITIALIZE INPUTS //////
////// Setup //////////////
///////////////////////////
void initializeInputs() {

  float thresholdPerc = SWITCH_THRESHOLD_OFFSET_PERC;
  float thresholdCenterBias = SWITCH_THRESHOLD_CENTER_BIAS/50.0;
  float pressThresholdAmount = (BUFFER_LENGTH * 8) * (thresholdPerc / 100.0);
  float thresholdCenter = ( (BUFFER_LENGTH * 8) / 2.0 ) * (thresholdCenterBias);
  pressThreshold = int(thresholdCenter + pressThresholdAmount);
  releaseThreshold = int(thresholdCenter - pressThresholdAmount);

  for (int i=0; i<NUM_INPUTS; i++)
  {
    inputs[i].pinNumber = pinNumbers[i];
    inputs[i].keyCode = keyCodes[i];

    for (int j=0; j<BUFFER_LENGTH; j++)
    {
      inputs[i].measurementBuffer[j] = 0;
    }
    inputs[i].oldestMeasurement = 0;
    inputs[i].bufferSum = 0;

    inputs[i].pressed = false;
    inputs[i].prevPressed = false;

    inputs[i].isMouseMotion = false;
    inputs[i].isMouseButton = false;
    inputs[i].isKey = false;

    if (inputs[i].keyCode < 0)
    {
      inputs[i].isMouseMotion = true;
    } 
    else if ((inputs[i].keyCode == MOUSE_LEFT) || (inputs[i].keyCode == MOUSE_RIGHT))
    {
      inputs[i].isMouseButton = true;
    } 
    else
    {
      inputs[i].isKey = true;
    }

  }
}

////////////////////////////////
// UPDATE MEASUREMENT BUFFERS //
////// Loop: Step 1 ////////////
////////////////////////////////
void updateMeasurementBuffers() 
{
  for (int i=0; i<NUM_INPUTS; i++)
  {
    // store the oldest measurement, which is the one at the current index,
    // before we update it to the new one 
    // we use oldest measurement in updateBufferSums
    byte currentByte = inputs[i].measurementBuffer[byteCounter];
    inputs[i].oldestMeasurement = (currentByte >> bitCounter) & 0x01; 

    // make the new measurement
    boolean newMeasurement = digitalRead(inputs[i].pinNumber);

    // invert so that true means the switch is closed
    newMeasurement = !newMeasurement; 

    // store it    
    if (newMeasurement)
    {
      currentByte |= (1<<bitCounter);
    } 
    else
    {
      currentByte &= ~(1<<bitCounter);
    }
    inputs[i].measurementBuffer[byteCounter] = currentByte;
  }
}

///////////////////////////
// UPDATE BUFFER SUMS /////
//// Loop: Step 2 /////////
///////////////////////////
void updateBufferSums() 
{
  // the bufferSum is a running tally of the entire measurementBuffer
  // add the new measurement and subtract the old one
  for (int i=0; i<NUM_INPUTS; i++)
  {
    byte currentByte = inputs[i].measurementBuffer[byteCounter];
    boolean currentMeasurement = (currentByte >> bitCounter) & 0x01; 
    if (currentMeasurement)
    {
      inputs[i].bufferSum++;
    }
    if (inputs[i].oldestMeasurement)
    {
      inputs[i].bufferSum--;
    }
  }  
}

///////////////////////////
// UPDATE BUFFER INDEX ////
///// Loop: Step 3 ////////
///////////////////////////
void updateBufferIndex() 
{
  bitCounter++;
  if (bitCounter == 8)
  {
    bitCounter = 0;
    byteCounter++;
    if (byteCounter == BUFFER_LENGTH)
    {
      byteCounter = 0;
    }
  }
}

///////////////////////////
// UPDATE INPUT STATES ////
///// Loop: Step 4 ////////
///////////////////////////
void updateInputStates()
{
  char charArray[6] = {0, 0, 0, 0, 0, 0};
  int count = 0;
  
  inputChanged = false;
  for (int i=0; i<NUM_INPUTS; i++) 
  {
    inputs[i].prevPressed = inputs[i].pressed; // store previous pressed state (only used for mouse buttons)
    if (inputs[i].pressed)  // if it was _previously_ pressed
    {
// Pressed -> Released
      if (inputs[i].bufferSum < releaseThreshold)
      {  
        inputChanged = true;
        inputs[i].pressed = false;
        if (inputs[i].isKey) 
        {
          makeyMate.keyRelease(inputs[i].keyCode);
        }
        if (inputs[i].isMouseMotion) 
        {  
          mouseHoldCount[i] = 0;  // input becomes released, reset mouse hold
        }
      }
// Pressed -> Pressed
      else if (inputs[i].isMouseMotion) 
      {  
        mouseHoldCount[i]++; // input remains pressed, increment mouse hold
      }
      else
      {
        makeyMate.keyPress(inputs[i].keyCode);
      }
    }
// Released -> Pressed
    else if (!inputs[i].pressed)
    {
      if (inputs[i].bufferSum > pressThreshold) // input becomes pressed
      {
        inputChanged = true;
        inputs[i].pressed = true; 
        checkSequence(i, resetSequence);  // Run the new key through reset sequence check
        if (inputs[i].isKey)
        {
          makeyMate.keyPress(inputs[i].keyCode);
        }
      }
    }
  }
}

//////////////////////////////
// SEND MOUSE BUTTON EVENTS //
///// Loop: Step 5 ///////////
//////////////////////////////
void sendMouseButtonEvents()
{
  if (inputChanged) {
    for (int i=0; i<NUM_INPUTS; i++)
    {
      if (inputs[i].isMouseButton)
      {
        if (inputs[i].pressed)
        {
          if (inputs[i].keyCode == MOUSE_LEFT)
          {
            makeyMate.moveMouse(MOUSE_LEFT, 0, 0);
          } 
          if (inputs[i].keyCode == MOUSE_RIGHT)
          {
            makeyMate.moveMouse(MOUSE_RIGHT, 0, 0);
          } 
        } 
        else if (inputs[i].prevPressed)
        {
          if (inputs[i].keyCode == MOUSE_LEFT)
          {
            makeyMate.moveMouse(0, 0, 0);
          } 
          if (inputs[i].keyCode == MOUSE_RIGHT)
          {
            makeyMate.moveMouse(0, 0, 0);
          }           
        }
      }
    }
  }
}

/////////////////////////////////
// SEND MOUSE MOVEMENT EVENTS ///
///// Loop: Step 6 //////////////
/////////////////////////////////
void sendMouseMovementEvents()
{
  byte right = 0;
  byte left = 0;
  byte down = 0;
  byte up = 0;
  byte horizmotion = 0;
  byte vertmotion = 0;

  mouseMovementCounter++;
  mouseMovementCounter %= MOUSE_MOTION_UPDATE_INTERVAL;
  if (mouseMovementCounter == 0)
  {
    for (int i=0; i<NUM_INPUTS; i++)
    {

      if (inputs[i].isMouseMotion)
      {
        if (inputs[i].pressed)
        {
          if (inputs[i].keyCode == MOUSE_MOVE_UP)
          {
            up=constrain(1+mouseHoldCount[i]/MOUSE_RAMP_SCALE, 1, MOUSE_MAX_PIXELS);
          }  
          if (inputs[i].keyCode == MOUSE_MOVE_DOWN)
          {
            down=constrain(1+mouseHoldCount[i]/MOUSE_RAMP_SCALE, 1, MOUSE_MAX_PIXELS);
          }  
          if (inputs[i].keyCode == MOUSE_MOVE_LEFT)
          {
            left=constrain(1+mouseHoldCount[i]/MOUSE_RAMP_SCALE, 1, MOUSE_MAX_PIXELS);
          }  
          if (inputs[i].keyCode == MOUSE_MOVE_RIGHT)
          {
            right=constrain(1+mouseHoldCount[i]/MOUSE_RAMP_SCALE, 1, MOUSE_MAX_PIXELS);
          }  
        }
      }
    }

    // diagonal scrolling and left/right cancellation
    if(left > 0)
    {
      if(right > 0)
      {
        horizmotion = 0; // cancel horizontal motion because left and right are both pushed
      }
      else
      {
        horizmotion = -left; // left yes, right no
      }
    }
    else
    {
      if(right > 0)
      {
        horizmotion = right; // right yes, left no
      }
    }

    if(down > 0)
    {
      if(up > 0)
      {
        vertmotion = 0; // cancel vertical motion because up and down are both pushed
      }
      else
      {
        vertmotion = down; // down yes, up no
      }
    }
    else
    {
      if (up > 0)
      {
        vertmotion = -up; // up yes, down no
      }
    }
    // now move the mouse
    if( !((horizmotion == 0) && (vertmotion==0)) )
    {
      makeyMate.moveMouse(0, horizmotion * PIXELS_PER_MOUSE_STEP, vertmotion * PIXELS_PER_MOUSE_STEP);
    }
  }
}

///////////////////////////
// ADD DELAY //////////////
///// Loop: Step 9 ////////
///////////////////////////
void addDelay() {

  loopTime = micros() - prevTime;
  
  if (loopTime < TARGET_LOOP_TIME)
  {
    int wait = TARGET_LOOP_TIME - loopTime;
    delayMicroseconds(wait);
  }

  prevTime = micros();
}

///////////////////////////
// CYCLE LEDS /////////////
/// Loop: Step 7 //////////
///////////////////////////
void cycleLEDs() {
  pinMode(inputLED_a, INPUT);
  pinMode(inputLED_b, INPUT);
  pinMode(inputLED_c, INPUT);
  digitalWrite(inputLED_a, LOW);
  digitalWrite(inputLED_b, LOW);
  digitalWrite(inputLED_c, LOW);

  ledCycleCounter++;
  ledCycleCounter %= 6;

  if ((ledCycleCounter == 0) && inputs[0].pressed) {
    pinMode(inputLED_a, INPUT);
    digitalWrite(inputLED_a, HIGH);
    pinMode(inputLED_b, OUTPUT);
    digitalWrite(inputLED_b, HIGH);
    pinMode(inputLED_c, OUTPUT);
    digitalWrite(inputLED_c, LOW);
  }
  if ((ledCycleCounter == 1) && inputs[1].pressed) {
    pinMode(inputLED_a, OUTPUT);
    digitalWrite(inputLED_a, HIGH);
    pinMode(inputLED_b, OUTPUT);
    digitalWrite(inputLED_b, LOW);
    pinMode(inputLED_c, INPUT);
    digitalWrite(inputLED_c, LOW);

  }
  if ((ledCycleCounter == 2) && inputs[2].pressed) {
    pinMode(inputLED_a, OUTPUT);
    digitalWrite(inputLED_a, LOW);
    pinMode(inputLED_b, OUTPUT);
    digitalWrite(inputLED_b, HIGH);
    pinMode(inputLED_c, INPUT);
    digitalWrite(inputLED_c, LOW);
  }
  if ((ledCycleCounter == 3) && inputs[3].pressed) {
    pinMode(inputLED_a, INPUT);
    digitalWrite(inputLED_a, LOW);
    pinMode(inputLED_b, OUTPUT);
    digitalWrite(inputLED_b, LOW);
    pinMode(inputLED_c, OUTPUT);
    digitalWrite(inputLED_c, HIGH);
  }
  if ((ledCycleCounter == 4) && inputs[4].pressed) {
    pinMode(inputLED_a, OUTPUT);
    digitalWrite(inputLED_a, LOW);
    pinMode(inputLED_b, INPUT);
    digitalWrite(inputLED_b, LOW);
    pinMode(inputLED_c, OUTPUT);
    digitalWrite(inputLED_c, HIGH);
  }
  if ((ledCycleCounter == 5) && inputs[5].pressed) {
    pinMode(inputLED_a, OUTPUT);
    digitalWrite(inputLED_a, HIGH);
    pinMode(inputLED_b, INPUT);
    digitalWrite(inputLED_b, LOW);
    pinMode(inputLED_c, OUTPUT);
    digitalWrite(inputLED_c, LOW);
  }

}

/////////////////////////
//// updateOutLEDs //////
//// Loop: Step 8 ///////
/////////////////////////

void updateOutLEDs()
{
  boolean keyPressed = 0;
  boolean mousePressed = 0;

  for (int i=0; i<NUM_INPUTS; i++)
  {
    if (inputs[i].pressed)
    {
      if (inputs[i].isKey)
      {
        keyPressed = 1;
      }
      else
      {
        mousePressed = 1;
      }
    }
  }

  if (keyPressed)
  {
    TXLED1;
  }
  else
  {
    TXLED0;
  }

  if (mousePressed)
  {
    RXLED1;
  }
  else
  {
    RXLED0;
  }
}

///////////////////////////
// DANCE LEDS  ////////////
/// Setup /////////////////
///////////////////////////
void danceLeds()
{
  int delayTime = 50;
  int delayTime2 = 100;

  // CIRCLE
  for(int i=0; i<4; i++)
  {
    // UP
    pinMode(inputLED_a, INPUT);
    digitalWrite(inputLED_a, HIGH);
    pinMode(inputLED_b, OUTPUT);
    digitalWrite(inputLED_b, HIGH);
    pinMode(inputLED_c, OUTPUT);
    digitalWrite(inputLED_c, LOW);
    delay(delayTime);

    //RIGHT
    pinMode(inputLED_a, INPUT);
    digitalWrite(inputLED_a, LOW);
    pinMode(inputLED_b, OUTPUT);
    digitalWrite(inputLED_b, LOW);
    pinMode(inputLED_c, OUTPUT);
    digitalWrite(inputLED_c, HIGH);
    delay(delayTime);


    // DOWN
    pinMode(inputLED_a, OUTPUT);
    digitalWrite(inputLED_a, HIGH);
    pinMode(inputLED_b, OUTPUT);
    digitalWrite(inputLED_b, LOW);
    pinMode(inputLED_c, INPUT);
    digitalWrite(inputLED_c, LOW);
    delay(delayTime);

    // LEFT
    pinMode(inputLED_a, OUTPUT);
    digitalWrite(inputLED_a, LOW);
    pinMode(inputLED_b, OUTPUT);    
    digitalWrite(inputLED_b, HIGH);
    pinMode(inputLED_c, INPUT);
    digitalWrite(inputLED_c, LOW);
    delay(delayTime);    
  }    

  // WIGGLE
  for(int i=0; i<4; i++)
  {
    // SPACE
    pinMode(inputLED_a, OUTPUT);
    digitalWrite(inputLED_a, HIGH);
    pinMode(inputLED_b, INPUT);
    digitalWrite(inputLED_b, LOW);
    pinMode(inputLED_c, OUTPUT);
    digitalWrite(inputLED_c, LOW);
    delay(delayTime2);    

    // CLICK
    pinMode(inputLED_a, OUTPUT);
    digitalWrite(inputLED_a, LOW);
    pinMode(inputLED_b, INPUT);
    digitalWrite(inputLED_b, LOW);
    pinMode(inputLED_c, OUTPUT);
    digitalWrite(inputLED_c, HIGH);
    delay(delayTime2);    
  }
}

// This function checks if a recent key press is part of an
// expected sequence of button presses. If the expected
// sequence is received, we attempt to connect to a stored
// remote address. By default the expected sequence is:
// Up -> Down -> Left -> Right -> Space -> Click
void checkSequence(int pressedKey, int * expectedSequence)
{
  buttonSequence[sequenceIndex] = pressedKey;
  
  for (int i=0; i<sequenceIndex + 1; i++)
  {
    if (buttonSequence[i] != expectedSequence[i])  // If not the correct sequence
    {
      sequenceIndex = 0;  // reset sequence index, start over
      return;  // get outta here
    }
  }
  sequenceIndex = (sequenceIndex++) % SEQUENCE_LENGTH;
  if (sequenceIndex == SEQUENCE_LENGTH)
  {
    danceLeds();  // good to indicate sequence was reeived
    makeyMate.connect();  // attempt to connect
    sequenceIndex = 0;
  }
}




