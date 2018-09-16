// =============================================================================
// Fetch the temperature over I2C from a BME280 sensor, and upload it
// to a ThingSpeak channel.
// =============================================================================
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ThingSpeak.h>

#include "thingspeak_bme280_settings.h"


inline void ts() { Serial.printf("%9dms :: ", millis()); } // timestamp


// =============================================================================
/// Sleep at time_us microseconds after reset (or time_us from now if
/// that's already passed)
inline void
sleep_after(unsigned long time_us) {
    pinMode(D0, WAKEUP_PULLUP);
    ts(); Serial.print("Going to sleep until ");
    Serial.println(time_us / 1000);
    unsigned long when = time_us - micros();
    if (when > time_us)
        when = time_us;
    ESP.deepSleep(when);
}


// =============================================================================
void setup() {
    // initialize BME280 asap, to get it to forced mode.  Some of them
    // auto-heat when in MODE_NORMAL
    Adafruit_BME280 bme;
    bool bme_ok = bme.begin(BME_I2C_ADDR);
    if (bme_ok) {
        bme.setSampling(Adafruit_BME280::MODE_FORCED,
                        Adafruit_BME280::SAMPLING_X1,
                        Adafruit_BME280::SAMPLING_X1,
                        Adafruit_BME280::SAMPLING_X1);
    }

    Serial.begin(9600);
    while (!Serial) ;
    Serial.println();
    if (!bme_ok) {
        Serial.println("ERROR: Failed to find BME sensor.");
        while (true);
    }

    // Wifi
    ts(); Serial.printf("Wifi connecting to %s as %s\r\n",
                        wifi_ssid,
                        wifi_hostname ? wifi_hostname : "<unset>");
    if (wifi_hostname)
        WiFi.hostname(wifi_hostname);
    WiFi.begin(wifi_ssid, wifi_psk);
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() > 30e3) {
            ts(); Serial.println("Failed to connect; sleeping");
            sleep_after(upload_period_us);
        }
        delay(100);
    }
    ts(); Serial.printf("Connected as %s; RSSI: %d\r\n",
                        WiFi.localIP().toString().c_str(),
                        WiFi.RSSI());

    // take measurements
    bme.takeForcedMeasurement();
    float temperature = TEMP_CONV(bme.readTemperature()) + TEMPERATURE_OFFSET;
    float pressure = bme.readPressure() / 100.0 + PRESSURE_OFFSET;
    float humidity = bme.readHumidity() + HUMIDITY_OFFSET;

    ts(); Serial.printf("%5.1fF; %.1fhPa; %.1f%%\r\n",
                        temperature, pressure, humidity);

    // ThingSpeak
    WiFiClient wifi_client;
    ThingSpeak.begin(wifi_client);
    if (TEMPERATURE_FIELD)
        ThingSpeak.setField(TEMPERATURE_FIELD, temperature);
    if (PRESSURE_FIELD)
        ThingSpeak.setField(PRESSURE_FIELD, pressure);
    if (HUMIDITY_FIELD)
        ThingSpeak.setField(HUMIDITY_FIELD, humidity);
    if (RSSI_FIELD)
        ThingSpeak.setField(RSSI_FIELD, WiFi.RSSI());

    ts(); Serial.print("Writing to ThingSpeak... ");
    int result = ThingSpeak.writeFields(CHANNEL_ID, WRITE_API_KEY);
    if (result != HTTP_CODE_OK) {
        Serial.printf("FAILED (%d)\r\n", result);
        sleep_after(upload_period_us / 10); // retry in 1/10 period
    }
    Serial.println("OK!");
    sleep_after(upload_period_us);
}


// =============================================================================
void loop() {}


// ==================================== End ====================================
