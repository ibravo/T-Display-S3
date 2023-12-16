/**
   ESP8266SignalkClient.ino
    Marco Bergman
    Created on: 24-NOV-2023

*/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WString.h>

// User configuration starts here
String wifiSsid        =  "baaja";
String wifiPassword    =  "argentina";
String signalkIpString =  "10.10.10.1";
int    signalkUdpPort  =  30330;
String signalkSource   =  "LILYGO-S3";
// User configuration ends here

WiFiClient client;
WiFiUDP udp;

IPAddress signalkIp;
bool x = signalkIp.fromString(signalkIpString);

int i = 1; // metadata counter


void setup() {
  Serial.begin(115200);
  startWifi();
  udp.begin(33333); //Local UPD Port to use to send FROM
}


void startWifi() {
  Serial.print("\nConnecting to Wifi");

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid, wifiPassword);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
}


void signalkSendValue (String path, String value, String units) {
  String message = "{\"updates\":[{\"$source\": \""+ signalkSource + "\", \"values\":[{\"path\":\"" + path + "\",\"value\":" + value + "}]}]}";

  i -= 1; if (i == 0) {  // to start, then periodically, update the 'units' metadata:
    message = "{\"updates\":[{\"meta\":[{\"path\":\"" + path + "\",\"value\":{\"units\":\"" + units + "\"}}]}]}";
    i = 100;
  }
  Serial.println(message);

  udp.beginPacket(signalkIp, signalkUdpPort);
  udp.print(message);
  udp.endPacket();  
}


void loop() {
  float value = float(analogRead(A0)) ;
  // float value = float(analogRead(A0))/30 ;
  signalkSendValue("cell.voltage", String(value), "V");
  delay(1000);
}
