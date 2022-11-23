/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-web-server-slider-pwm/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

/*
 * RVG: augmented with different functionality:
 * added FastLED to control WS2811 LED strip
 * buttons to switch certain LED patterns (TODO)
 */

// Import required WiFi libraries
#include <WiFiConfig.h>
#include <WiFi.h> // for both OTA and own functions
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// for the webserver
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Replace with your network credentials
//const char* ssid = "REPLACE_WITH_YOUR_SSID";
//const char* password = "REPLACE_WITH_YOUR_PASSWORD";

// FastLED includes
#include <FastLED.h>

// FastLED defines
#define NUM_LEDS 240 // short Strip for inf mirror: 71 LEDs, long reel: 240 LEDs
#define DATA_PIN 12 // ESP32-CAM: GPIO12/HS2_DATA3
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
// Gradient palette "Sunset_Real_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/atmospheric/tn/Sunset_Real.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.
DEFINE_GRADIENT_PALETTE( Sunset_Real_gp ) {
    0, 120,  0,  0,
   22, 179, 22,  0,
   51, 255,104,  0,
   85, 167, 22, 18,
  135, 100,  0,103,
  198,  16,  0,130,
  255,   0,  0,160};

// FastLED parameters
CRGB leds[NUM_LEDS];
uint8_t paletteIndex = 0;
CRGBPalette16 myPal = Sunset_Real_gp; // Choose your palette
int LEDBrightness; 
int LEDPatternSpeed; 
int LEDPatternDelay;
enum PatternType { PAT_MOV_PALETTE, PAT_CHG_COLOR };
PatternType patternSelected = PAT_MOV_PALETTE; // default pattern

// Web page input parameters
String slider1Value = "60";
String slider2Value = "20";
//String patternValue = "";
const char* PARAM_INPUT = "value";

