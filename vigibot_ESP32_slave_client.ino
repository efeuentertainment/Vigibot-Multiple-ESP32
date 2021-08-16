/*
  Author:   firened
  File:     vigibot_ESP32_slave_client.ino
  Version:
  V1.0      10.2020
  V1.2      12.2020
  V2.0      07.2021

  Description:
  Vigibot Pi to Multiple ESP32 over WLAN, based on the Arduino Uart example by Mike118.
  check the robot-maker post.

  pinout GPIO:


*/

#include <WiFi.h>
#include <ESPmDNS.h>

const char* ssid = "ssid";
const char* pass = "pass";

const char * host = "192.168.1.63"; // ip or dns of your host
const uint16_t port = 7070;

// Use WiFiClient class to create TCP connections
WiFiClient client;

// Meta Type :

typedef struct {
  union {
    struct {
      int16_t x;
      int16_t y;
    };
    int16_t coordonnees[2];
    uint8_t bytes[4];
  };
} Point;

typedef struct {
  union {
    struct {
      int8_t x;
      int8_t y;
      int8_t z;
    };
    uint8_t bytes[3];
  };
} Vitesses;


// CONFIG

#define NBPOSITIONS 4
#define FAILSAFE 250 // ms


// TTS

#define TTSBUFFERSIZE 255
uint8_t ttsBuffer[TTSBUFFERSIZE];
uint8_t ttsCurseur = 0;


// TX

#define TXFRAMESIZE (NBPOSITIONS * 4 + 17)

typedef struct {
  union {
    struct {
      uint8_t sync[4];               // 4
      Point positions[NBPOSITIONS];  // NBPOSITIONS * 4
      uint16_t val16[2];             // 2 * 2
      uint8_t choixCameras;          // 1
      Vitesses vitesses;             // 3
      uint8_t interrupteurs;         // 1
      uint8_t val8[4];               // 4
    };

    uint8_t bytes[TXFRAMESIZE];
  };
} TrameTx;


// RX

#define RXFRAMESIZE (NBPOSITIONS * 4 + 9)

typedef struct {
  union {
    struct {                       // Sizes
      uint8_t sync[4];              // 4
      Point positions[NBPOSITIONS]; // NBPOSITIONS * 4
      uint8_t choixCameras;         // 1
      Vitesses vitesses;            // 3
      uint8_t interrupteurs;        // 1
    };

    uint8_t bytes[RXFRAMESIZE];
  };
} TrameRx;


TrameTx trameTx;
TrameRx trameRx;

uint32_t lastTrameTimestamp = millis();
uint32_t lastSleepBeacon;
uint32_t lastConnectAttempt;

void setup()
{
  Serial.begin(115200);

  // We start by connecting to a WiFi network

  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE); //required for hostname to take effect on ESP32 (workaround)
  WiFi.setHostname("Vigibot_ESP_0");
  WiFi.begin(ssid, pass);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // add all your init here

}

void loop() {
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WLAN Connection Failed! reconnecting...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    delay(10000);
  }
  checkClient();

  if (readPiSerial()) {
    // each time we receive a full trame run repeatedly:
    // use values inside trameRx to tell your robot how to move ...
    // trameRx.vitesses.x , trameRx.vitesses.y, trameRx.vitesses.z
    // trameRx.positions[i].x trameRx.positions[i].y  etc....

    //test return
    /*if (trameTx.interrupteurs & 0b00100000) { //testing purposes
      trameRx.vitesses.y = 10;
    }*/

    //writePiSerial(); //make sure only one esp sends replies
    lastTrameTimestamp = millis();
  }
  if ( millis() - lastTrameTimestamp > FAILSAFE ) {
    if ((millis() - lastSleepBeacon > 10000) ) { // every 10 seconds
      //writePiSerial();                        // Beacon to say that the robot is alive
      lastSleepBeacon = millis();
    }
    // Stop the robot in case the robot lost connection with the Pi

  } else {
    // put your main code here, to run repeatedly:
    // avoid abstacle, run speed ...

  }
  //check for millis() overflow
  if (millis() < lastSleepBeacon) {
    lastSleepBeacon = millis();
  }
  if (millis() < lastConnectAttempt) {
    lastConnectAttempt = millis();
  }
}

