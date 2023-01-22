#include "iot_config.h"

#include "clock.h"

#include <IotWebConf.h>

// Name of this IoT object.
#define THING_NAME "WordClockLT"
// Initial WiFi access point password.
#define INITIAL_WIFI_AP_PASSWORD "12345678"
// IoT configuration version. Change this whenever IotWebConf object's
// configuration structure changes.
#define CONFIG_VERSION "v1"
// Port used by the IotWebConf HTTP server.
#define WEB_SERVER_PORT 80
// -- Status indicator pin.
//    Rapid blinking (mostly on, interrupted by short off periods) - Entered Access Point mode. This means the device create an own WiFi network around it. You can connect to the device with your smartphone (or WiFi capable computer).
//    Alternating on/off blinking - Trying to connect the configured WiFi network.
//    Mostly off with occasional short flash - The device is online..
#define IOT_WEB_STATUS_PIN 2
// -- set up a push button. If push button is detected
//    to be pressed at boot time, the Thing will use the initial
//    password for accepting connections in Access Point mode. This
//    is usefull e.g. when custom password was lost.
#define IOT_AP_CONFIG_PIN 4
// -- setup an indicator LED for the NTP time state
//    Off             - Waiting, no NTP activity
//    Blinking        - Connecting to NTP server to sync time
//    On continuously - Connected to NTP server, using NTP time for the clock
#define NTP_STATUS_PIN 16
#define NTP_BLINK_MS 300

// HTTP OK status code.
#define HTTP_OK 200

// HTTP MIME type.
#define MIME_HTTP "text/html"

// CLOCK ========================================================================
// Maintains local time, using NTP servers plus timezone settings
#define CLK_SVR1   "pool.ntp.org"
#define CLK_SVR2   "europe.pool.ntp.org"
#define CLK_SVR3   "north-america.pool.ntp.org"

//#define CLK_TZ     "CET-1CEST,M3.5.0,M10.5.0/3" // Amsterdam
#define CLK_TZ     "EST5EDT,M3.2.0,M11.1.0" // America/New-York
#define NTP_LT_TIMEOUT 3000 // [ms] waiting time for time from NTP server

