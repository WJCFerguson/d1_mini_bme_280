// =====================================================================================
//
// ESP8266 firmware for uploading BME280 sensor data to InfluxDB
//
// =====================================================================================
#include <Adafruit_BME280.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <InfluxDbClient.h>
#include <string.h>

#define STRINGX(arg) #arg
#define STRING(name) STRINGX(name)

Adafruit_BME280 bme;
static const uint8_t BME_I2C_ADDR = 0x76; // may be 0x77
// =============================== TODO: return to false: ==============================
static bool outputSerial = true;

struct Measurement
{
    unsigned int millis;
    float temperature;
    float humidity;
    float pressure;
};


// ====================================== Utility ======================================

// return Stream* that has a user at the other end
inline Stream&
getStream()
{
    // perhaps will get a socket in here some time?
    return Serial;
}


// utility logging fn to log to `getStream()`
void
_log(bool timestamp, const char* format, va_list args)
{
    static const size_t bufLen = 1024;
    static char buffer[bufLen];
    static const char* tsFmt = "%6ld.%03lds ";
    if (timestamp)
    {
        unsigned long now = millis();
        getStream().write(buffer, snprintf(buffer, 16, tsFmt, now / 1000, now % 1000));
    }
    getStream().write(buffer, vsnprintf(buffer, bufLen - 1, format, args));
    getStream().flush();
}


// log with timestamp
void
log(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    _log(true, format, args);
    va_end(args);
}


// Log without timestamp
void
logNoTS(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    _log(false, format, args);
    va_end(args);
}


String
getLineFromUser()
{
    getStream().setTimeout(ULONG_MAX);
    String result = getStream().readStringUntil('\n');
    getStream().setTimeout(1000);
    result.trim();
    return result;
}


void
flushStream()
{
    while (getStream().available())
        getStream().read();
}


void
flashLED(unsigned long flash_duration, unsigned long total_duration = 0)
{
    if (flash_duration > 0)
    {
        digitalWrite(LED_BUILTIN, LOW);
        delay(flash_duration);
        digitalWrite(LED_BUILTIN, HIGH);
    }
    if (total_duration > flash_duration)
        delay(total_duration - flash_duration);
}


// =================================== EEPROM Config ===================================

// 'Safely' restore any `saveData` type from EEPROM at `EEPROM_posn`, providing
// `saveSentinel` is found in EEPROM immediately following it.
//
// Return success value.
template <typename DataType, typename Sentinel>
bool
safeRestoreFromEEPROM(DataType& data, const Sentinel& saveSentinel, size_t EEPROM_posn)
{
    EEPROM.begin(EEPROM_posn + sizeof(saveSentinel) + sizeof(DataType));
    // ensure sentinel was written after
    char readSentinel[sizeof(saveSentinel)];
    EEPROM.get(EEPROM_posn + sizeof(DataType), readSentinel);
    if (0 != strncmp(readSentinel, saveSentinel, sizeof(saveSentinel)))
    {
        log("No data found in EEPROM\r\n");
        return false;
    }
    // read data
    EEPROM.get(EEPROM_posn, data);
    log("Data fetched from EEPROM\r\n");
    return true;
}


// Write `saveData` to EEPROM at EEPROM_posn, followed by `saveSentinel`
template <typename DataType, typename Sentinel>
void
safeSaveToEEPROM(const DataType& data, const Sentinel& saveSentinel, size_t EEPROM_posn)
{
    EEPROM.begin(EEPROM_posn + sizeof(saveSentinel) + sizeof(DataType));
    EEPROM.put(EEPROM_posn, data);
    EEPROM.put(EEPROM_posn + sizeof(DataType), saveSentinel);
    EEPROM.commit();
    log("Saved data to EEPROM\r\n");
}


// Sentinel data, stored in EEPROM to ensure we're reading data we stored; should really
// be a checksum:
static const char EEPROM_saveSentinel[16] = "bme_280_1.0    ";
// non-zero arbitrary start location in EEPROM, to avoid wearing out from byte 0
static const size_t EEPROM_start = 33;


// Macros to facilitate setting members by member name string:

// evaluate whether `nameStr` matches `identifier` to at least `minMatchLen`
#define MEMBER_MATCH(identifier, nameStr, minMatchLen)                                   \
    0 == strncmp(STRING(identifier),                                                     \
                 nameStr.c_str(),                                                        \
                 max(nameStr.length(), static_cast<unsigned int>(minMatchLen)))

// if MEMBER_MATCH then set char[] `identifier` to `valueStr`; return success
#define MEMBER_STRING_SET(identifier, nameStr, minMatchLen, valueStr)                    \
    (MEMBER_MATCH(identifier, nameStr, minMatchLen)                                      \
         ? (bool)strncpy(identifier, value.c_str(), sizeof(identifier) - 1)              \
         : false)


// =====================================================================================
// Structure for all persistent configuration.
struct Config
{
    // ==================================== Members ====================================

    // wifi:
    char ssid[33]; // all must have an extra for nulls
    char psk[65];
    char hostname[33];

    // general
    bool fahrenheit;
    unsigned int update_period_s;

    // sensor calibration values
    float temp_offset_C;

    // InfluxDB config
    char url[128];
    char org[32];
    char token[128];
    char bucket[32];
    char location[32];

    // ==================================== Methods ====================================
    Config()
    {
        memset(this, 0, sizeof(Config));
        update_period_s = 300;
    }

    void dump()
    {
        logNoTS("\r\n\n"
                "====================================================================\r\n"
                "Settings:\r\n"
                "Wifi:\r\n"
                "  ssid            = \"%s\"\r\n"
                "  psk             = \"%s\"\r\n"
                "  hostname        = \"%s\"\r\n",
                ssid,
                psk,
                hostname);
        logNoTS("Config\r\n"
                "  update_period_s = %d\r\n"
                "  fahrenheit      = %s\r\n"
                "Calibration values\r\n"
                "  temp_offset_C   = %.2f (NOTE: Celcius)\r\n",
                update_period_s,
                fahrenheit ? "yes" : "no",
                temp_offset_C);
        logNoTS("InfluxDB Settings\r\n"
                "  url             = \"%s\"\r\n"
                "  org             = \"%s\" (InfluxDB 1: name)\r\n"
                "  token           = \"%s\" (InfluxDB 1: leave blank)\r\n"
                "  bucket          = \"%s\" (InfluxDB 1: leave blank)\r\n"
                "  location        = \"%s\" (Influx record tag)\r\n"
                "\n\n",
                url,
                org,
                token,
                bucket,
                location);
    }

    bool minimallyConfigured()
    {
        // all (v2) or nothing (v1) for InfluxDB token and bucket;
        bool versionOneOrTheOther = (strlen(token) && strlen(bucket)) ||
                                    !(strlen(token) || strlen(bucket));
        return strlen(ssid) && strlen(psk) && strlen(hostname) && update_period_s > 0 &&
               strlen(url) && versionOneOrTheOther;
    }

    void ensureWifiConfigured()
    {
        // only set things if they're not already right.  I'm convinced WiFi doesn't like to
        // be disturbed...
        if (WiFi.hostname() != hostname)
            WiFi.hostname(hostname);
        if (WiFi.SSID() != ssid || WiFi.psk() != psk)
        {
            log("Configuring new ssid\r\n");
            WiFi.begin(ssid, psk);
        }
        if (!WiFi.getAutoConnect())
            WiFi.setAutoConnect(true);
    }

