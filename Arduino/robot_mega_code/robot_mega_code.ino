#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <SoftwareSerial.h>
#include <NewPing.h>


#define SEALEVELPRESSURE_HPA 1013.25
#define US_MAX_DISTANCE 200


/*
*  Variables
*/
// Motor Variables
struct MotorPins { uint8_t in1, in2, pwm; };
MotorPins motorNW = {26,28,5};
MotorPins motorNE = {30,32,2};
MotorPins motorSW = {22,24,4};
MotorPins motorSE = {34,36,3};


// Distance Sensor Variables
struct DisSensPins { uint8_t trig, echo; };
struct DisSensPinsPING { uint8_t ping; };
DisSensPinsPING uSensorEast = {43};
DisSensPins uSensorWest = {25,27};
DisSensPins uSensorNorth = {33,35};


NewPing eastSonar(uSensorEast.ping, uSensorEast.ping, US_MAX_DISTANCE);
NewPing westSonar(uSensorWest.trig, uSensorWest.echo, US_MAX_DISTANCE);
NewPing northSonar(uSensorNorth.trig, uSensorNorth.echo, US_MAX_DISTANCE);


// Temperature & Pressure Sensor Variables
Adafruit_BME280 bme;
LiquidCrystal_I2C lcd(0x27,16,2);
double currentTemp, currentPres, previousTemp, previousPres;


// Data Transfer Variables
const byte sensorStartMarker = 0x7E;
typedef struct SensorValues {
  uint8_t east;
  uint8_t west;
  uint8_t north;
} SensorValues;
SensorValues valuesToSend;


const byte commandStartMarker = 0xAC;
typedef struct Commands {
  uint8_t rotate; // 0 = no, 1 = cw, 2 = ccw
  uint8_t move; // 0 = no, 1 = N, 2 = E, 3 = S, 4 = W,
                // 5 = NE, 6 = NW, 7 = SE, 8 = SW
} Commands;
Commands commandsReceived;


bool newCommand = false;


// Time Management Variables
const int distanceInterval = 200;
unsigned long previousDistanceMillis = 0;
const int tempPresInterval = 500;
unsigned long previousTempPresMillis = 0;


// Movement Configuration Variables
const int robotMovementSpeed = 80;
const bool timeBasedMovement = false;
int robotMovementTimeLength = 400;
int robotRotationTimeLength = 475;


// Debug
const bool debugModeCommands = false;


/*
*  SETUP
*/
void setup(){
  pinMode(motorNW.in1, OUTPUT); pinMode(motorNW.in2, OUTPUT);
  pinMode(motorNE.in1, OUTPUT); pinMode(motorNE.in2, OUTPUT);
  pinMode(motorSW.in1, OUTPUT); pinMode(motorSW.in2, OUTPUT);
  pinMode(motorSE.in1, OUTPUT); pinMode(motorSE.in2, OUTPUT);


  pinMode(uSensorWest.trig, OUTPUT); pinMode(uSensorWest.echo, INPUT);
  pinMode(uSensorNorth.trig, OUTPUT); pinMode(uSensorNorth.echo, INPUT);


  lcd.init(); lcd.backlight();
  lcd.setCursor(0,0); lcd.print("Hello!");


  Serial.begin(19200);
  Serial1.begin(9600);


  if (debugModeCommands) { return;}


  if (!bme.begin(0x76)){
    lcd.setCursor(0,0); lcd.print("Couldn't find BME.");
    while(1);
  }


  getBMEValues();
  printBMEValuesToLCD();


  setRobotSpeed(robotMovementSpeed);
  if (!timeBasedMovement) { robotMovementTimeLength = -1; }
}


