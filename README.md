# Temperature, Humidity and Atmospheric Pressure from an ESP8266 with an BME280

This is some simple Arduino/ESP8266 `.ino` sketch code to pull libraries from other people's hard work.

There are two versions.  One uses deep sleep between measurements to
save power, the other is always on and offers a simple web page with
settings.

They both:

* Connect to Wifi
* monitor temperature, humidity and atmospheric pressure
* Periodically send data to ThingSpeak
* Report and log over Serial

The deep sleep sketch is the default (`make flash`), and the web
sketch can be made with `make SKETCH=src/thingspeak_bme280.ino flash`.

## Prerequsites

* `make`
* `git`
* either the `arduino-mk` package or another package that provides `esptool`.

Also, A dumb terminal emulator like `picocom` is very useful, if only to find out the assigned IP addr.

On Ubuntu/Debian: `sudo apt install make git arduino-mk picocom`.

## Building

#### Hardware

Just takes a little soldering.  Any ESP8266 with I2C pins should work.  This was created using a D1 Mini clone and typical 4-pin I2C BME280 package (total < US$6), connecting pins:

```
| 3.3V | VIN |
| G    | GND |
| D1   | SCL |
| D2   | SCA |
```

#### Firmware

Building and flashing should be trivial.  You'll need to make a ThingSpeak account and channel (free of charge).

* Edit the settings in `src/thingspeak_bme280_settings.h`.
* make `src/wifi_credentials.h` as outlined in `src/thingspeak_bme280_settings.h`
* If you aren't using a D1 Mini, edit the `BOARD` type in `Makefile`
* Plug it in and `make flash`

See below for verification & troubleshooting

## Repo structure

This repo provides a little infrastructure such that on first `make` it will fetch and initialize the required repositories as git submodules:

* The automagic ESP8266 Makefile project [plerup/makeEspArduino](https://github.com/plerup/makeEspArduino) does an amazing job, leaving you do to little more than write your `.ino` code.
* The [esp8266/Arduino](https://github.com/esp8266/Arduino.git) project provides the support that allows Arduino-style sketches on esp8266.
* [adafruit/Adafruit_BME280_Library](https://github.com/adafruit/Adafruit_BME280_Library.git) provides support for the BME280
* [adafruit/Adafruit_Sensor](https://github.com/adafruit/Adafruit_Sensor.git) is needed by `adafruit/Adafruit_BME280_Library`.
* [mathworks/thingspeak-arduino](https://github.com/mathworks/thingspeak-arduino.git) helps upload data to ThingSpeak, but frankly raw HTTP Posts might be almost as easy.

If you extend it and need other libraries, simply cloning them into `./libraries` should be all you need to do.  `makeEspArduino` will automatically find them.

# Verification and Troubleshooting

To validate you can build to the device, plug it in and `make blink`, should make and flash an LED blink demo.

To check which I2C Address your BME280 uses, `make enumerate` will make and flash a sketch that enumerates I2C addresses over Serial. Connect e.g. `picocom /dev/ttyUSB1` (exit `picocom` with `ctrl-a ctrl-c`).

After building the main program with `make flash`, connect with serial and hit reset to start from the beginning, and watch to see that it connects to wifi, finds the sensor, connects to ThingSpeak etc..  You can also connect a browser to the IP address to get fresh readings and confirm some settings.
