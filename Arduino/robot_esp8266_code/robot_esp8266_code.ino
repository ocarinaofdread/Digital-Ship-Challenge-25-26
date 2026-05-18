#include <ESP8266WiFi.h>
#include <espnow.h>


/*
*  VARIABLES
*/
// Communication Variables
uint8_t broadcastAddress[] = {0x08, 0xF9, 0xE0, 0x4F, 0xBF, 0x0D};
const byte numChars = 32;
char receivedChars[numChars];


// Data Transfer Variables
const byte sensorStartMarker = 0x7E;
typedef struct SensorValues {
  uint8_t east, west, north;
} SensorValues;
SensorValues valuesReceived;
bool newData = false;


const byte commandStartMarker = 0xAC;
typedef struct Commands {
  uint8_t rotate;
  uint8_t move;
} Commands;
Commands commandsReceived;
int incomingRotate;
int incomingMove;


/*
*  SETUP
*/
void setup() {
  delay(2000);
  Serial.begin(9600);
  Serial.swap();
 
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();


  if (esp_now_init() != 0) return;


  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(OnDataSent);
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
  esp_now_register_recv_cb(OnDataRecv);
}


/*
*  LOOP
*/
void loop() {
  if (Serial.available() > 0){
    if (Serial.read() == sensorStartMarker){
      byte* ptr = (byte*)&valuesReceived;
      int bytesRead = Serial.readBytes(ptr, sizeof(valuesReceived));
      if (bytesRead == sizeof(valuesReceived)){
        newData = true;
      }
    }
  }


  if (newData){
    esp_now_send(broadcastAddress, (uint8_t *) &valuesReceived, sizeof(valuesReceived));
    newData = false;
  }
}


/*
*  COMMUNICATION CALLBACKS
*/
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  // preserved for future use if necessary
}


void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
  memcpy(&commandsReceived, incomingData, sizeof(commandsReceived));
  incomingRotate = commandsReceived.rotate;
  incomingMove = commandsReceived.move;


  Serial.write(commandStartMarker);
  Serial.write((byte *)&commandsReceived, sizeof(commandsReceived));
}