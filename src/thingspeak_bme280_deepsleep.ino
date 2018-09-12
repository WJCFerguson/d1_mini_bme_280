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

// =============================================================================
/// Adafruit_BME280 class tailored to the specific needs
class BME280Handler : public Adafruit_BME280
{
  public:
    bool begin(uint8_t addr)
    {
        bool result = Adafruit_BME280::begin(addr);
        // slow & conservative 1Hz/16x oversampling and significant filtering.
        setSampling(
            MODE_NORMAL,
            TEMPERATURE_FIELD ? SAMPLING_X16 : SAMPLING_NONE,
            PRESSURE_FIELD    ? SAMPLING_X16 : SAMPLING_NONE,
            HUMIDITY_FIELD    ? SAMPLING_X16 : SAMPLING_NONE,
            FILTER_X4,
            STANDBY_MS_1000);
        return result;
    }

    void take_measurement()
    {
        temperature = TEMP_CONV(readTemperature()) + TEMPERATURE_OFFSET;
        pressure = readPressure() / 100.0 + PRESSURE_OFFSET;
        humidity = readHumidity() + HUMIDITY_OFFSET;
    }

  public:
    float temperature;
    float humidity;
    float pressure;
};


// =============================================================================
inline void
timestamp()
{
    static char buf[64];
    snprintf(buf, sizeof(buf), "%9dms :: ", millis());
    Serial.print(buf);
}


// =============================================================================
/// Sleep at time_us microseconds after reset (or time_us from now if
// that's already passed)
inline void
sleep_after(unsigned long time_us)
{
    timestamp();
    Serial.print("Going to sleep until ");
    Serial.println(time_us / 1000);
    unsigned long when = time_us - micros();
    if (when > time_us)
        when = time_us;
    ESP.deepSleep(when);
}


// =============================================================================
void setup() {
    pinMode(D0, WAKEUP_PULLUP);

    Serial.begin(9600);
    while (!Serial)
        ;
    Serial.println();

    // Sensor
    BME280Handler bme;
    if (!bme.begin(BME_I2C_ADDR))
    {
        Serial.println("ERROR: Could not find a valid BME280 sensor.");
        while (true)
            ;
    }
    bme.take_measurement();

    timestamp();
    Serial.println(String(bme.temperature, 1) + "F; " +
                   String(bme.pressure, 1) + "hPa; " +
                   String(bme.humidity, 1) + "%; ");

    // Wifi
    timestamp();
    Serial.print(String("Wifi connecting to ") + wifi_ssid);
    if (wifi_hostname)
        WiFi.hostname(wifi_hostname);
    WiFi.begin(wifi_ssid, wifi_psk);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(10);
        if (millis() % 500 == 0)
            Serial.print(".");
    }
    Serial.println();
    timestamp();
    Serial.print("Connected as ");
    Serial.print(WiFi.localIP());
    Serial.println(String("; RSSI: ") + String(WiFi.RSSI()) + " dBm.");

    // ThingSpeak
    WiFiClient wifi_client;
    ThingSpeak.begin(wifi_client);
    if (TEMPERATURE_FIELD)
        ThingSpeak.setField(TEMPERATURE_FIELD, bme.temperature);
    if (PRESSURE_FIELD)
        ThingSpeak.setField(PRESSURE_FIELD, bme.pressure);
    if (HUMIDITY_FIELD)
        ThingSpeak.setField(HUMIDITY_FIELD, bme.humidity);

    timestamp();
    Serial.print("Writing to ThingSpeak... ");
    int result = ThingSpeak.writeFields(CHANNEL_ID, WRITE_API_KEY);
    if (result == HTTP_CODE_OK)
    {
        Serial.println("OK!");
        sleep_after(upload_period_us);
    }
    Serial.println(String("FAILED (") + result + ")");

    sleep_after(10e6);          // retry in 10s
}


// =============================================================================
void loop() {}

// ==================================== End ====================================
