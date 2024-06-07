#include <WiFiManager.h>
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#include <TinyGPSPlus.h>
#include <LoRa.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define ss 5
#define rst 14
#define dio0 2

#define API_KEY "AIzaSyCWqKYwAcB-f9ZQ5AlXp3d9vQovCOCf_yI"
#define DATABASE_URL "https://garbage-collector-8c46b-default-rtdb.firebaseio.com"
#define USER_EMAIL "garbagecollector@node.com"
#define USER_PASSWORD "@garbageNode"

//Address
String nodes[1] = { "Truck-01" };
byte address[1] = { 0xAA };
byte MASTER = 0xFF;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseJson json;
FirebaseJsonArray arr;
FirebaseConfig config;

unsigned long startMillis;
unsigned long currentMillis;
const unsigned long period = 30000;

bool signupOK = false;
unsigned long timestamp;

AsyncWebServer server(80);
TinyGPSPlus gps;

String parentPath = "/NODES";

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.windows.com");


void setup() {
  Serial.begin(115200);

  WiFiManager wm;

  bool res;
  res = wm.autoConnect("ESP-32 WIFI Manager");

  if (!res) {
    Serial.println("Failed to connect");
    ESP.restart();
  } else {

    LoRa.setPins(ss, rst, dio0);

    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;

    Firebase.reconnectWiFi(true);
    fbdo.setResponseSize(4096);

    config.token_status_callback = tokenStatusCallback;
    Firebase.begin(&config, &auth);

    while ((auth.token.uid) == "") {
      Serial.print('.');
      delay(1000);
    }

    if (!LoRa.begin(433E6)) {
      Serial.println("Starting LoRa failed!");
      while (1)
        ;
    }

    Serial.println("LoRa Master Node: Initialized.");
    startMillis = millis();
    arr.clear();
  }
}

unsigned long getTime() {
  timeClient.update();
  unsigned long now = timeClient.getEpochTime();
  return now;
}

void sendMessage(String payload, byte MasterNode, byte otherNode) {
  LoRa.beginPacket();
  LoRa.write(otherNode);
  LoRa.write(MasterNode);
  LoRa.write(payload.length());
  LoRa.print(payload);
  LoRa.endPacket();
}

void onReceive(int packetSize) {

  if (packetSize == 0) return;
  String incoming;

  int recipient = LoRa.read();
  byte sender = LoRa.read();
  byte incomingLength = LoRa.read();

  while (LoRa.available()) {
    incoming += (char)LoRa.read();
  }

  if (incomingLength != incoming.length()) {
    ;
    return;
  }

  Serial.print("Recipient: ");
  Serial.println(recipient);
  Serial.print("Sender: ");
  Serial.println(sender);
  Serial.print("Length: ");
  Serial.println(incomingLength);
  Serial.print("Message: ");
  Serial.println(incoming);

  for (int a = 0; a < 1; a++) {
    if (sender == address[a]) {

      String initialLat = getValue(incoming, ',', 0);
      String initialLng = getValue(incoming, ',', 1);
      String currentLat = getValue(incoming, ',', 2);
      String currentLng = getValue(incoming, ',', 3);
      timestamp = getTime();

      json.set("/initial/latitude", initialLat);
      json.set("/initial/longitude", initialLng);
      json.set("/timestamp", timestamp);

      if (Firebase.ready() && currentMillis - startMillis >= period) {
        arr.add(currentLat + "," + currentLng);
        Firebase.RTDB.setJSON(&fbdo, (parentPath + "/" + nodes[a]).c_str(), &json);
        Firebase.RTDB.setArray(&fbdo, (parentPath + "/" + nodes[a] + "/current").c_str(), &arr);

        startMillis = currentMillis;
      }
    }
  }
}

String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;
  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void loop() {

  currentMillis = millis();
  onReceive(LoRa.parsePacket());
}