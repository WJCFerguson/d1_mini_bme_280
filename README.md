# Weather reporting to ThingSpeak from an ESP8266 with an BME280

This is some simple Arduino/ESP8266 `.ino` sketch code to pull together the pieces from other people's hard work.

* Connects to Wifi
* Collects data on a cadence from the ESP8266
* Sends data to ThingSpeak
* Reports progress on Serial

### Prerequsites

Just `make` and `git`, plus either the arduino package or another package that provides `esptool` 

## Building

This was created using a D1 Mini clone and typical 4-pin I2C BME280 package.  Pins:

### Hardware

* 3.3V->VIN
* G->GND
* D1->SCL
* D2->SCA

### Firmware

* Edit the settings at the top of `src/thingspeak_bme280.ino` to set your WiFi and ThingSpeak details.
* If you don't have a D1 Mini, edit the `BOARD` type in `Makefile`
* Plug it in and `make flash`

See below for verification & troubleshooting

## Project Repo using makeEspArduino.mk

The automagic ESP8266 Makefile project [makeEspArduino](https://github.com/plerup/makeEspArduino) does an amazing job of allowing you to do little more than just write your `.ino` code.  

This repo adds a very little more infrastructure that allows you to clone this repo, and on first `make` it will fetch and initialize the required repositories, as git submodules.

# Verification and Troubleshooting

To validate you can build to the device `make blink`, should make and flash an LED blink demo.

To check which I2C Address your BME280 uses, `make enumerate` will make and flash a sketch that enumerates I2C addresses over Serial. Connect e.g. `picocom -b 115200 /dev/ttyUSB1`.

After building the main program with `make flash`, connect with serial and hit reset to start from the beginning, and watch to see that it connects to wifi, finds the sensor, connects to ThingSpeak etc..  You can also connect a browser to the IP address to get fresh readings and confirm some settings.
