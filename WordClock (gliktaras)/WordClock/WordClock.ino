//#include "clock.h"
#include "Display.h"
#include "ClockFace.h"
#include "iot_config.h"

#include <IotWebConf.h>
#include <NeoPixelBus.h>

// Baud rate of the serial output.
#define SERIAL_BAUD_RATE 115200

// Not all LEDC frequency and duty value combinations are valid. See
// https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/ledc.html#supported-range-of-frequency-and-duty-resolutions
// for details.

//// Built-in LED pin number.
//#define LEDC_PIN 4
//// LED channel to use for the built-in LED.
//#define LEDC_CHANNEL 0
//// Built-in LED timer resolution.
//#define LEDC_RESOLUTION 13
//// Built-in LED base frequency.
//#define LEDC_BASE_FREQUENCY 5000
//// Maximum duty for the built-in LED.
//#define LEDC_MAX_DUTY (1 << 13 - 1)

// Light sensor pin number.
//#define LDR_PIN 25

// LED strip pin number.
//#define NEOPIXEL_PIN 32
// Number of LEDs in the LED strip.
#define NEOPIXEL_COUNT 114
// Number of LEDs in a single row of the grid.
#define PIXEL_GRID_WIDTH 11

namespace {

// The LED strip.
//NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> led_strip(
//        NEOPIXEL_COUNT, NEOPIXEL_PIN);
// Clock Display state.
//WordClock word_clock(&led_strip);
EnglishClockFace clockFace(ClockFace::LightSensorPosition::Bottom);
Display display(clockFace);
// IoT configuration portal.
//IotConfig iot_config(&word_clock);
IotConfig iot_config(&display);

}  // namespace

// Initializes sketch.
void setup() {
    // Initialize serial output.
    Serial.begin(SERIAL_BAUD_RATE);
    while (!Serial);

    // Initialize built-in board LED.
//    pinMode(LEDC_PIN, OUTPUT);
//    ledcSetup(LEDC_CHANNEL, LEDC_BASE_FREQUENCY, LEDC_RESOLUTION);
//    ledcAttachPin(LEDC_PIN, LEDC_CHANNEL);
//    ledcWrite(LEDC_CHANNEL, 0);

    // Initialize light sensor.
//    pinMode(LDR_PIN, INPUT);

    // Initialize real time clock.
//    const bool rtc_valid = rtc.begin();
//    if (!rtc_valid) {
//        Serial.println("[ERROR] Could not initialize RTC.");
//    } else if (rtc.lostPower()) {
//        Serial.println("[WARN] RTC lost power, resetting ");
//        rtc.adjust(DEFAULT_DATETIME);
//    }

    // Initialize the remaining components.
//    led_strip.Begin();
//    word_clock.setup();
    display.setup();
    iot_config.setup();
}

// Prints program debug state to Serial output.
//void printDebugState() {
//    Serial.println("==========");
//    Serial.print("Light sensor: ");
//    Serial.print(analogRead(LDR_PIN));
//    Serial.println("");
//}

// Executes the event loop once.
void loop() {
  //struct tm: tm_year, tm_mon, tm_mday, tm_hour, tm_min, tm_sec
  struct tm timeinfo;

// NOTE: the second arg in getLocalTime() is the max [ms] the function waits 
//       for the time to be received from the NTP server. Be aware to set it to
//       a low value to prevent blocking issues!
//       See the following resources:
// https://github.com/espressif/arduino-esp32/blob/34125cee1d1c6a81cd1da07de27ce69d851f9389/cores/esp32/esp32-hal-time.c#L87
// https://techtutorialsx.com/2021/09/01/esp32-system-time-and-sntp/
// https://www.lucadentella.it/en/2017/05/11/esp32-17-sntp/
  getLocalTime(&timeinfo, 10); 
  display.updateForTime(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  iot_config.loop();
//  word_clock.loop();
  display.loop();
}