namespace {

// Attempts to parse `str` as a date in yyyy-mm-dd format. If it succeeds,
// returns true and populates `year`, `month` and `day` with the corresponding
// date values. If it fails, returns false and leaves other parameters unchanged.
bool parseDateValue(const char* str, uint16_t* year, uint8_t* month,
                    uint8_t* day) {
    unsigned int parsed_year;
    unsigned int parsed_month;
    unsigned int parsed_day;

    int result = sscanf(str, "%u-%u-%u", &parsed_year, &parsed_month,
                        &parsed_day);
    if (result != 3 || parsed_year < 2000 || parsed_year > 9999 ||
            parsed_month == 0 || parsed_month > 12 || parsed_day == 0 ||
            parsed_day > 31) {
        Serial.print("[INFO] Could not parse date value \"");
        Serial.print(str);
        Serial.println("\".");
        return false;
    }

    *year = parsed_year;
    *month = parsed_month;
    *day = parsed_day;
    return true;
}

// Attempts to parse `str` as a time in hh:mm:ss format. If it succeeds, returns
// true and populates `hour`, `minute` and `second` with the corresponding time
// values. If it fails, returns false and leaves other parameters unchanged.
bool parseTimeValue(const char* str, uint8_t* hour, uint8_t* minute,
                    uint8_t* second) {
    unsigned int parsed_hour;
    unsigned int parsed_minute;
    unsigned int parsed_second;

    int result = sscanf(str, "%u:%u:%u", &parsed_hour, &parsed_minute,
                        &parsed_second);
    if (result != 3 || parsed_hour > 23 || parsed_minute > 59 ||
            parsed_second > 59) {
        Serial.print("[INFO] Could not parse time value \"");
        Serial.print(str);
        Serial.println("\".");
        return false;
    }

    *hour = parsed_hour;
    *minute = parsed_minute;
    *second = parsed_second;
    return true;
}

// Attempts to parse the provided `date_str` and `time_str` as date and time,
// respectively. If set values are successfully parsed, adjusts `rtc` to the new
// date and time.
void parseAndSetDateTime(WordClock* word_clock, const char* date_str,
                         const char* time_str) {
    const DateTime now = word_clock->getCurrentTime();
    uint16_t year = now.year();
    uint8_t month = now.month();
    uint8_t day = now.day();
    uint8_t hour = now.hour();
    uint8_t minute = now.minute();
    uint8_t second = now.second();
    bool datetime_changed = false;

    if (date_str[0] != 0 && parseDateValue(date_str, &year, &month, &day)) {
        datetime_changed = true;
    }
    if (time_str[0] != 0 && parseTimeValue(time_str, &hour, &minute, &second)) {
        datetime_changed = true;
    }

    if (datetime_changed) {
        //rtc->adjust(DateTime(year, month, day, hour, minute, second));
    }
}

// Attempts to parse `str` as a color in "#RRGGBB" format. If it succeeds,
// returns the parsed value. If it fails, returns `default_value`.
RgbColor parseColorValue(const char* str, const RgbColor& default_value) {
    bool has_error = str[0] != '#' || str[7] != 0;
    for (int i = 1; i <= 6; i++) {
        has_error &= !isxdigit(str[i]);
    }
    if (has_error) {
        Serial.print("[INFO] Could not parse color value \"");
        Serial.print(str);
        Serial.println("\".");
        return default_value;
    }

    const int parsed_value = strtol(str + 1, nullptr, 16);
    const uint8_t red = (parsed_value >> 16) & 0xFF;
    const uint8_t green = (parsed_value >> 8) & 0xFF;
    const uint8_t blue = parsed_value & 0xFF;
    return RgbColor(red, green, blue);
}

// Attempts to parse `str` as a number in the interval `[min_value, max_value].
// If it succeeds, returns the parsed value. If it fails, returns
// `default_value`.
int parseNumberValue(const char* str, int min_value, int max_value,
                     int default_value) {
    char* end_ptr = nullptr;
    int parsed_value = strtol(str, &end_ptr, 10);
    if (*end_ptr != 0 || parsed_value < min_value || parsed_value > max_value) {
        Serial.print("[INFO] Could not parse number value \"");
        Serial.print(str);
        Serial.println("\".");
        return default_value;
    }
    return parsed_value;
}

}  // namespace

IotConfig::IotConfig(WordClock* word_clock)
    : web_server_(WEB_SERVER_PORT), word_clock_(word_clock),
      datetime_separator_("Date and time"),
      date_param_("Date", "date", date_value_, IOT_CONFIG_VALUE_LENGTH, "date",
                  "yyyy-mm-dd", nullptr, "pattern='\\d{4}-\\d{1,2}-\\d{1,2}'"),
      time_param_("Time", "time", time_value_, IOT_CONFIG_VALUE_LENGTH, "time",
                  "hh:mm:ss", nullptr,
                  "pattern='\\d{1,2}:\\d{1,2}:\\d{1,2}' step='1'"),
      dst_param_("Daylight saving time? (0=false, 1=true)", "dst", dst_value_,
                 IOT_CONFIG_VALUE_LENGTH, "number", "0", "0",
                 "pattern='[01]' min='0' max='1' "
                 "style='max-width: 2em; display: block;'"),
      appearance_separator_("Appearance"),
      palette_id_param_(
              "Color palette number (0=custom)", "palette_id", palette_id_value_,
              IOT_CONFIG_VALUE_LENGTH, "number", "1", "1",
              "pattern='\\d+' min='0' max='7' "
              "style='max-width: 2em; display: block;'"),
      color_1_param_("Custom color 1", "color_1", color_1_value_,
                     IOT_CONFIG_VALUE_LENGTH, "color", "#RRGGBB", "#BE0900",
                     "pattern='#[0-9a-fA-F]{6}' "
                     "style='border-width: 1px; padding: 1px;'"),
      color_2_param_("Custom color 2", "color_2", color_2_value_,
                     IOT_CONFIG_VALUE_LENGTH, "color", "#RRGGBB", "#CB5B0A",
                     "pattern='#[0-9a-fA-F]{6}' "
                     "style='border-width: 1px; padding: 1px;'"),
      color_3_param_("Custom color 3", "color_3", color_3_value_,
                     IOT_CONFIG_VALUE_LENGTH, "color", "#RRGGBB", "#FECC5C",
                     "pattern='#[0-9a-fA-F]{6}' "
                     "style='border-width: 1px; padding: 1px;'"),
      period_param_("Show period? (0=false, 1=true)", "period", period_value_,
                    IOT_CONFIG_VALUE_LENGTH, "number", "0", "0",
                    "pattern='[01]' min='0' max='1' "
                    "style='max-width: 2em; display: block;'"),
      debug_separator_("Debug"),
      clock_mode_param_(
              "Clock mode (0=real clock)", "clock_mode", clock_mode_value_,
              IOT_CONFIG_VALUE_LENGTH, "number", "0", "0",
              "pattern='\\d+' min='0' max='6' "
              "style='max-width: 2em; display: block;'"),
      fast_time_factor_param_(
              "Fast time factor", "fast_time_factor", fast_time_factor_value_,
              IOT_CONFIG_VALUE_LENGTH, "number", "30", "30",
              "pattern='\\d+' min='1' max='3600' "
              "style='max-width: 4em; display: block;'"),
      iot_web_conf_(THING_NAME, &dns_server_, &web_server_,
                    INITIAL_WIFI_AP_PASSWORD, CONFIG_VERSION) {}