// PWM props /LED for testing
const int output = 2;
//// setting PWM properties
//const int freq = 5000;
//const int ledChannel = 0;
//const int resolution = 8;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP Web Server</title>
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h2 {font-size: 2.3rem;}
    p {font-size: 1.9rem;}
    body {max-width: 400px; margin:0px auto; padding-bottom: 25px;}
    .slider { -webkit-appearance: none; margin: 14px; width: 360px; height: 25px; background: #FFD65C;
      outline: none; -webkit-transition: .2s; transition: opacity .2s;}
    .slider::-webkit-slider-thumb {-webkit-appearance: none; appearance: none; width: 35px; height: 35px; background: #003249; cursor: pointer;}
    .slider::-moz-range-thumb { width: 35px; height: 35px; background: #003249; cursor: pointer; } 
    .button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px; text-decoration: none; font-size: 20px; margin: 2px; cursor: pointer; }
    .button2 { background-color: #555555; }
  </style>
</head>
<body>
  <h2>Ambient LED Control</h2>
  <p><i>[ESP32 Web Server]</i></p>
  <p><span id="textSlider1Value">Brightness: %SLIDER1VALUE%</span></p>
  <p><input type="range" onchange="updateSlider1PWM(this)" id="pwmSlider1" min="0" max="255" value="%SLIDER1VALUE%" step="1" class="slider"></p>
  <p><span id="textSlider2Value">Speed: %SLIDER2VALUE%</span></p>
  <p><input type="range" onchange="updateSlider2PWM(this)" id="pwmSlider2" min="0" max="100" value="%SLIDER2VALUE%" step="1" class="slider"></p>
  <p>Select pattern:</p>
  <input type="radio" onchange="updateRadio1(this)" id="movingpalette" name="pattern_type" value="MovingPalette" %RADIO1CHECKED% class="button">
  <label for="movingpalette">Moving Palette</label><br>
  <input type="radio" onchange="updateRadio2(this)" id="changingcolor" name="pattern_type" value="ChangingColor" %RADIO2CHECKED% class="button">
  <label for="changingColor">Changing Color</label><br>
<script>
function updateSlider1PWM(element) {
  var slider1Value = document.getElementById("pwmSlider1").value;
  document.getElementById("textSlider1Value").innerHTML = "Brightness: "+slider1Value;
  console.log(slider1Value);
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/slider1?value="+slider1Value, true);
  xhr.send();
}
function updateSlider2PWM(element) {
  var slider2Value = document.getElementById("pwmSlider2").value;
  document.getElementById("textSlider2Value").innerHTML = "Speed: "+slider2Value;
  console.log(slider2Value);
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/slider2?value="+slider2Value, true);
  xhr.send();
}
function updateRadio1(element) {
  var patternValue = document.getElementById("movingpalette").value;
  console.log(patternValue);
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/pattern?value="+patternValue, true);
  xhr.send();
}
function updateRadio2(element) {
  var patternValue = document.getElementById("changingcolor").value;
  console.log(patternValue);
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/pattern?value="+patternValue, true);
  xhr.send();
}
</script>
</body>
</html>
)rawliteral";

// Replaces placeholder with button section in your web page
String processor(const String& var) {
  //Serial.println(var);
  if (var == "SLIDER1VALUE") {
    return slider1Value;
  } else if (var == "SLIDER2VALUE") {
    return slider2Value;
  } else if (var == "RADIO1CHECKED") {
    switch (patternSelected) {
      case PAT_MOV_PALETTE:
        return "checked";
        break;
      case PAT_CHG_COLOR:
        return "";
        break;
    }
  } else if (var == "RADIO2CHECKED") {
    switch (patternSelected) {
      case PAT_MOV_PALETTE:
        return "";
        break;
      case PAT_CHG_COLOR:
        return "checked";
        break;
    }
  }

  return String();
}

void processSlider1Change(int sliderVal) {
//  ledcWrite(ledChannel, sliderVal);
  LEDBrightness = sliderVal/2; // halve the brightness to prevent overbright LEDs
  FastLED.setBrightness(LEDBrightness);
}

void processSlider2Change(int sliderVal) {
  LEDPatternSpeed = sliderVal;
  Serial.print((String)"Speed set to "+LEDPatternSpeed);
  LEDPatternDelay = (int)map(LEDPatternSpeed, 0, 100, 400, 20);
  Serial.println((String)"  Delay set to "+LEDPatternDelay+"ms");
}

void processPatternChange(String patternVal) {
  if (patternVal.equals("MovingPalette")) {
    patternSelected = PAT_MOV_PALETTE;
  } else { // if (patternVal.equals("ChangingColor")) {
    patternSelected = PAT_CHG_COLOR;
  }
}

// The Arduino map() function produces incorrect distribution for integers. See https://github.com/arduino/Arduino/issues/2466
// So, create a fixed function instead:
// map() maps a value of fromLow would get mapped to toLow, a value of fromHigh to toHigh, values in-between to values in-between, etc.
// syntax: map(value, fromLow, fromHigh, toLow, toHigh)
long map2(long x, long in_min, long in_max, long out_min, long out_max) {
        return (x - in_min) * (out_max - out_min+1) / (in_max - in_min+1) + out_min;
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
  Serial.println(__FILE__ " Created:" __DATE__ " " __TIME__);
  
  // Init LED strip
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
//  FastLED.setBrightness(BRIGHTNESS); // set master brightness control
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
  FastLED.clear();
  
//  // configure LED PWM functionalitites
//  ledcSetup(ledChannel, freq, resolution);
//  
//  // attach the channel to the GPIO to be controlled
//  ledcAttachPin(output, ledChannel);

  // process initial values to hardware
  processSlider1Change(slider1Value.toInt());
  processSlider2Change(slider2Value.toInt());
//  ledcWrite(ledChannel, slider1Value.toInt());

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, WiFiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi..");
    delay(1000);
  }

  ////////////////
  // OTA stuff
  ////////////////
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  // ArduinoOTA.setHostname("myesp32");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
  
  // Print ESP Local IP Address
  Serial.println((String)"IP address: "+WiFi.localIP());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // Upon receiving a GET request to <ESP_IP>/slider1?value=<inputMessage>
  server.on("/slider1", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    // GET input1 value on <ESP_IP>/slider1?value=<inputMessage>
    if (request->hasParam(PARAM_INPUT)) {
      inputMessage = request->getParam(PARAM_INPUT)->value();
      slider1Value = inputMessage;
      processSlider1Change(slider1Value.toInt());
    }
    else {
      inputMessage = "No message sent";
    }
    Serial.println(inputMessage);
    request->send(200, "text/plain", "OK");
  });

  // Upon receiving a GET request to <ESP_IP>/slider2?value=<inputMessage>
  server.on("/slider2", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    // GET input2 value on <ESP_IP>/slider2?value=<inputMessage>
    if (request->hasParam(PARAM_INPUT)) {
      inputMessage = request->getParam(PARAM_INPUT)->value();
      slider2Value = inputMessage;
      processSlider2Change(slider2Value.toInt());
    }
    else {
      inputMessage = "No message sent";
    }
    Serial.println(inputMessage);
    request->send(200, "text/plain", "OK");
  });

  // Upon receiving a GET request to <ESP_IP>/slider2?value=<inputMessage>
  server.on("/pattern", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    // GET input value on <ESP_IP>/pattern?value=<inputMessage>
    if (request->hasParam(PARAM_INPUT)) {
      inputMessage = request->getParam(PARAM_INPUT)->value();
//      patternValue = inputMessage;
      processPatternChange(inputMessage);
    }
    else {
      inputMessage = "No message sent";
    }
    Serial.println(inputMessage);
    request->send(200, "text/plain", "OK");
  });
  
  // Start server
  server.begin();
}
  
void loop() {
  ArduinoOTA.handle();
  
  switch (patternSelected) {
    case PAT_MOV_PALETTE:
      // MOVING PALETTE EFFECT
      //fill_palette(led array, nLEDS, startIndex, indexDelta, palette,
      //             brightness, blendType:LINEARBLEND|NOBLEND)
      fill_palette(leds, NUM_LEDS, paletteIndex, 255/NUM_LEDS, myPal, LEDBrightness, LINEARBLEND);
      break;
    case PAT_CHG_COLOR:
      // CHANGE COLOR EFFECT (using palette)
      // CRGB ColorFromPalette(const CRGBPalette16/256 &pal, uint8_t index, uint8_t brightness=255, TBlendType blendType=LINEARBLEND)
      for (int i=0; i<NUM_LEDS; i++) {
        leds[i] = ColorFromPalette(myPal, paletteIndex, LEDBrightness);
      }
      break;
    default:
      break;
  }
  
  EVERY_N_MILLISECONDS_I(patternTimer, LEDPatternDelay) {
//    Serial.println((String)"EVERY_N_MILLISECONDS "+LEDPatternDelay);
    if (LEDPatternSpeed != 0) {
      paletteIndex++; // because index is 8-bits unsigned, it automagically resets to 0 when reaching 256
    }
  }
  patternTimer.setPeriod(LEDPatternDelay);
  
  FastLED.show();  
}
