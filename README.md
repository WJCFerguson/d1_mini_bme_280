# Project Repo using makeEspArduino.mk

The automagic ESP8266 Makefile project [makeEspArduino](https://github.com/plerup/makeEspArduino) does an amazing job of automagicaly working.  This a very little more automagic to allow you to clone this repo and type `make flash` and flash the Blink demo to your board.

This defines git submodules for [makeEspArduino](https://github.com/plerup/makeEspArduino) and [esp8266/Arduino](https://github.com/esp8266/Arduino)  that will be resolved and prepared on first `make` or if you type `make submodules`

### Prerequsites

Just `make` and `git`.

## Usage

Clone this repo, replace the main.ino with your preferred code, type `make flash`

Beyond handling the submodules, all the hard work is done by [makeEspArduino](https://github.com/plerup/makeEspArduino) - refer to its docs.