IotConfig::~IotConfig() {}

void IotConfig::clearTransientParams_() {
    date_value_[0] = 0;
    time_value_[0] = 0;
}

// Note that IotWebConf does not currently work with parameters that require an
// HTML checkbox input to set in the configuration portal, so we have to use the
// workaround of representing booleans as 0 or 1 integers.
void IotConfig::updateClockFromParams_() {
    Serial.println("=IotConfig::updateClockFromParams_()");
    parseAndSetDateTime(word_clock_, date_value_, time_value_);

    word_clock_->setClockMode(static_cast<ClockMode>(
            parseNumberValue(clock_mode_value_, 0,
                             static_cast<int>(ClockMode::MAX_VALUE),
                             static_cast<int>(ClockMode::REAL_TIME))));
    word_clock_->setDst(static_cast<bool>(
            parseNumberValue(dst_value_, 0, 1, 0)));
    word_clock_->setFastTimeFactor(
            parseNumberValue(fast_time_factor_value_, 1, 3600, 30));
    word_clock_->setPaletteId(
            parseNumberValue(palette_id_value_, 0, PALETTE_COUNT, 1));
    word_clock_->setPeriod(static_cast<bool>(
            parseNumberValue(period_value_, 0, 1, 0)));

    word_clock_->setCustomColor1(
            parseColorValue(color_1_value_, RgbColor(190, 9, 0)));
    word_clock_->setCustomColor2(
            parseColorValue(color_2_value_, RgbColor(203, 91, 10)));
    word_clock_->setCustomColor3(
            parseColorValue(color_3_value_, RgbColor(254, 204, 92)));
}

void IotConfig::handleHttpToRoot_() {
    static const char html[] =
        "<!DOCTYPE html>"
        "<html lang='en'>"
        "<head>"
            "<meta name='viewport' content='width=device-width, "
                    "initial-scale=1, user-scalable=no'/>"
            "<title>Word clock LT</title>"
        "</head>"
        "<body>"
            "<h1>Word Clock LT</h1>"
            "<ul><li><a href='config'>Settings</a></li></ul>"
        "</body>"
        "</html>\n";

    if (iot_web_conf_.handleCaptivePortal()) return;
    web_server_.send(HTTP_OK, MIME_HTTP, html);
}

void IotConfig::handleHttpToConfig_() {
    clearTransientParams_();
    iot_web_conf_.handleConfig();
}

void IotConfig::handleConfigSaved_() {
    updateClockFromParams_();
}

void IotConfig::wifiConnected_() {
  Serial.println("WiFi was connected. Initiating NTP proces...");
  needNTPConnect_ = true;
  NTPState_ = NTP_Connecting;
}

