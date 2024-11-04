#include <GxEPD2_BW.h>
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "wifi_credentials.h" // Include Wi-Fi credentials
#include <Adafruit_GFX.h>
#include <Fonts/FreeMonoBold24pt7b.h> // Large font for time
#include <Fonts/FreeMonoBold12pt7b.h> // More fonts; https://github.com/adafruit/Adafruit-GFX-Library/tree/master/Fonts
#include "w3_ip28pt7b.h" // https://www.dafont.com/w3usdip.font?text=01%3A07+Friday+15th+August&back=theme
#include "w3_ip18pt7b.h" 
#include "w3_ip14pt7b.h"

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60        /* Time ESP32 will go to sleep (in seconds) */

// GPIO bitmasks for wakeup
#define BUTTON_1 (1ULL << GPIO_NUM_1) // GPIO 1 bitmask for EXT1 wakeup
#define BUTTON_2 (1ULL << GPIO_NUM_2) // GPIO 2 bitmask for EXT1 wakeup

// E-paper display pin configuration
#define EPD_CS    21
#define EPD_DC    17
#define EPD_RST   16
#define EPD_BUSY  20

// Button pins
#define BUTTON_WIFI 1   // Button to connect Wi-Fi
#define BUTTON_SET  2   // Button to set custom time

// Battery monitoring pin
#define BATTERY_PIN  0

// Add these constants for OTA
#define HOSTNAME "t2.local"  // Change this to whatever you'd like to call your device
#define OTA_PASSWORD "timeisprecious"  // Change this to a secure password

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
char monthsOfTheYear[12][12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// NTP server settings
const long utcOffsetInSeconds = 19800; // UTC+5:30 for IST

// Delay sleep if required
int delaySleep = 0;


// Initialize display
GxEPD2_BW<GxEPD2_154_GDEY0154D67, GxEPD2_154_GDEY0154D67::HEIGHT> display(GxEPD2_154_GDEY0154D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// Initialize RTC
RTC_DS3231 rtc;

// Initialize NTP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

// Battery monitoring variables
float batteryVoltage = 0.0;
const float voltageDividerRatio = 2.0; // 1/2 attenuation ratio
int battPercent = 0;

void setup() {
  // Begin Serial for debugging
  Serial.begin(115200);

  // Initialize I2C
  Wire.begin(22, 23);

  // Initialize e-paper display
  display.init();

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting time to compile time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Configure button pins
  pinMode(BUTTON_WIFI, INPUT);
  pinMode(BUTTON_SET, INPUT);

  // Handle button logic
  buttonLogic();

  // Update battery voltage
  updateBatteryVoltage();

  // Update the display
  updateDisplay();

 // Go to sleep if not in Set Time mode
    sleepCycle();
}

void loop() {
  // Should never get here, as the ESP will wake up from deep sleep
}

void connectToWiFi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  
  // Wait for connection with a timeout
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Wi-Fi");
    setupOTA();  // Initialize OTA after WiFi connection
  } else {
    Serial.println("\nWiFi connection failed!");
  }
}

void setupOTA() {
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    display.setFullWindow();
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
      display.setTextColor(GxEPD_BLACK);
      display.setFont(&w3_ip28pt7b);
      display.setCursor(7, 100);
      display.print("OTA Update Starting...");
    } while (display.nextPage());
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onEnd([]() {
    display.setFullWindow();
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
      display.setTextColor(GxEPD_BLACK);
      display.setFont(&FreeMonoBold12pt7b);
      display.setCursor(10, 100);
      display.print("Update Complete!");
    } while (display.nextPage());
  });

  ArduinoOTA.begin();
}

void checkAndSyncTime() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  timeClient.update();
  DateTime now = rtc.now();
  unsigned long currentEpoch = timeClient.getEpochTime();

  if (abs((long)(now.unixtime() - currentEpoch)) > 10) {
    Serial.println("Time discrepancy detected, updating RTC...");
    rtc.adjust(DateTime(currentEpoch));
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void updateBatteryVoltage() {
  uint32_t Vbatt = 0;
  for (int i = 0; i < 16; i++) {
    Vbatt += analogReadMilliVolts(BATTERY_PIN); // ADC with correction
  }
  int batt = voltageDividerRatio * Vbatt / 16;
  batteryVoltage = voltageDividerRatio * Vbatt / 16 / 1000.0; // mV to V
  battPercent = map(batt,3200,4200,0,100);
}

void updateDisplay() {
  DateTime now = rtc.now();

  display.setRotation(0);
  display.setFont(&w3_ip28pt7b);
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() - tbh) / 2) - tby;
  
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(7, y - 25);
    display.println(String(now.hour()) + ":" + String(now.minute())); // 22:23
    
    display.setFont(&w3_ip18pt7b);
    display.setCursor(0, y + 5);
    display.println("---------"); 
    display.setCursor(7, y + 32);
    display.println(daysOfTheWeek[now.dayOfTheWeek()]);    // Wednesday 
    
    display.setFont(&w3_ip14pt7b);
    display.setCursor(7, y + 65);
    display.println(String(now.day()) + ' ' + String(monthsOfTheYear[now.month() - 1]));    // 14 Nov

    // Draw battery icon
    drawBatteryIcon();
  } while (display.nextPage());
}

void drawBatteryIcon() {
  int x = 170, y = 0;
  display.drawRect(x, y, 30, 15, GxEPD_BLACK); // Battery Outline 
  display.fillRect(x + 31, y + 4, 2, 6, GxEPD_BLACK); // Battery terminal
  int fillWidth = (int)(30 * battPercent / 100); // Scale for 4.2V max
  display.fillRect(x + 1, y + 1, fillWidth, 13, GxEPD_BLACK); // Battery Fill
  // display.setCursor(140, 40);
  // display.print(String(batteryVoltage));
  // display.setCursor(140, 60);
  // display.print(String(battPercent));
 
}

void sleepCycle() {

  //Delay Sleep
  delay(delaySleep);
 
  // Configure wakeup sources
  esp_sleep_enable_ext1_wakeup(BUTTON_1 | BUTTON_2, ESP_EXT1_WAKEUP_ANY_LOW); // Wake on either button
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR); // Wakeup timer

  // Enter deep sleep
  esp_deep_sleep_start();
}

void buttonLogic() {
  // If Wi-Fi button is pressed, connect to Wi-Fi and wait for potential OTA
  if (digitalRead(BUTTON_WIFI) == LOW) {
    connectToWiFi();
    if (WiFi.status() == WL_CONNECTED) {
      delaySleep = 180000;  // Delay sleep for 1 minute to allow for OTA
      // Wait for potential OTA update
      unsigned long startTime = millis();
      while (millis() - startTime < 60000) {  // Wait for 1 minute
        ArduinoOTA.handle();
        delay(10);
      }
      checkAndSyncTime();  // Sync time after OTA window
    }
  }
}

void setCustomTime() {
  //Placeholder for the time stamp
  Serial.println("Entering Set Time Mode...");
  
}