    void edit()
    {
        flushStream();
        while (true)
        {
            dump();
            log("Enter e.g. \"ssid=MyWiFi\"\r\n");
            log(" - shortest unique name prefix is accepted, e.g. \"ss=MyWiFi\"\r\n");
            if (minimallyConfigured())
                log("Or enter \"!\" to quit\r\n");
            log("> ");

            String tspBuf = getLineFromUser();
            if (tspBuf[0] == '!' && minimallyConfigured())
            {
                log("Finished editing config\r\n");
                safeSaveToEEPROM(*this, EEPROM_saveSentinel, EEPROM_start);
                break;
            }
            if (tspBuf.isEmpty() || tspBuf.substring(0, 1) == "#")
            {
                continue;
            }
            int equals = tspBuf.indexOf('=');
            if (equals == -1)
            {
                log("\r\n");
                log("****************************************\r\n");
                log("Unrecognized or ambiguous entry:\r\n");
                log("\"%s\"\r\n", tspBuf.c_str());
                log("****************************************\r\n");
                continue;
            }
            String name = tspBuf.substring(0, equals);
            String value = tspBuf.substring(equals + 1);
            if ((MEMBER_STRING_SET(ssid, name, 2, value) ||
                 MEMBER_STRING_SET(psk, name, 1, value) ||
                 MEMBER_STRING_SET(hostname, name, 1, value) ||
                 MEMBER_STRING_SET(url, name, 2, value) ||
                 MEMBER_STRING_SET(org, name, 1, value) ||
                 MEMBER_STRING_SET(token, name, 2, value) ||
                 MEMBER_STRING_SET(bucket, name, 1, value) ||
                 MEMBER_STRING_SET(location, name, 1, value)))
            {
            } else if (MEMBER_MATCH(temp_offset_C, name, 2))
                temp_offset_C = value.toFloat();
            else if (MEMBER_MATCH(update_period_s, name, 2))
                update_period_s = value.toInt() > 0 ? value.toInt() : update_period_s;
            else if (MEMBER_MATCH(fahrenheit, name, 1))
            {
                char firstChar = toupper(value.c_str()[0]);
                fahrenheit = firstChar == 'Y' || firstChar == 'T' || value.toInt() != 0;
            } else
                log("Unrecognized or ambiguous value name \"%s\".\r\n", name.c_str());
        }
        ensureWifiConfigured();
    }
};

Config config;


void
restoreConfig()
{
    if (!safeRestoreFromEEPROM(config, EEPROM_saveSentinel, EEPROM_start))
    {
        log("A1");
        // if we couldn't restore our config, adopt any WiFi itself has
        if (WiFi.SSID().length())
        {
            strncpy(config.ssid, WiFi.SSID().c_str(), sizeof(config.ssid) - 1);
            strncpy(config.psk, WiFi.psk().c_str(), sizeof(config.psk) - 1);
        }
        config.edit();
    } else
        config.ensureWifiConfigured();
}


void
writeConfig()
{
    safeSaveToEEPROM(config, EEPROM_saveSentinel, EEPROM_start);
}


// =================================== Setup and loop ==================================

// Wait until connected; user has option to reconfigure while waiting
void
awaitWiFiConnected()
{
    if (!config.minimallyConfigured())
        config.edit();
    while (!WiFi.isConnected())
    {
        if (!WiFi.isConnected())
            log("Connecting to \"%s\"...\r\n", WiFi.SSID().c_str());
        log("Hit a key to reconfigure\r\n");
        for (int wait = 0; wait < 20000 && !WiFi.isConnected(); wait += 500)
        {
            if (getStream().available())
                config.edit();
            flashLED(wait > 2000 ? 1 : 0, 500);
        }
        if (0 != WiFi.status())
        {
            log("Connection to %s failed\r\n", config.ssid);
            WiFi.printDiag(getStream());
        }
    }
    log("Connection established!\r\n");
    log("IP address:\t%s\r\n", WiFi.localIP().toString().c_str());
}


// Set up the sensor and wait until a valid measurement should be readable
void
setupBME()
{
    while (!bme.begin(BME_I2C_ADDR))
    {
        log("ERROR: sensor.begin() failed; will retry\r\n");
        if (getStream().available())
            config.edit();
        delay(1000);
    }

    bme.setTemperatureCompensation(config.temp_offset_C);
    bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                    Adafruit_BME280::SAMPLING_X16,
                    Adafruit_BME280::SAMPLING_X16,
                    Adafruit_BME280::SAMPLING_X16,
                    Adafruit_BME280::FILTER_X8,
                    Adafruit_BME280::STANDBY_MS_0_5);
    // 16x sampling at 0.5ms -> 8ms; with X8 filter, 12 cycles should get us a stable
    // reading
    delay(100);
    log("BME 280 configured\r\n");
}


