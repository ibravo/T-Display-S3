/**
   ESP8266SignalkClient.ino
    Marco Bergman
    Created on: 24-NOV-2023

*/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WString.h>
#include "TFT_eSPI.h" /* Please use the TFT library provided in the library. */
#include "pin_config.h"


// User configuration starts here
String wifiSsid        =  "baaja"; //baaja
String wifiPassword    =  "argentina";//argentina
String signalkIpString =  "10.10.10.1";
int    signalkUdpPort  =  30330;
String signalkSource   =  "LILYGO-S3";
// User configuration ends here

//Screen Variables
/* The product now has two screens, and the initialization code needs a small change in the new version. The LCD_MODULE_CMD_1 is used to define the
 * switch macro. */
#define LCD_MODULE_CMD_1

TFT_eSPI tft = TFT_eSPI();
unsigned long targetTime = 0; // Used for testing draw times

#if defined(LCD_MODULE_CMD_1)
typedef struct {
    uint8_t cmd;
    uint8_t data[14];
    uint8_t len;
} lcd_cmd_t;

lcd_cmd_t lcd_st7789v[] = {
    {0x11, {0}, 0 | 0x80},
    {0x3A, {0X05}, 1},
    {0xB2, {0X0B, 0X0B, 0X00, 0X33, 0X33}, 5},
    {0xB7, {0X75}, 1},
    {0xBB, {0X28}, 1},
    {0xC0, {0X2C}, 1},
    {0xC2, {0X01}, 1},
    {0xC3, {0X1F}, 1},
    {0xC6, {0X13}, 1},
    {0xD0, {0XA7}, 1},
    {0xD0, {0XA4, 0XA1}, 2},
    {0xD6, {0XA1}, 1},
    {0xE0, {0XF0, 0X05, 0X0A, 0X06, 0X06, 0X03, 0X2B, 0X32, 0X43, 0X36, 0X11, 0X10, 0X2B, 0X32}, 14},
    {0xE1, {0XF0, 0X08, 0X0C, 0X0B, 0X09, 0X24, 0X2B, 0X22, 0X43, 0X38, 0X15, 0X16, 0X2F, 0X37}, 14},
};
#endif

// End screen Definitions

WiFiClient client;
WiFiUDP udp;

IPAddress signalkIp;
bool x = signalkIp.fromString(signalkIpString);

int i = 1; // metadata counter


void setup() {
// Initialize Display
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);
    tft.begin();

#if defined(LCD_MODULE_CMD_1)
    for (uint8_t i = 0; i < (sizeof(lcd_st7789v) / sizeof(lcd_cmd_t)); i++) {
        tft.writecommand(lcd_st7789v[i].cmd);
        for (int j = 0; j < (lcd_st7789v[i].len & 0x7f); j++) {
            tft.writedata(lcd_st7789v[i].data[j]);
        }

        if (lcd_st7789v[i].len & 0x80) {
            delay(120);
        }
    }
#endif

    tft.setRotation(3);
    tft.setSwapBytes(true);
//    delay(2000);

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5,0,0)
    ledcSetup(0, 2000, 8);
    ledcAttachPin(PIN_LCD_BL, 0);
    ledcWrite(0, 255);
#else
    ledcAttach(PIN_LCD_BL, 200, 8);
    ledcWrite(PIN_LCD_BL, 255);
#endif


// Connect to Wifi

  Serial.begin(115200);
  startWifi();
  udp.begin(33333); //Local UPD Port to use to send FROM
}


void startWifi() {
  //Show status on Serial and Screen
  Serial.print("\nConnecting to Wifi");
  tft.setTextSize(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_BLUE, TFT_BLACK);
  tft.drawString("Connecting to Wifi:", 0, 0, 4);
  tft.drawString(wifiSsid, 10, 30, 4);

  //Connect with Wifi
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
  //TO DO
  //reconnect to wifi if disconnected

  targetTime = millis();

  float CellValue = float(analogRead(A0)) ;
  // float value = float(analogRead(A0))/30 ;
  signalkSendValue("cell.voltage", String(CellValue), "V");

    // Display Values
    // First we test them with a background colour set
    tft.setTextSize(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);

    // Font 6 row = 20 units
    tft.setTextColor(TFT_RED, TFT_BLACK); //Yellow numbers
    tft.drawString(String(int(CellValue)), 0, 0, 7);

    tft.setTextSize(1);
    tft.setTextColor(TFT_BLUE, TFT_BLACK); //Blue numbers
    tft.drawString("Volts", 90, 20, 4);
    tft.setTextSize(1);

  delay(2000);
}
