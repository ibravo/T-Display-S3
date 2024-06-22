/**
   NMEA2k.ino
    Ignacio Bravo
    Created on: 12-JUN-2024

*/

#include <Arduino.h>
#include <WiFi.h>        // To connect to Wifi
#include <WiFiUdp.h>     // To Send UDP Packets over Wifi
#include <WString.h>
#include "TFT_eSPI.h"    // Please use the TFT library provided in the library. */
#include "pin_config.h"  // Defines PINs by function

#include <Wire.h>

//Define I2C ports
int sda = 44;
int scl = 43;


// User configuration starts here
//SignalK and Wifi
String wifiSsid        =  "Tango5";       //baaja
String wifiPassword    =  "du1cede1eche";   //argentina
String signalkIpString =  "10.10.10.1";
int    signalkUdpPort  =  30330;
String signalkSource   =  "HELM";
// User configuration ends here


//Screen Variables
TFT_eSPI tft = TFT_eSPI();   // Create TFT Library

//Other Screen that I'm not sure what they are for
/* The product now has two screens, and the initialization code needs a small change in the new version. The LCD_MODULE_CMD_1 is used to define the
 * switch macro. */
#define LCD_MODULE_CMD_1

unsigned long WifiTime = 0; // Used for testing draw times

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


// Signal K and Wifi Definitions
WiFiClient client;
WiFiUDP udp;
IPAddress signalkIp;
bool x = signalkIp.fromString(signalkIpString);
int SK_MetaCounter = 1;          // SignalK metadata counter


void setup() {
// Initialize Display
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);
  tft.begin();                      // Screen ON
  tft.setRotation(3);
  tft.setSwapBytes(true); 
  tft.fillScreen(TFT_BLACK);        // Clean Screen


// Unknown Screen Settings 
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

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5,0,0)
    ledcSetup(0, 2000, 8);
    ledcAttachPin(PIN_LCD_BL, 0);
    ledcWrite(0, 255);
#else
    ledcAttach(PIN_LCD_BL, 200, 8);
    ledcWrite(PIN_LCD_BL, 255);
#endif
// End of Unknown Screen commands


// Connect to Serial, I2C and Wifi
  Serial.begin(115200);
  Wire.begin(sda, scl);
  startWifi();
//  udp.begin(33333); //Local UPD Port to use to send FROM

}


void startWifi() {
/*
  //Show status on Screen
  tft.setTextSize(1);
  tft.fillScreen(TFT_BLACK);                        //Clean Screen
  tft.setTextColor(TFT_BLUE, TFT_BLACK);
  tft.drawString("Connecting to Wifi:", 0, 0, 4);   //Display info about Wifi network
  tft.drawString(wifiSsid, 10, 30, 4);
*/
  Serial.println("\nConnecting to Wifi...");
  tft.fillCircle(300,10,5,TFT_BLUE);

  //Connect with Wifi
  WiFi.mode(WIFI_STA);   //Client (Station) Mode
  WiFi.begin(wifiSsid, wifiPassword);

/* Uncomment this section to show connection time
  unsigned long StartTime = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - StartTime) < 15000) {
    Serial.print(".");
    tft.drawString(String("Connecting... " + String(millis()/1000)), 10, 60, 4);   //Display info about Wifi network
    delay(500);
  }
*/
delay(1000);
udp.begin(33333);

}

// Prints an asterisk in the top right showing connection status
void Print_Wifi_Status() {
  if (WiFi.status() == WL_CONNECTED) {
    tft.fillCircle(300,10,5,TFT_GREEN);
//    tft.setTextColor(TFT_GREEN, TFT_BLACK);
//    tft.drawString("*",300,0,4);
  } else {
    tft.fillCircle(300,10,5,TFT_RED);
//    tft.setTextColor(TFT_RED, TFT_BLACK);
//    tft.drawString("*",300,0,4);
  };
};

// Sends data to SignalK Server
void signalkSendValue (String path, String value, String units) {
  String message = "{\"updates\":[{\"$source\": \""+ signalkSource + "\", \"values\":[{\"path\":\"" + path + "\",\"value\":" + value + "}]}]}";

  SK_MetaCounter -= 1; if (SK_MetaCounter == 0) {  // to start, then periodically, update the 'units' metadata:
    message = "{\"updates\":[{\"meta\":[{\"path\":\"" + path + "\",\"value\":{\"units\":\"" + units + "\"}}]}]}";
    SK_MetaCounter = 100;
  }


  // Checks WiFi status and reconnects if not up
  if (WiFi.status() != WL_CONNECTED) {
    //Wifi is not connected.
  Serial.println("\n ==// Not connected to WiFi //==");
  Serial.println(" ");

  } else {
    //Wifi Connected, send packets
  udp.beginPacket(signalkIp, signalkUdpPort);
  udp.print(message);
  udp.endPacket();  
  Serial.println(message);
  }

}

  

void loop() {
  //TO DO
 
  //Define Battery Variables for testing
  float batt1_v = 13.95;
  float batt2_v = 12.04;
  float batt3_v = 12.96;

  float batt1_c = 3.58;
  float batt2_c = 5.33;
  float batt3_c = 4.47;
  
  int batt1_pct = 29;
  int batt2_pct = 101;
  int batt3_pct = 59;

// Send values to SignalK Server
  //Battery3 Voltage
  signalkSendValue("cell.voltage", String(batt3_v), "V");
 

  // Display Values
  // First we test them with a background colour set
  //tft.fillScreen(TFT_BLACK);

  // Print Battery Screen
  // Battery Titles
  tft.setTextSize(1);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  tft.drawString("Volts", 10, 0, 4);
  tft.drawString("Amps", 100, 0, 4);
  tft.drawString("Battery", 200, 0, 4);


  // Battery 1 to 3 Volts and Amps
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString((String) batt1_v, 0, 40, 4);
  tft.drawString((String) batt1_c, 110, 40, 4);
  tft.drawString((String) batt2_v, 0, 90, 4);
  tft.drawString((String) batt2_c, 110, 90, 4);
  tft.drawString((String) batt3_v, 0, 140, 4);
  tft.drawString((String) batt3_c, 110, 140, 4);



  // Horizontal Dividers
  tft.drawLine(0, 25, 300, 25, TFT_LIGHTGREY);
  tft.drawLine(0, 75, 300, 75, TFT_LIGHTGREY);
  tft.drawLine(0, 125, 300, 125, TFT_LIGHTGREY);



  // Print Wifi Connection Status
  Print_Wifi_Status();              // Prints asterisk on Top Right



  // Perform these checks every 20 seconds
  if (millis() - WifiTime >= 20000) {
    // Checks WiFi status and reconnects if not up
    if (WiFi.status() != WL_CONNECTED) {
      //Wifi is not connected. Reconnect
      startWifi();
    }
    // Other tests every 10 seconds
    //

  //Reset WifiTime clock back to zero
  WifiTime = millis();
  }

  delay(2000);
}
