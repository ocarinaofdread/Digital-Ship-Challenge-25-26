#include <SoftwareSerial.h>


/*
*  VARIABLES
*/
const int leds[5] = {4, 5, 6, 7, 8};
SoftwareSerial ESPSerial(10, 9);


// LED Variables
int currentLED = 0;
typedef struct LED {
  const int pin;
  int state;
  unsigned long milliseconds;
  unsigned long interval;
} LED;
const int maxBlinkDistance = 40;
LED westLED = {5, LOW, 0, 0};
LED northLED = {6, LOW, 0, 0};
LED eastLED = {7, LOW, 0, 0};
const int leftRotatePin = 4;
const int rightRotatePin = 8;


// Buzzer Variables
const int buzzer = A2;
bool buzzerState;
const int toneFrequency = 1000;
const int minBuzzerDistance = 5; // cm


// Rotary Encoder Variables
const int encoderCLK = 2;
const int encoderDT = 3;
int lastCLK;
int lastDT;
unsigned long lastEncoderRotateRight = 0;
unsigned long lastEncoderRotateLeft = 0;
unsigned long encoderBlinkTime = 200;
bool justRotatedRight;
bool justRotatedLeft;


// Joystick Variables
const int joystickX = A1;
const int joystickY = A0;
unsigned long lastJoyMove = 0;
const int joyDelay = 150;


// Data Transfer Variables
const byte sensorStartMarker = 0x7E;
typedef struct SensorValues {
  uint8_t east;
  uint8_t west;
  uint8_t north;
} SensorValues;
SensorValues receivedValues;
bool newData = false;
unsigned long millisSinceLastNewData = 0;
unsigned long lackOfNewDataReset = 5000;


const byte commandStartMarker = 0xAC;
typedef struct Commands {
  uint8_t rotate; // 0 = no, 1 = cw, 2 = ccw
  uint8_t move; // 0 = no, 1 = N, 2 = E, 3 = S, 4 = W,
                // 5 = NE, 6 = NW, 7 = SE, 8 = SW
} Commands;
Commands commandsToSend;
Commands previousCommandsToSend;
bool justSent = false;


// Distance Visualization Variables
unsigned long previousDisPrintMillis = 0;
unsigned long disPrintInterval = 500;


// Debug Variables
const bool debugPrintStatements = false;
const bool debugPrintLEDStates = false;
unsigned long previousDebugMillis = 0;
unsigned long intervalPrintLED = 500;


/*
*  SETUP
*/
void setup() {
  ESPSerial.begin(9600);
  delay(3000);


  Serial.begin(19200);


  pinMode(encoderCLK, INPUT_PULLUP);
  pinMode(encoderDT, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);


  for (int i = 0; i < 5; i++) {
    pinMode(leds[i], OUTPUT);
    digitalWrite(leds[i], LOW);
  }


  lastCLK = digitalRead(encoderCLK);
  lastDT  = digitalRead(encoderDT);
}


/*
*  LOOP
*/
void loop() {
  if (ESPSerial.available() > 0){
    if (justSent) {
      for (int i = 0; i < sizeof(commandsToSend); i++){
          if (ESPSerial.available()) ESPSerial.read();
      }
      justSent = false;
    }
    else if (ESPSerial.read() == sensorStartMarker){
      byte* ptr = (byte*)&receivedValues;
      int bytesRead = ESPSerial.readBytes(ptr, sizeof(receivedValues));
      if (bytesRead == sizeof(receivedValues)){
        newData = true;
      }
    }
  }


  if (newData){
    millisSinceLastNewData = millis();


    if (debugPrintStatements) { Serial.println("Received data."); }


    westLED.interval = getNewInterval("[W] ", receivedValues.west);
    northLED.interval = getNewInterval("[N] ", receivedValues.north);
    eastLED.interval = getNewInterval("[E] ", receivedValues.east);
    newData = false;
  }


  handleEncoder();
  handleJoystick();
  handleLEDs();
  handleBuzzer();
}