/*
*  LOOP
*/
void loop(){
  unsigned long currentMillis = millis();


  if (currentMillis - previousTempPresMillis >= tempPresInterval
      && !debugModeCommands){
    previousTempPresMillis = currentMillis;


    getBMEValues();
    printBMEValuesToLCD();
  }


  if (currentMillis - previousDistanceMillis >= distanceInterval){
    previousDistanceMillis = currentMillis;


    valuesToSend.east = eastSonar.ping() / US_ROUNDTRIP_CM;


    valuesToSend.west = westSonar.ping() / US_ROUNDTRIP_CM;
    pinMode(uSensorWest.echo, OUTPUT);
    digitalWrite(uSensorWest.echo, LOW);
    pinMode(uSensorWest.echo, INPUT);


    valuesToSend.north = northSonar.ping() / US_ROUNDTRIP_CM;
    pinMode(uSensorNorth.echo, OUTPUT);
    digitalWrite(uSensorNorth.echo, LOW);
    pinMode(uSensorNorth.echo, INPUT);


    //Serial.println("Sending values to ESP.");
    Serial1.write(sensorStartMarker);
    Serial1.write((byte*)&valuesToSend,sizeof(valuesToSend));
  }




  if (Serial1.available()){
    if (Serial1.read() == commandStartMarker){
      byte* ptr = (byte*)&commandsReceived;
      int bytesRead = Serial1.readBytes(ptr,sizeof(commandsReceived));
      if (bytesRead == sizeof(commandsReceived)) newCommand = true;
    }
  }


  if (newCommand){
    newCommand = false;
    if (debugModeCommands){
      lcd.setCursor(0,0); lcd.print("                ");
      lcd.setCursor(0,1); lcd.print("                ");
      lcd.setCursor(0,0);
    }


    switch(commandsReceived.rotate){
      case 0:
        if (debugModeCommands){ lcd.print("Don't Rotate"); }
        break;
      case 1:
        if (debugModeCommands){ lcd.print("Rotate CW"); }
        else { rotateRobot(true, robotRotationTimeLength); }
        break;
      case 2:
        if (debugModeCommands) { lcd.print("Rotate CCW"); }
        else { rotateRobot(false, robotRotationTimeLength); }
        break;
    }


    if (debugModeCommands) { lcd.setCursor(0,1); }
    switch(commandsReceived.move){
      case 0:
        if (debugModeCommands) { lcd.print("Don't Move"); }
        else { stopRobot(); }
        break;
      case 1:
        if (debugModeCommands) { lcd.print("Move N"); }
        else { moveRobot(true,false,false,false,robotMovementTimeLength); }
        break;
      case 2:
        if (debugModeCommands) { lcd.print("Move E"); }
        else { moveRobot(false,false,true,false,robotMovementTimeLength); }
        break;
      case 3:
        if (debugModeCommands) { lcd.print("Move S"); }
        else { moveRobot(false,true,false,false,robotMovementTimeLength); }
        break;
      case 4:
        if (debugModeCommands) { lcd.print("Move W"); }
        else { moveRobot(false,false,false,true,robotMovementTimeLength); }
        break;
      case 5:
        if (debugModeCommands) { lcd.print("Move NE"); }
        else { moveRobot(true,false,true,false,robotMovementTimeLength); }
        break;
      case 6:
        if (debugModeCommands) { lcd.print("Move NW"); }
        else { moveRobot(true,false,false,true,robotMovementTimeLength); }
        break;
      case 7:
        if (debugModeCommands) { lcd.print("Move SE"); }
        else { moveRobot(false,true,true,false,robotMovementTimeLength); }
        break;
      case 8:
        if (debugModeCommands) { lcd.print("Move SW"); }
        else { moveRobot(false,true,false,true,robotMovementTimeLength); }
        break;
    }
  }
}


