#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <LoRa.h>

static const int RXPin = 4, TXPin = 3;
static const uint32_t GPSBaud = 9600;

TinyGPSPlus gps;
SoftwareSerial ss(RXPin, TXPin);

//Address
byte NODE = 0xAA;
byte MASTER = 0xFF;
bool isInitial = true;
String initialLat, initialLng, currentLat, currentLng;

void setup() {
  Serial.begin(9600);
  ss.begin(GPSBaud);

  while (!Serial)
    ;
  Serial.println("LoRa Sender");
  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1)
      ;
  }
}

void sendMessage(String payload, byte MasterNode, byte node) {
  LoRa.beginPacket();
  LoRa.write(MasterNode);
  LoRa.write(node);
  LoRa.write(payload.length());
  LoRa.print(payload);
  LoRa.endPacket();
}

void loop() {
  while (ss.available() > 0) {
    gps.encode(ss.read());
    if (gps.location.isUpdated()) {

      if (!gps.location.isValid()) return;

      if (isInitial) {
        initialLat = String(gps.location.lat(), 6);
        initialLng = String(gps.location.lng(), 6);

        isInitial = false;
      }

      currentLat = String(gps.location.lat(), 6);
      currentLng = String(gps.location.lng(), 6);

      Serial.print("Latitude: ");
      Serial.println(currentLat);
      Serial.print("Longitude: ");
      Serial.println(currentLng);

      String payload = initialLat + "," + initialLng + "," + currentLat + "," + currentLng;
      sendMessage(payload, MASTER, NODE);
      delay(50);
    }
  }
}