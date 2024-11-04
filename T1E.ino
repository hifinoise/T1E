#include <GxEPD2_BW.h>
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Fonts/FreeMonoBold24pt7b.h> // Large font for time
#include <Fonts/FreeMonoBold12pt7b.h>  // Smaller font for date
#include <Adafruit_GFX.h>
#include "wifi_credentials.h" // Include Wi-Fi credentials

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
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi");
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

  char timeBuffer[10];
  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", now.hour(), now.minute());

  char dateBuffer[20];
  snprintf(dateBuffer, sizeof(dateBuffer), "%02d %s", now.day(), monthName(now.month()));

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    // Display time, centered
    display.setFont(&FreeMonoBold24pt7b);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(timeBuffer, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((display.width() - w) / 2, 100);
    display.print(timeBuffer);

    // Display date, centered
    display.setFont(&FreeMonoBold12pt7b);
    display.getTextBounds(dateBuffer, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((display.width() - w) / 2, 150);
    display.print(dateBuffer);

    // Draw battery icon
    drawBatteryIcon();
  } while (display.nextPage());
}

const char* monthName(int month) {
  const char* monthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  return monthNames[month - 1];
}

void drawBatteryIcon() {
  int x = 160, y = 0;
  display.drawRect(x, y, 30, 15, GxEPD_BLACK); // Battery Outline 
  display.fillRect(x + 31, y + 4, 2, 6, GxEPD_BLACK); // Battery terminal
  int fillWidth = (int)(30 * battPercent / 100); // Scale for 4.2V max
  display.fillRect(x + 1, y + 1, fillWidth, 13, GxEPD_BLACK); // Battery Fill
 // Uncomment for Battery Diagnostics
 //display.setCursor(140, 40);
 // display.print(String(batteryVoltage));
 // display.setCursor(140, 60);
 //display.print(String(battPercent));
 
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

  // If Wi-Fi button is pressed, connect to Wi-Fi and update time
  if (digitalRead(BUTTON_WIFI) == LOW) {
    connectToWiFi();
    checkAndSyncTime();
  }

 // If the set time button is pressed, enter the Set Time mode
  if (digitalRead(BUTTON_SET) == LOW) {
    setCustomTime();
  }
}

void setCustomTime() {
  //Placeholder for the time stamp
  Serial.println("Entering Set Time Mode...");
  
}
