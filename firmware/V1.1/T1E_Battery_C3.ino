#include <GxEPD2_BW.h>
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Fonts/FreeMonoBold24pt7b.h> // Large font for time
#include <Fonts/FreeMonoBold12pt7b.h> // Smaller font for date
#include <Adafruit_GFX.h>
#include "example_wifi_credentials.h" // Include Wi-Fi credentials
#include <esp_sleep.h>

#define uS_TO_S_FACTOR 1000000ULL /* Conversion factor for microseconds to seconds */
#define TIME_TO_SLEEP  60        /* Time ESP32 will go to sleep (in seconds) */

// GPIO bitmasks for wakeup
#define BUTTON_1 (1ULL << GPIO_NUM_3) // GPIO 1 bitmask for EXT1 wakeup
#define BUTTON_2 (1ULL << GPIO_NUM_4) // GPIO 2 bitmask for EXT1 wakeup

// E-paper display pin configuration
#define EPD_CS    5
#define EPD_DC    20
#define EPD_RST   21
#define EPD_BUSY  9

// Button pins
#define BUTTON_WIFI GPIO_NUM_3   // Button to connect Wi-Fi
#define BUTTON_SET  GPIO_NUM_4   // Button to set custom time

// Battery monitoring pin
#define BATTERY_PIN 2

// Delay sleep if required
int delaySleep = 0;

// NTP server settings
const long utcOffsetInSeconds = 19800; // UTC+5:30 for IST

// Battery monitoring variables
float batteryVoltage = 0.0;
const float voltageDividerRatio = 2.0; // 1/2 attenuation ratio
int battPercent = 0;

// Initialize display
GxEPD2_BW<GxEPD2_154_GDEY0154D67, GxEPD2_154_GDEY0154D67::HEIGHT> display(GxEPD2_154_GDEY0154D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// Initialize RTC
RTC_DS3231 rtc;

// Initialize NTP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

void setup() {
  // Begin Serial for debugging
  Serial.begin(115200);

  // Initialize I2C (use Xiao ESP32-C3 default pins)
  Wire.begin(6, 7);

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

  unsigned long startAttemptTime = millis();
  const unsigned long timeout = 10000; // 10 seconds timeout

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Wi-Fi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to Wi-Fi");
  }
}

void checkAndSyncTime() {
  if (WiFi.status() != WL_CONNECTED) {
    updateDisplay();
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
  battPercent = map(batt, 3200, 4180, 0, 100);
}

void updateDisplay() {
  DateTime now = rtc.now();
  char timeBuffer[10];
  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", now.hour(), now.minute());

  char dateBuffer[20];
  snprintf(dateBuffer, sizeof(dateBuffer), "%02d %s", now.day(), monthName(now.month()));

  display.firstPage();
  do {
    display.fillScreen(GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);

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
  int x = 158, y = 3;
  display.drawRect(x, y, 30, 15, GxEPD_WHITE); // Battery Outline 
  display.fillRect(x + 31, y + 4, 2, 6, GxEPD_WHITE); // Battery terminal
  int fillWidth = (int)(30 * battPercent / 100); // Scale for 4.2V max
  display.fillRect(x + 1, y + 1, fillWidth, 13, GxEPD_WHITE);
   
  // Uncomment for Battery Status
 display.setCursor(120, 40);
 display.print(String(batteryVoltage) + "V");
 display.setCursor(142, 60);
 display.print(String(battPercent) + "%");

  // Low battery indication
  if (batteryVoltage < 3.2) {
    display.setCursor(140, 80);
    display.print("LOW"); }
}

void sleepCycle() {
  // Protect against deep discharge
  if (batteryVoltage < 3.2) {
    Serial.println("Battery critically low. Shutting down indefinitely to prevent damage.");
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER); // Disable Timer wakeup
    esp_deep_sleep_start(); // Enter indefinite deep sleep
  }
 


  // Configure GPIO wakeup
  esp_sleep_enable_gpio_wakeup();

  // Configure wakeup pins (low level triggers wakeup)
  esp_deep_sleep_enable_gpio_wakeup(BUTTON_1, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_deep_sleep_enable_gpio_wakeup(BUTTON_2, ESP_GPIO_WAKEUP_GPIO_LOW);

  // Enable timer wakeup
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR); // Wakeup timer
   
   
   //Delay Sleep
  delay (delaySleep);
  delaySleep = 0;

  
  // Enter deep sleep
  esp_deep_sleep_start();


}

void buttonLogic() {
  // If Wi-Fi button is pressed, connect to Wi-Fi and update time
  if (digitalRead(BUTTON_WIFI) == LOW) {
    connectToWiFi();
    checkAndSyncTime();
    delaySleep = 0;

  }

  // If the set time button is pressed, enter the Set Time mode
  if (digitalRead(BUTTON_SET) == LOW) {
    updateFirmware();
  }
}

void updateFirmware() {
delaySleep = 180000;

}
