/**
   ESP8266SignalkClient.ino
    Marco Bergman
    Created on: 24-NOV-2023

*/

#include <Arduino.h>
#include <WiFi.h>        // To connect to Wifi
#include <WiFiUdp.h>     // To Send UDP Packets over Wifi
#include <WString.h>
#include "TFT_eSPI.h"    // Please use the TFT library provided in the library. */
#include "pin_config.h"  // Defines PINs by function

#include "SDL_Arduino_INA3221.h"  // For the Shunt Power
#include <Wire.h>

// INA3221 Variables
SDL_Arduino_INA3221 ina3221;
// the three channels of the INA3221 named for SunAirPlus Solar Power Controller channels (www.switchdoc.com)
#define INA3221_CHANNEL1 1
#define INA3221_CHANNEL2 2
#define INA3221_CHANNEL3 3

//Define I2C ports
int sda = 44;
int scl = 43;



// User configuration starts here
//SignalK and Wifi
String wifiSsid        =  "baaja";       //baaja
String wifiPassword    =  "argentina";   //argentina
String signalkIpString =  "10.10.10.1";
int    signalkUdpPort  =  30330;
String signalkSource   =  "LILYGO-S3";
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
  ina3221.begin();  // Enable INA3221 Current Circuit

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
  Serial.print("\nConnecting to Wifi");

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
delay(500);
udp.begin(33333);

}

// Prints an asterisk in the top right showing connection status
void Print_Wifi_Status() {
  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("*",300,0,4);
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("*",300,0,4);
  };
};

// Sends data to SignalK Server
void signalkSendValue (String path, String value, String units) {
  String message = "{\"updates\":[{\"$source\": \""+ signalkSource + "\", \"values\":[{\"path\":\"" + path + "\",\"value\":" + value + "}]}]}";

  SK_MetaCounter -= 1; if (SK_MetaCounter == 0) {  // to start, then periodically, update the 'units' metadata:
    message = "{\"updates\":[{\"meta\":[{\"path\":\"" + path + "\",\"value\":{\"units\":\"" + units + "\"}}]}]}";
    SK_MetaCounter = 100;
  }
  Serial.println(message);

  udp.beginPacket(signalkIp, signalkUdpPort);
  udp.print(message);
  udp.endPacket();  
}

  

void loop() {
  //TO DO


  //float CellVolt = float(analogRead(A0))/30 ;
  double CellVoltRaw = analogRead(A0) ;
  double CellVolt = CellVoltRaw * 16.5 / 4095;
  //CellVolt = analogRead(A0);

  // Values from SHUNT
  String shunt_title= String("Bus V") + " " + String("Shunt mV") + " " + String("Load V") + "  " + String("Current mA") ;

  // INA3221 Channel 1
  float shuntvoltage1 = 0;
  float busvoltage1 = 0;
  float current_mA1 = 0;
  float loadvoltage1 = 0;

  busvoltage1 = ina3221.getBusVoltage_V(INA3221_CHANNEL1);
  shuntvoltage1 = ina3221.getShuntVoltage_mV(INA3221_CHANNEL1);
  current_mA1 = -ina3221.getCurrent_mA(INA3221_CHANNEL1);  // minus is to get the "sense" right.   - means the battery is charging, + that it is discharging
  loadvoltage1 = busvoltage1 + (shuntvoltage1 / 1000);
  
  String shunt_output1= String(busvoltage1) + "V " + String(shuntvoltage1) + " mV " + String(loadvoltage1) + "V " + String(current_mA1) + " mA " ;
  Serial.print(shunt_output1);
  // INA3221 Channel 2
  float shuntvoltage2 = 0;
  float busvoltage2 = 0;
  float current_mA2 = 0;
  float loadvoltage2 = 0;

  busvoltage2 = ina3221.getBusVoltage_V(INA3221_CHANNEL2);
  shuntvoltage2 = ina3221.getShuntVoltage_mV(INA3221_CHANNEL2);
  current_mA2 = -ina3221.getCurrent_mA(INA3221_CHANNEL2);  // minus is to get the "sense" right.   - means the battery is charging, + that it is discharging
  loadvoltage2 = busvoltage2 + (shuntvoltage2 / 1000);
  
  String shunt_output2= String(busvoltage2) + "V " + String(shuntvoltage2) + " mV " + String(loadvoltage2) + "V " + String(current_mA2) + " mA " ;
  Serial.print(shunt_output2);
  // INA3221 Channel 3
  float shuntvoltage3 = 0;
  float busvoltage3 = 0;
  float current_mA3 = 0;
  float loadvoltage3 = 0;

  busvoltage3 = ina3221.getBusVoltage_V(INA3221_CHANNEL3);
  shuntvoltage3 = ina3221.getShuntVoltage_mV(INA3221_CHANNEL3);
  current_mA3 = -ina3221.getCurrent_mA(INA3221_CHANNEL3);  // minus is to get the "sense" right.   - means the battery is charging, + that it is discharging
  loadvoltage3 = busvoltage3 + (shuntvoltage3 / 1000);
  
  String shunt_output3= String(busvoltage3) + "V " + String(shuntvoltage3) + " mV " + String(loadvoltage3) + "V " + String(current_mA3) + " mA " ;
  Serial.print(shunt_output3);
  


// Send values to SignalK Server
  signalkSendValue("cell.voltage", String(CellVolt), "V");


  //Formats Voltage so that it always reads in as two int values
  String CellVoltText = "0" + String((CellVolt));
  if (CellVolt <= 10){
    String CellVoltText = String((CellVolt));}
  String CellVoltTextRaw = String(int(CellVoltRaw));
  

  // Display Values
  // First we test them with a background colour set
  tft.fillScreen(TFT_BLACK);

  // Font 6 row = 20 units
  tft.setTextSize(1);
  tft.setTextColor(TFT_RED, TFT_BLACK); //Red numbers
  tft.drawString(CellVoltText, 0, 0, 6);
  tft.drawString(CellVoltTextRaw, 180, 20, 4);
  tft.setTextColor(TFT_BLUE, TFT_BLACK); //Green numbers
  tft.drawString(shunt_title, 0, 70, 4);
  tft.setTextColor(TFT_GREEN, TFT_BLACK); //Green numbers
  tft.drawString(shunt_output1, 0, 90, 4);
  tft.drawString(shunt_output2, 0, 110, 4);
  tft.drawString(shunt_output3, 0, 130, 4);

  tft.setTextSize(1);
  tft.setTextColor(TFT_BLUE, TFT_BLACK); //Blue numbers
  tft.drawString("V", 130, 20, 4);
  tft.drawString("Raw", 250, 20, 4);
  tft.setTextSize(1);

  Print_Wifi_Status();              // Prints asterisk on Top Right


  // Perform these checks every 10 seconds
  if (millis() - WifiTime >= 10000) {
    // Checks WiFi status and reconnects if not up
    if (WiFi.status() != WL_CONNECTED) {
      //Wifi is not connected. Reconnect
      //startWifi();
    }
    // Other tests every 10 seconds
    //

  //Reset WifiTime clock back to zero
  WifiTime = millis();
  }


  delay(2000);
}