/*
*  ADVANCED MOVEMENT METHODS
*/
void moveRobot(bool north,bool south,bool east,bool west,int ms){
  if (north){
    if (east){ moveMotorNE(0); moveMotorSE(1); moveMotorSW(0); moveMotorNW(1); }
    else if (west){ moveMotorNE(1); moveMotorSE(0); moveMotorSW(1); moveMotorNW(0); }
    else { moveMotorNE(1); moveMotorSE(1); moveMotorSW(1); moveMotorNW(1); }
  } else if (south){
    if (east){ moveMotorNE(2); moveMotorSE(0); moveMotorSW(2); moveMotorNW(0); }
    else if (west){ moveMotorNE(0); moveMotorSE(2); moveMotorSW(0); moveMotorNW(2); }
    else { moveMotorNE(2); moveMotorSE(2); moveMotorSW(2); moveMotorNW(2); }
  } else {
    if (east){ moveMotorNE(2); moveMotorSE(1); moveMotorSW(2); moveMotorNW(1); }
    else if (west){ moveMotorNE(1); moveMotorSE(2); moveMotorSW(1); moveMotorNW(2); }
  }


  if (ms != -1){
    delay(ms);
    stopRobot();
  }
}


void rotateRobot(bool clockwise,double ms){
  if(clockwise){ moveMotorNE(2); moveMotorSE(2); moveMotorSW(1); moveMotorNW(1); }
  else { moveMotorNE(1); moveMotorSE(1); moveMotorSW(2); moveMotorNW(2); }
  delay(ms); stopRobot();
}


void stopRobot(){ moveMotorNE(0); moveMotorNW(0); moveMotorSE(0); moveMotorSW(0); }




/*
*  BASIC MOVEMENT METHODS
*/
void moveMotorNE(int state){
  switch(state){
    case 0: digitalWrite(motorNE.in1,LOW); digitalWrite(motorNE.in2,LOW); break;
    case 1: digitalWrite(motorNE.in1,LOW); digitalWrite(motorNE.in2,HIGH); break;
    case 2: digitalWrite(motorNE.in1,HIGH); digitalWrite(motorNE.in2,LOW); break;
  }
}


void moveMotorNW(int state){
  switch(state){
    case 0: digitalWrite(motorNW.in1,LOW); digitalWrite(motorNW.in2,LOW); break;
    case 1: digitalWrite(motorNW.in1,HIGH); digitalWrite(motorNW.in2,LOW); break;
    case 2: digitalWrite(motorNW.in1,LOW); digitalWrite(motorNW.in2,HIGH); break;
  }
}


void moveMotorSE(int state){
  switch(state){
    case 0: digitalWrite(motorSE.in1,LOW); digitalWrite(motorSE.in2,LOW); break;
    case 1: digitalWrite(motorSE.in1,LOW); digitalWrite(motorSE.in2,HIGH); break;
    case 2: digitalWrite(motorSE.in1,HIGH); digitalWrite(motorSE.in2,LOW); break;
  }
}


void moveMotorSW(int state){
  switch(state){
    case 0: digitalWrite(motorSW.in1,LOW); digitalWrite(motorSW.in2,LOW); break;
    case 1: digitalWrite(motorSW.in1,HIGH); digitalWrite(motorSW.in2,LOW); break;
    case 2: digitalWrite(motorSW.in1,LOW); digitalWrite(motorSW.in2,HIGH); break;
  }
}


void setRobotSpeed(int speed){
  analogWrite(motorNE.pwm,speed);
  analogWrite(motorNW.pwm,speed);
  analogWrite(motorSE.pwm,speed);
  analogWrite(motorSW.pwm,speed);
}




/*
*  TEMPERATURE & PRESSURE SENSOR METHODS
*/
void getBMEValues(){
  currentTemp=(bme.readTemperature()*1.8)+32;
  currentPres=(bme.readPressure()/100.0F)*0.029529;
}


void printBMEValuesToLCD(){
  if(currentTemp==previousTemp && currentPres==previousPres)
  return;


  lcd.setCursor(0,0);
  lcd.print("                ");
  lcd.setCursor(0,1);
  lcd.print("                ");
  lcd.setCursor(0,0);


  lcd.print("T: ");
  lcd.print(currentTemp);
  lcd.print(" F");


  lcd.setCursor(0,1);
  lcd.print("P: ");
  lcd.print(currentPres);
  lcd.print(" inHg");


  previousTemp=currentTemp;
  previousPres=currentPres;
}