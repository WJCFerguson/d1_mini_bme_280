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
#include <ESP8266WebServer.h>
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


// =============================== Static objects ==============================
BME280Handler bme;
static unsigned long next_upload_micros = 0;
WiFiClient wifi_client;
ESP8266WebServer web_server(80);


// =============================================================================
// serve the 'web page'
void handleRoot() {
    Serial.println("Web request received");
    bme.take_measurement();
    web_server.send(
        200,
        "text/plain",
        String(wifi_hostname) +
        "(RSSI: " + String(WiFi.RSSI()) + " dBm).\n\nMeasurements:\n" +
        "  Temperature: " + String(bme.temperature, 1) + " F\n" +
        "  Humidity   : " + String(bme.humidity, 1) + " %\n" +
        "  Pressure   : " + String(bme.pressure, 1) + " hPa\n\n" +
        String("Calibration Offsets:\n") +
        "  Temperature: " + String(TEMPERATURE_OFFSET, 1) + " F\n" +
        "  Humidity   : " + String(HUMIDITY_OFFSET, 1) + " %\n" +
        "  Pressure   : " + String(PRESSURE_OFFSET, 1) + " hPa\n\n" +
        String("ThingSpeak Fields:\n") +
        "  Temperature: " + String(TEMPERATURE_FIELD) + "\n" +
        "  Humidity   : " + String(HUMIDITY_FIELD) + "\n" +
        "  Pressure   : " + String(PRESSURE_FIELD) + "\n");
}


// =============================================================================
void setup() {
    Serial.begin(9600);

    // Sensor
    if (!bme.begin(BME_I2C_ADDR))
    {
        Serial.println("ERROR: Could not find a valid BME280 sensor.");
        while (true);
    }
    Serial.println("Sensor found");

    // Wifi
    Serial.print(String("Wifi connecting to ") + wifi_ssid);
    if (wifi_hostname)
        WiFi.hostname(wifi_hostname);
    WiFi.begin(wifi_ssid, wifi_psk);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("Connected as ");
    Serial.print(WiFi.localIP());
    Serial.println(String("; RSSI: ") + String(WiFi.RSSI()) + " dBm");

    // ThingSpeak
    ThingSpeak.begin(wifi_client);

    // Web web_server
    web_server.on("/", handleRoot);
    web_server.begin();
    Serial.print("Web serving on http://");
    Serial.println(WiFi.localIP());

    // Set up next sample time.
    next_upload_micros = micros();
}

// =============================================================================
void send_values()
{
    static const int RETRIES = 5;

    bme.take_measurement();

    for (int i = 1; i <= RETRIES; ++i)
    {
        if (TEMPERATURE_FIELD)
            ThingSpeak.setField(TEMPERATURE_FIELD, bme.temperature);
        if (PRESSURE_FIELD)
            ThingSpeak.setField(PRESSURE_FIELD, bme.pressure);
        if (HUMIDITY_FIELD)
            ThingSpeak.setField(HUMIDITY_FIELD, bme.humidity);

        Serial.print(String(millis() / 1000.0, 2) + "s: " +
                     String(bme.temperature, 1) + "F; " +
                     String(bme.pressure, 1) + "hPa; " +
                     String(bme.humidity, 1) + "%; " +
                     "Writing to ThingSpeak (attempt " + i + ")... ");
        int result = ThingSpeak.writeFields(CHANNEL_ID, WRITE_API_KEY);
        if (result == HTTP_CODE_OK)
        {
            Serial.println("OK!");
            break;
        }
        Serial.println(String("FAILED (") + result + ")");
        delay(15 * 1000);       // ThingSpeak rate-limits to one every 15s
    }
}


// =============================================================================
void loop()
{
    if ((micros() - next_upload_micros) < 0x80000000) // handle uint wraparound
    {
        send_values();
        next_upload_micros = next_upload_micros + (upload_period_us);
    }
    web_server.handleClient();
}

// ==================================== End ====================================
