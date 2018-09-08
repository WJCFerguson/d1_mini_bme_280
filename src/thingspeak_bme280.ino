// =============================================================================
// Fetch the temperature over I2C from a BME280 sensor, and upload it
// to a ThingSpeak channel.
// =============================================================================
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ThingSpeak.h>

// ================================== Settings =================================
// BME280
static const uint8_t BME_I2C_ADDR = 0x76; // may be 0x77

// Wi-Fi
const char* ssid = 'xxxxx';
const char* password = 'xxxxxxxx';

// ThingSpeak Settings
const int CHANNEL_ID = xxxxx;
const char* WRITE_API_KEY = "xxxxx";

// set any field to zero to not send
const unsigned int TEMPERATURE_FIELD = 1;
const unsigned int HUMIDITY_FIELD = 2;
const unsigned int PRESSURE_FIELD = 3;

// crude calibration of measurements via offsets:
const float TEMPERATURE_OFFSET = 0.0;
const float HUMIDITY_OFFSET = 0.0;
const float PRESSURE_OFFSET = 0.0;

// Behavior
static const unsigned long upload_period_s = 60;

// ================================ End Settings ===============================

// =============================================================================
/// Tailored version of the Adafruit_BME280 class to our specific needs
class BME280Handler : public Adafruit_BME280
{
  public:
    bool begin(uint8_t addr)
    {
        bool result = Adafruit_BME280::begin(addr);
        // recommended settings for weather/climate monitoring
        setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X1, // temperature
                    Adafruit_BME280::SAMPLING_X1, // pressure
                    Adafruit_BME280::SAMPLING_X1, // humidity
                    Adafruit_BME280::FILTER_OFF);
        return result;
    }

    void take_measurement()
    {
        static const float F_MULT = 9.0 / 5.0;
        takeForcedMeasurement(); // in forced mode, so must do this first
        temperature = 32 + TEMPERATURE_OFFSET + readTemperature() * F_MULT;
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
static unsigned long next_upload_millis = 0;
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
        String("Measurements\n") +
        "  Temperature: " + String(bme.temperature, 1) + " F\n" +
        "  Humidity   : " + String(bme.humidity, 1) + " %\n" +
        "  Pressure   : " + String(bme.pressure, 1) + " hPa\n\n" +
        String("Offsets:\n") +
        "  Temperature: " + String(TEMPERATURE_OFFSET, 1) + " F\n" +
        "  Humidity   : " + String(HUMIDITY_OFFSET, 1) + " %\n" +
        "  Pressure   : " + String(PRESSURE_OFFSET, 1) + " hPa\n\n" +
        String("Fields:\n") +
        "  Temperature: " + String(TEMPERATURE_FIELD) + "\n" +
        "  Humidity   : " + String(HUMIDITY_FIELD) + "\n" +
        "  Pressure   : " + String(PRESSURE_FIELD) + "\n");
}


// =============================================================================
void setup() {
    Serial.begin(115200);

    // Sensor
    if (!bme.begin(BME_I2C_ADDR))
    {
        Serial.println("ERROR: Could not find a valid BME280 sensor.");
        while (true);
    }
    Serial.println("Sensor found");

    // Wifi
    Serial.print("Wifi connecting");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.print("Connected as ");
    Serial.println(WiFi.localIP());

    // ThingSpeak
    ThingSpeak.begin(wifi_client);

    // Web web_server
    web_server.on("/", handleRoot);
    web_server.begin();
    Serial.println("Web serving");

    // Set up next sample time.
    next_upload_millis = millis();
}

// =============================================================================
void send_values()
{
    bme.take_measurement();
    Serial.println(String(millis() / 1000.0, 2) + "s >>> " +
                   "Temp: " + String(bme.temperature, 1) + " F" +
                   "; Pressure: " + String(bme.pressure, 1) + " hPa" +
                   "; Humidity: " + String(bme.humidity, 1) + " %");
    if (TEMPERATURE_FIELD)
        ThingSpeak.setField(TEMPERATURE_FIELD, bme.temperature);
    if (PRESSURE_FIELD)
        ThingSpeak.setField(PRESSURE_FIELD, bme.pressure);
    if (HUMIDITY_FIELD)
        ThingSpeak.setField(HUMIDITY_FIELD, bme.humidity);
    ThingSpeak.writeFields(CHANNEL_ID, WRITE_API_KEY);
    Serial.println("Wrote to ThingSpeak");
}


// =============================================================================
void loop()
{
    if ((millis() - next_upload_millis) < 0x80000000) // handle uint wraparound
    {
        send_values();
        next_upload_millis = next_upload_millis + (upload_period_s * 1000);
    }
    web_server.handleClient();
}

// ==================================== End ====================================
