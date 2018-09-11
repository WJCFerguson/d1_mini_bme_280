// =============================================================================
// Settings for the thingspeak_bme280.ino sketch
// =============================================================================


// ==================================== WiFi ===================================
/*
Wifi ssid/password is configured via git-ignored file wifi_credentials.h.
Cut/paste these two lines into wifi_credentials.h and replace the strings:

const char* wifi_ssid = "xxxxxxxx";
const char* wifi_psk  = "yyyyyyyy";

*/
#include "wifi_credentials.h"

// set to a string if you want a device hostname:
const char* wifi_hostname = 0;

// =================================== BME280 ==================================
static const uint8_t BME_I2C_ADDR = 0x76; // may be 0x77


// ================================= ThingSpeak ================================
const int CHANNEL_ID = xxxxx;
const char* WRITE_API_KEY = "xxxxx";

// set any field to zero to inhibit sending
const unsigned int TEMPERATURE_FIELD = 1;
const unsigned int HUMIDITY_FIELD = 2;
const unsigned int PRESSURE_FIELD = 3;


// ============================= Temperature units =============================
// uncomment one or other of these TEMP_CONV definitions to choose units
// #define TEMP_CONV(val) (val) // Celsius
#define TEMP_CONV(val) (32.0 + val * 9.0 / 5.0) // Fahrenheit


// ========================== Measurement calibration ==========================
// measurements can be calibrated via offsets:
const float TEMPERATURE_OFFSET = 0.0;
const float HUMIDITY_OFFSET = 0.0;
const float PRESSURE_OFFSET = 0.0;


// ============================== Recording period =============================
static const unsigned long upload_period_s = 60;


// ================================ End Settings ===============================
