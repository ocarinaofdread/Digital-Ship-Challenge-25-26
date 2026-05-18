#include <ESP8266WiFi.h>
#include <espnow.h>
#include <SoftwareSerial.h>


/*
*  VARIABLES
*/
// Communication Variables
uint8_t broadcastAddress[] = {0x48, 0x3F, 0xDA, 0x8A, 0xA9, 0x35};
const byte numChars = 32;
char receivedChars[numChars];


// Data Transfer Variables
const byte sensorStartMarker = 0x7E;
typedef struct SensorValues {
  uint8_t east, west, north;
} SensorValues;
SensorValues sensorsReceived;
int incomingNorthValue;
int incomingEastValue;
int incomingWestValue;


const byte commandStartMarker = 0xAC;
typedef struct Commands {
  uint8_t rotate; // 0 = no, 1 = cw, 2 = ccw
  uint8_t move; // 0 = no, 1 = N, 2 = E, 3 = S, 4 = W
} Commands;
Commands commandsReceived;
Commands previousCommandsReceived;
bool newCommand = false;
unsigned long lastSend = 0;
const int sendInterval = 200;


// Debug Variables
const bool printIncomingValues = false;
const bool printSentData = false;


/*
*  SETUP
*/
void setup() {
  delay(2000);
 
  Serial.begin(9600);
  Serial.swap();
  while (Serial.available()) {
    Serial.read();
  }
  pinMode(0, INPUT);
  delay(100);  


  WiFi.mode(WIFI_STA);
  WiFi.disconnect();


  if (esp_now_init() != 0) {
    //Serial.println("Error initializing ESP-NOW");
    return;
  }


  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(OnDataSent);
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
  esp_now_register_recv_cb(OnDataRecv);
}


/*
*  LOOP
*/
void loop() {
  static byte buffer[sizeof(Commands)];
  static byte idx = 0;
  static boolean receivingPacket = false;


  while (Serial.available() > 0) {
    char c = Serial.read();


    if (c == commandStartMarker) {
      idx = 0;
      receivingPacket = true;
      continue;
    }


    if (receivingPacket) {
      buffer[idx++] = c;


      if (idx > sizeof(Commands)) {
        idx = 0;
        receivingPacket = false;
      }


      if (idx == sizeof(Commands)) {
        memcpy(&commandsReceived, buffer, sizeof(Commands));
        newCommand = true;
        receivingPacket = false;
      }
    }
  }


  if (newCommand && millis() - lastSend > sendInterval) {
    if (previousCommandsReceived.rotate == commandsReceived.rotate &&
        previousCommandsReceived.move == commandsReceived.move){
      return;
    }


    previousCommandsReceived = commandsReceived;
    if (printSentData){
      Serial.print("Sending new command: R/M | ");
      Serial.print(commandsReceived.rotate);
      Serial.print("/");
      Serial.println(commandsReceived.move);
    }
    esp_now_send(broadcastAddress, (uint8_t *)&commandsReceived, sizeof(commandsReceived));
    lastSend = millis();
    newCommand = false;
  }
}


/*
*  COMMUNICATION CALLBACKS
*/
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  // preserved for future use if necessary
}


void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
  memcpy(&sensorsReceived, incomingData, sizeof(sensorsReceived));
  // Serial.print("Bytes received: ");
  // Serial.println(len);
  if (incomingNorthValue == sensorsReceived.north &&
      incomingWestValue == sensorsReceived.west &&
      incomingEastValue == sensorsReceived.east){
        return;
  }
  incomingNorthValue = sensorsReceived.north;
  incomingWestValue = sensorsReceived.west;
  incomingEastValue = sensorsReceived.east;


  if (printIncomingValues){
    Serial.print("North: ");
    Serial.print(incomingNorthValue);
    Serial.print(" | East: ");
    Serial.print(incomingEastValue);
    Serial.print(" | West: ");
    Serial.println(incomingWestValue);
  }


  Serial.write(sensorStartMarker);
  Serial.write((byte*)&sensorsReceived, sizeof(sensorsReceived));
}