/*
*  HANDLER VARIABLES
*/
void handleEncoder() {
  int rotateCommand = 0;


  int currentCLK = digitalRead(encoderCLK);
  if (lastCLK == HIGH && currentCLK == LOW) {
    if (digitalRead(encoderDT) == HIGH) {
      rotateCommand = 1;
      digitalWrite(rightRotatePin, HIGH);
      justRotatedRight = true;
      lastEncoderRotateRight = millis();
    } else {
      rotateCommand = 2;
      digitalWrite(leftRotatePin, HIGH);
      justRotatedLeft = true;
      lastEncoderRotateLeft = millis();
    }
  }


  if (millis() - lastEncoderRotateRight >= encoderBlinkTime &&
      justRotatedRight){
    justRotatedRight = false;
    digitalWrite(rightRotatePin, LOW);
  }
  if (millis() - lastEncoderRotateLeft >= encoderBlinkTime &&
      justRotatedLeft){
    justRotatedLeft = false;
    digitalWrite(leftRotatePin, LOW);
  }


  if(isNewCommand(rotateCommand, 0, true)){
    if (debugPrintStatements){
      Serial.print("[R] New command: ");
      Serial.print(rotateCommandMessage(rotateCommand));
      Serial.print(" differs from ");
      Serial.println(rotateCommandMessage(previousCommandsToSend.rotate));
    }


    commandsToSend.rotate = rotateCommand;
    commandsToSend.move = 0;
    ESPSerial.write(commandStartMarker);
    ESPSerial.write((byte*)&commandsToSend, sizeof(commandsToSend));
    justSent = true;
   
    previousCommandsToSend.rotate = commandsToSend.rotate;
    previousCommandsToSend.move = commandsToSend.move;
    commandsToSend.rotate = 0;
    commandsToSend.move = 0;
  }


  lastCLK = currentCLK;
}


void handleJoystick() {
  if (millis() - lastJoyMove > joyDelay) {
    int joyXValue = analogRead(joystickX);
    int joyYValue = analogRead(joystickY);


    int moveCommand = 0;
    if (joyYValue > 700) {
      moveCommand = 1; // N
    }
    else if (joyYValue < 300){
      moveCommand = 3; // S
    }
   
    if (joyXValue > 700) {
      if (moveCommand == 1){ moveCommand = 5; } // NE
      else if (moveCommand == 3){ moveCommand = 7; } // SE
      else{ moveCommand = 2; } // E
    }
    else if (joyXValue < 300) {
      if (moveCommand == 1){ moveCommand = 6; } // NW
      else if (moveCommand == 3){ moveCommand = 8; } // SW
      else{ moveCommand = 4; } // W
    }


    if (isNewCommand(0, moveCommand, false)){
      if (debugPrintStatements){
        Serial.print("[M] New command: ");
        Serial.print(moveCommandMessage(moveCommand));
        Serial.print(" differs from ");
        Serial.println(moveCommandMessage(previousCommandsToSend.move));
      }


      commandsToSend.rotate = 0;
      commandsToSend.move = moveCommand;
      ESPSerial.write(commandStartMarker);
      ESPSerial.write((byte*)&commandsToSend, sizeof(commandsToSend));
      justSent = true;


      previousCommandsToSend.rotate = commandsToSend.rotate;
      previousCommandsToSend.move = commandsToSend.move;
      commandsToSend.rotate = 0;
      commandsToSend.move = 0;
    }


    lastJoyMove = millis();
  }
}


void handleLEDs(){
  handleSpecificLED(westLED);
  handleSpecificLED(northLED);
  handleSpecificLED(eastLED);


  double milliseconds = millis();


  if (milliseconds - previousDebugMillis >= intervalPrintLED &&
      debugPrintLEDStates){
    previousDebugMillis = millis();
    debugPrintLEDState("[W] ", westLED);
    debugPrintLEDState("[N] ", northLED);
    debugPrintLEDState("[E] ", eastLED);
    Serial.println();
  }


  if (milliseconds - previousDisPrintMillis >= disPrintInterval){
    previousDisPrintMillis = milliseconds;
    printLEDDistances();
  }


  if (milliseconds - millisSinceLastNewData >= lackOfNewDataReset){
    millisSinceLastNewData = milliseconds;
    westLED.interval = 0;
    northLED.interval = 0;
    eastLED.interval = 0;
    receivedValues.west = 0;
    receivedValues.north = 0;
    receivedValues.east = 0;
   
    setLostConnectionAnimation();
  }
}


