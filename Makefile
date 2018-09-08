ROOT_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
ESP_ROOT = $(ROOT_DIR)/esp8266

BOARD ?= d1_mini
SKETCH ?= src/thingspeak_bme280.ino
CUSTOM_LIBS ?= ./libraries

# sentinels for rebuilding submodules and fetching tools
esp8266/tools/xtensa-lx106-elf: submodules

.PHONY: submodules
submodules:
	git submodule update --init --recursive
	(cd esp8266/tools; python get.py)
	if [ "$(MAKECMDGOALS)" != "submodules" ]; then	\
	  $(MAKE) $(MAKECMDGOALS);				\
	fi

# useful hello-world to check it's working
blink:
	$(MAKE) SKETCH=esp8266/libraries/esp8266/examples/Blink/Blink.ino flash

# useful I2C enumeration sketch
enumerate:
	$(MAKE) SKETCH=src/enumerate.ino flash

-include ./makeEspArduino/makeEspArduino.mk