// Default NTP poll time is SNTP_UPDATE_DELAY (sntp.c, value 3600000 ms or 1 hour).
// Change with sntp_set_update_delay(ms)
//  RVG changed this since Arduino time implementation doesn't seem to support the function prototype below
//  configTime(CLK_TZ, CLK_SVR1, CLK_SVR2, CLK_SVR3);
void IotConfig::connectNTP_() {
  struct tm timeinfo;
  bool gotLocalTime = false;
  String timezone(CLK_TZ);
  
  Serial.println((String)"connectNTP_: Connecting to NTP server: "+CLK_SVR1);
  configTime(0, 0, CLK_SVR1);    // First connect to NTP server, with 0 TZ offset
  Serial.printf(" Setting Timezone to %s\n",timezone.c_str());
  setenv("TZ", timezone.c_str(),1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
  
  Serial.print(" NTP: Obtaining local time");
  gotLocalTime = getLocalTime(&timeinfo, 10);
  Serial.print(&timeinfo, " %A, %B %d %Y %H:%M:%S");
  Serial.printf(" (%d)\n", gotLocalTime);
}

bool IotConfig::isNTPConnected_() {
  struct tm timeinfo;
  bool getLTStatus = getLocalTime(&timeinfo, 10);
  return getLTStatus;
}

void IotConfig::updateNTPLEDStatus_() {
  static bool ntpPinHigh = false;
  static unsigned long lastBlinkTime = 0;
  unsigned long now;
  
  switch (NTPState_) {
    case NTP_Waiting:
      ntpPinHigh = false;
      break;
    case NTP_Connecting:
      now = millis();
      if (now - lastBlinkTime > NTP_BLINK_MS) {
        ntpPinHigh = !ntpPinHigh;
        lastBlinkTime = now;
      }
      break;
    case NTP_Connected:
      ntpPinHigh = true;
      break;
    default:
      break;
  }

  digitalWrite(NTP_STATUS_PIN, ntpPinHigh ? HIGH : LOW);
}
void IotConfig::setup() {
    Serial.println("=IotConfig::setup()");
    if (initialized_) {
        Serial.println("[WARN] Trying to setup IotConfig multiple times.");
        return;
    }

    // Getting NTP time is not in iot_web_conf. So pin config here
    pinMode(NTP_STATUS_PIN, OUTPUT);
    updateNTPLEDStatus_();

    // Here we can set IotWebConf's status and config reset pins, if available.
    // iot_web_conf_.setStatusPin(LEDC_PIN);
    iot_web_conf_.setConfigPin(IOT_AP_CONFIG_PIN);
    iot_web_conf_.setStatusPin(IOT_WEB_STATUS_PIN);

    iot_web_conf_.addParameter(&datetime_separator_);
    iot_web_conf_.addParameter(&date_param_);
    iot_web_conf_.addParameter(&time_param_);
    iot_web_conf_.addParameter(&dst_param_);
    iot_web_conf_.addParameter(&appearance_separator_);
    iot_web_conf_.addParameter(&palette_id_param_);
    iot_web_conf_.addParameter(&color_1_param_);
    iot_web_conf_.addParameter(&color_2_param_);
    iot_web_conf_.addParameter(&color_3_param_);
    iot_web_conf_.addParameter(&period_param_);
    iot_web_conf_.addParameter(&debug_separator_);
    iot_web_conf_.addParameter(&clock_mode_param_);
    iot_web_conf_.addParameter(&fast_time_factor_param_);

    iot_web_conf_.setConfigSavedCallback([this](){ handleConfigSaved_(); });
    iot_web_conf_.setWifiConnectionCallback([this](){ wifiConnected_(); });

    iot_web_conf_.init();

    clearTransientParams_();
    updateClockFromParams_();

    web_server_.on("/", [this](){ handleHttpToRoot_(); });
    web_server_.on("/config", [this](){ handleHttpToConfig_(); });
    web_server_.onNotFound([this](){ iot_web_conf_.handleNotFound(); });

    initialized_ = true;
}

void IotConfig::loop() {
    if (!initialized_) {
        Serial.println("[ERROR] IotConfig not initialized, loop aborted.");
        return;
    }

    updateNTPLEDStatus_(); // controls the LED pin
    if (needNTPConnect_) {
      connectNTP_();
      needNTPConnect_ = false;
    }
    if (NTPState_ == NTP_Connecting) {
      if (isNTPConnected_()) {
        Serial.println("NTP connection succesful");
        NTPState_ = NTP_Connected;
      }
    }

    iot_web_conf_.doLoop();
}