void handleSpecificLED(LED &led){
  if (led.interval < 0.1){
    led.milliseconds = millis();
    led.state = LOW;
    digitalWrite(led.pin, LOW);
  }
  else if (millis() - led.milliseconds >= led.interval){
    led.milliseconds = millis();


    if (led.state == HIGH){
      led.state = LOW;
    }
    else {
      led.state = HIGH;
    }
    digitalWrite(led.pin, led.state);
  }
}


void handleBuzzer(){
  bool tooClose = (receivedValues.west >= 1 && receivedValues.west <= minBuzzerDistance) ||
                  (receivedValues.north >= 1 && receivedValues.north <= minBuzzerDistance) ||
                  (receivedValues.east >= 1 && receivedValues.east <= minBuzzerDistance);
  if (tooClose &&
      !buzzerState){
    buzzerState = true;
    tone(BUZZER, toneFrequency);
  }
  else if (!tooClose &&
           buzzerState){
    buzzerState = false;
    noTone(BUZZER);
  }
}


/*
*  MISCELLANEOUS METHODS
*/
void setLostConnectionAnimation(){
  digitalWrite(westLED.pin, HIGH);
  digitalWrite(northLED.pin, HIGH);
  digitalWrite(eastLED.pin, HIGH);
  delay(250);
  digitalWrite(westLED.pin, LOW);
  digitalWrite(northLED.pin, LOW);
  digitalWrite(eastLED.pin, LOW);
  delay(250);


  digitalWrite(westLED.pin, HIGH);
  digitalWrite(northLED.pin, HIGH);
  digitalWrite(eastLED.pin, HIGH);
  delay(250);
  digitalWrite(westLED.pin, LOW);
  digitalWrite(northLED.pin, LOW);
  digitalWrite(eastLED.pin, LOW);
  delay(250);
}


unsigned long getNewInterval(String start, int distance){
  if (distance > maxBlinkDistance){
    return 0;
  }


  float c = 12.25 / 12.0;
  float d = distance;
  double seconds = (pow(c, d)) - 1;


  return seconds * 1000; // milliseconds
}


/*
*  MAP VISUALIZATION METHODS
*/
void printLEDDistances(){
  Serial.print(receivedValues.west);
  Serial.print("x");
  Serial.print(receivedValues.north);
  Serial.print("x");
  Serial.println(receivedValues.east);
}


/*
*  DATA TRANSFER METHODS
*/
bool isNewCommand(int rotate, int move, bool intendedForRotate){
  if (previousCommandsToSend.move == move &&
      !intendedForRotate){
    return false;
  }
  if (previousCommandsToSend.rotate == rotate &&
      intendedForRotate){
    return false;
  }


  return true;
}


/*
*  DEBUG METHODS
*/
void debugPrintLEDState(String start, LED led){
  Serial.print(start);
  Serial.print("State: ");
  Serial.print(led.state);
  Serial.print(" | millis: ");
  Serial.print(led.milliseconds);
  Serial.print(" | Interval: ");
  Serial.print(led.interval);
  Serial.print(" ");
}


String rotateCommandMessage(int rotate){
  String message = "";
  switch (rotate){
    case 0:
      message = "Don't Rotate";
      break;
    case 1:
      message = "Rotate Right";
      break;
    case 2:
      message = "Rotate Left";
      break;
  }


  return message;
}


String moveCommandMessage(int move){
  String message = "";
  switch (move){
    case 0:
      message = "Don't Move";
      break;
    case 1:
      message = "Move Forward";
      break;
    case 2:
      message = "Move Right";
      break;
    case 3:
      message = "Move Backward";
      break;
    case 4:
      message = "Move Left";
      break;
  }


  return message;
}