// If there is pending Serial input, handle it
void
handleUserIO()
{
    bool userWasActive = outputSerial;
    while (getStream().available())
    {
        outputSerial = true;
        switch (getStream().read())
        {
        case '\n':
        case 'c': {
            config.edit();
            flushStream();
            awaitWiFiConnected();
            float temp_comp = bme.getTemperatureCompensation();
            if ((temp_comp - config.temp_offset_C) > 0.01)
            {
                log("Changed pressure compensation to %f\r\n", config.temp_offset_C);
                bme.setTemperatureCompensation(config.temp_offset_C);
            }
            log("Continuing...\r\n");
        }
        break;
        case 't':
            userWasActive = !userWasActive;
            log("Toggled Logging %s.\r\n", userWasActive ? "on" : "off");
            break;
        default:
            log("'c' to edit config; 't' to toggle logging on.\r\n");
            break;
        }
        outputSerial = userWasActive;
    }
}


Measurement
fetchMeasurement()
{
    Measurement result;
    result.temperature = 1000;
    while (true)
    {
        result.temperature = bme.readTemperature();
        if (config.fahrenheit)
            result.temperature = 32.0 + (result.temperature / 5) * 9;
        result.pressure = bme.readPressure() / 100.0;
        result.humidity = bme.readHumidity();
        result.millis = millis();
        if (result.temperature < 150 && result.temperature > -100)
        {
            log("measurement fetched\r\n");
            log("%5.1f%s; %.1fhPa; %.1f%%\r\n",
                result.temperature,
                config.fahrenheit ? "F" : "C",
                result.pressure,
                result.humidity);
            return result;
        }
        log("ERROR: implausible temperature measurement (%f); will retry\r\n",
            result.temperature);
        handleUserIO();
        delay(1000);
    }
}


void
publishMeasurement(const Measurement& measurement)
{
    InfluxDBClient client = [&] {
        if (strlen(config.bucket) && strlen(config.token))
            return InfluxDBClient(config.url, config.org, config.bucket, config.token);
        else
            return InfluxDBClient(config.url, config.org);
    }();

    if (!client.validateConnection())
    {
        log("Failed to validate InfluxDb connection.  \"%s\"\r\n",
            client.getLastErrorMessage().c_str());
        return;
    }
    Point pointDevice("bme280");
    // Set tags
    pointDevice.addTag("device", config.hostname);
    pointDevice.addTag("SSID", WiFi.SSID());
    pointDevice.addTag("IP", WiFi.localIP().toString());
    pointDevice.addTag("location", config.location);

    // Add data
    pointDevice.addField("rssi", WiFi.RSSI());
    pointDevice.addField("millis", measurement.millis);
    pointDevice.addField("temperature", measurement.temperature);
    pointDevice.addField("humidity", measurement.humidity);
    pointDevice.addField("pressure", measurement.pressure);
    // Write data
    if (!client.writePoint(pointDevice))
    {
        log("Failed to write to InfluxDb connection.  \"%s\"\r\n",
            client.getLastErrorMessage().c_str());
        return;
    }
    log("Wrote to InfluxDB\r\n");
}


/// Sleep until `time_us` microseconds after last reset (or time_us from now if
/// that's already passed)
inline void
sleepAfterReset(unsigned long time_us)
{
    pinMode(D0, WAKEUP_PULLUP);
    log("Going to sleep until %.2f\r\n", time_us / 1e6);
    unsigned long when = time_us - micros();
    if (when > time_us)
        when = time_us;
    ESP.deepSleep(when);
}


void
setup()
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    restoreConfig();
    setupBME();
    // fetch the measurement without delay after startup, before the BME280 auto-heats
    Measurement measurement = fetchMeasurement();
    awaitWiFiConnected();
    publishMeasurement(measurement);
    handleUserIO(); // last chance before sleep

    // give them 2 seconds to connect
    for (int wait = 0; wait < 2000; wait += 250)
    {
        if (getStream().available())
            config.edit();
        flashLED(1, 250);
    }

    // our work here is done; go to sleep and do not enter loop()
    sleepAfterReset(config.update_period_s * 1e6);
}


// NOTE: loop() GENERALLY NOT  CALLED.  We fell asleep before we got here.
void
loop()
{
}
