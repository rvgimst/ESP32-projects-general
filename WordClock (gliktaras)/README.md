# Word clock

Firmware and face plate designs for a word clock project documented [in this
gallery](https://photos.app.goo.gl/JyxsdS8WfhfHDBUL9).

## Face plate design

The files are as follows:

-   `clean.svg`: Base layout of the clock's body.
-   `final_text.svg`: Final face plate design with editable text.
-   `final_to_print.svg`: Design ready to be submitted to a laser cutter.

## Software setup

-   Install [Arduino IDE](https://www.arduino.cc/en/main/software).
-   Install
    [CP210x USB to UART Bridge VCP Drivers](https://www.silabs.com/products/development-tools/software/usb-to-uart-bridge-vcp-drivers).
-   Open Arduino IDE.
-   Add [Arduino core for the ESP32](https://github.com/espressif/arduino-esp32)
    to the IDE, as described in its documentation (version >= 1.0.4).
-   Go to Sketch -> Include Library -> Manage Libraries... and install the
    following dependencies:
    -   [IotWebConf](https://github.com/prampec/IotWebConf) by Balazs Keleman (>=
        2.3.0).
    -   [NeoPixelBus](https://github.com/Makuna/NeoPixelBus) by Makuna (>=
        2.5.3).
    -   [RTClib](https://github.com/adafruit/RTClib) by Adafruit (>= 1.3.3).
-   Set the current board to "ESP32 Dev Module" (Tools -> Board).
-   Set the current port (Tools -> Port).

At this point you should be able to open the sketch, compile and upload it to the
clock's board. If the upload fails, you may need to install latest
[esptool](https://github.com/espressif/esptool) as well.
