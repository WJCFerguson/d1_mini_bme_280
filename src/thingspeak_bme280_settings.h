// Settings for bme
#include "wifi_credentials.h"
// make wifi_credentials.h with contents of the form:
// const char* wifi_ssid = "xxxxxxxx";
// const char* wifi_psk  = "yyyyyyyy";

const char* wifi_hostname = 0; // set to string if you want a device hostname

// BME280
static const uint8_t BME_I2C_ADDR = 0x76; // may be 0x77

// ThingSpeak Settings
const int CHANNEL_ID = xxxxx;
const char* WRITE_API_KEY = "xxxxx";

// set any field to zero to not send
const unsigned int TEMPERATURE_FIELD = 1;
const unsigned int HUMIDITY_FIELD = 2;
const unsigned int PRESSURE_FIELD = 3;

// uncomment one or other of these TEMP_CONV definitions to choose units
// #define TEMP_CONV(val) (val) // Celsius
#define TEMP_CONV(val) (32.0 + val * 9.0 / 5.0) // Fahrenheit

// crude calibration of measurements via offsets:
const float TEMPERATURE_OFFSET = 0.0;
const float HUMIDITY_OFFSET = 0.0;
const float PRESSURE_OFFSET = 0.0;

// Behavior
static const unsigned long upload_period_s = 60;

// ================================ End Settings ===============================