void checkClient(){
  static byte lastClientState = 2;
  if (client.connected()) {
    if (lastClientState != 0) {
      lastClientState = 0;
      Serial.println("Client connected.");
    }
  } else {
    if (lastClientState == 0) { //if client is not actively connected anymore, stop and reconnect
      lastClientState = 1;
      // close the connection:
      client.stop();
      Serial.println("Client Disconnected.");
    }
    if ((millis() - lastConnectAttempt) > 5000) {
      lastConnectAttempt = millis();
      Serial.print("Connecting to ");
      Serial.println(host);
      if (!client.connect(host, port)) {
        Serial.println("Connection failed. Waiting 5 seconds before retrying...");
      }
    }
  }
}

bool readPiSerial() {
  uint8_t current;
  static uint8_t lastType = 0;
  static uint8_t n = 0;
  static uint8_t frame[RXFRAMESIZE];
  while (client.available()) {
    current = client.read();
    //Serial.write(current); //debug

    switch (n) {

      case 0:
        if (current == '$')
          n = 1;
        break;

      case 1:
        if (current != 'T' && lastType == 'T')
          writeTtsBuffer('\n');

        if (current == 'S' || current == 'T') {
          lastType = current;
          n = 2;
        } else
          n = 0;
        break;

      default:
        frame[n++] = current;

        if (n == RXFRAMESIZE) {

          if (lastType == 'T') {

            for (uint8_t i = 4; i < RXFRAMESIZE; i++) // Do not send the 4 sync data in tts
              writeTtsBuffer(frame[i]);

          } else if (lastType == 'S') {

            for (uint8_t p = 0; p < RXFRAMESIZE; p++)
              trameRx.bytes[p] = frame[p];

          }
          n = 0;
          return true;
        }
        //break;
    }
  }
  return false;
}

void writePiSerial() {
  // Header, do not modify
  trameTx.sync[0] = '$';
  trameTx.sync[1] = 'R';
  trameTx.sync[2] = ' ';
  trameTx.sync[3] = ' ';

  // modify the feedback according your need. By default we copy the trameRx content ...
  for (uint8_t i = 0; i < NBPOSITIONS; i++) {
    trameTx.positions[i].x = trameRx.positions[i].x;
    trameTx.positions[i].y = trameRx.positions[i].y;
  }
  trameTx.val16[0] = 0;   // Voltage (will be updated by Raspberry pi)
  trameTx.val16[1] = 0;   // Percent (will be updated by Raspberry pi)
  trameTx.choixCameras = trameRx.choixCameras;
  trameTx.vitesses.x = trameRx.vitesses.x;
  trameTx.vitesses.y = trameRx.vitesses.y;
  trameTx.vitesses.z = trameRx.vitesses.z;
  trameTx.interrupteurs = trameRx.interrupteurs;
  trameTx.val8[0] = 0;   // CPU load (will be updated by Raspberry pi)
  trameTx.val8[1] = 0;   // Soc temp (will be updated by Raspberry pi)
  trameTx.val8[2] = 0;   // link (will be updated by Raspberry pi)
  trameTx.val8[3] = 0;   // RSSI (will be updated by Raspberry pi)

  for ( uint8_t i = 0; i < TXFRAMESIZE; i++)
    client.write(trameTx.bytes[i]);
}

void displayTtsBuffer (uint8_t * ttsBuffer, uint8_t bufferSize) {
  // you can modify this function to display text on a screen depending on your hardware...
  for ( uint8_t i = 0; i < bufferSize; i++) {
    Serial.write(ttsBuffer[i]);
  }
  Serial.println("");
}

void writeTtsBuffer( uint8_t ttsChar) {
  static uint8_t ttsCurseur = 0;
  if ( ttsCurseur < TTSBUFFERSIZE && ttsChar != '\n') {
    ttsBuffer[ttsCurseur] = ttsChar;
    ttsCurseur ++;
  }
  if ( ttsCurseur == TTSBUFFERSIZE || ttsChar == '\n') {
    displayTtsBuffer (ttsBuffer, ttsCurseur);
    ttsCurseur = 0;
  }
}
