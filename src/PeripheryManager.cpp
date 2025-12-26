#include <PeripheryManager.h>
#include "Adafruit_SHT31.h"
#include "Adafruit_BME280.h"
#include "Adafruit_BMP280.h"
#include "Adafruit_HTU21DF.h"
#include "SoftwareSerial.h"
#include <DFMiniMp3.h>
#include <MelodyPlayer/melody_player.h>
#include <MelodyPlayer/melody_factory.h>
#include "Globals.h"
#include "DisplayManager.h"
#include "MQTTManager.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <LightDependentResistor.h>
#include <MenuManager.h>
#include <ServerManager.h>
#include <MedianFilterLib.h>
#include <MeanFilterLib.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

const int buzzerPin = 2;       // Buzzer an GPIO2
const int baudRate = 50;       // Nachrichtenübertragungsrate
const char *message = "HELLO"; // Die Nachricht, die gesendet werden soll
#define LEDC_CHANNEL 0
#define LEDC_RESOLUTION 8 // 8 bit resolution
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define MEDIAN_WND 7 // A median filter window size of seven should be enough to filter out most spikes
#define MEAN_WND 7   // After filtering the spikes we don't need many samples anymore for the average

#define DFPLAYER_RX 23
#define DFPLAYER_TX 18
#define BUZZER_PIN 15
#define RESET_PIN 13

#ifdef awtrix2_upgrade
// Pinouts für das WEMOS_D1_MINI32-Environment
#define LDR_PIN A0
#define BUTTON_UP_PIN D0
#define BUTTON_DOWN_PIN D8
#define BUTTON_SELECT_PIN D4

#define I2C_SCL_PIN D1
#define I2C_SDA_PIN D3
#elif ESP32_S3
#define BATTERY_PIN 4
#define BUZZER_PIN 5
#define LDR_PIN 6
#define BUTTON_UP_PIN 7
#define BUTTON_DOWN_PIN 8
#define BUTTON_SELECT_PIN 10
#define I2C_SCL_PIN 10
#define I2C_SDA_PIN 11
#else
// Pinouts für das ULANZI-Environment
#define BATTERY_PIN 34

#define LDR_PIN 35
#define BUTTON_UP_PIN 26
#define BUTTON_DOWN_PIN 14
#define BUTTON_SELECT_PIN 27
#define I2C_SCL_PIN 22
#define I2C_SDA_PIN 21
#endif

Adafruit_BME280 bme280;
Adafruit_BMP280 bmp280;
Adafruit_HTU21DF htu21df;
Adafruit_SHT31 sht31;

#ifdef awtrix2_upgrade
#define USED_PHOTOCELL LightDependentResistor::GL5528
#define PHOTOCELL_SERIES_RESISTOR 1000
#else
#define USED_PHOTOCELL LightDependentResistor::GL5516
#define PHOTOCELL_SERIES_RESISTOR 10000
#endif

class Mp3Notify
{
};
SoftwareSerial mySoftwareSerial(DFPLAYER_RX, DFPLAYER_TX); // RX, TX
DFMiniMp3<SoftwareSerial, Mp3Notify> dfmp3(mySoftwareSerial);

MelodyPlayer player(BUZZER_PIN, 1, LOW);

EasyButton button_left(BUTTON_UP_PIN);
EasyButton button_right(BUTTON_DOWN_PIN);
EasyButton button_select(BUTTON_SELECT_PIN);
EasyButton button_reset(RESET_PIN);

LightDependentResistor photocell(LDR_PIN,
                                 PHOTOCELL_SERIES_RESISTOR,
                                 USED_PHOTOCELL,
                                 10,
                                 10);

int readIndex = 0;
int sampleIndex = 0;
unsigned long previousMillis_BatTempHum = 0;
unsigned long previousMillis_LDR = 0;
const unsigned long interval_BatTempHum = 10000;
const unsigned long interval_LDR = 100;
int total = 0;
unsigned long startTime;

MedianFilter<uint16_t> medianFilterBatt(MEDIAN_WND);
MedianFilter<uint16_t> medianFilterLDR(MEDIAN_WND);
MeanFilter<uint16_t> meanFilterBatt(MEAN_WND);
MeanFilter<uint16_t> meanFilterLDR(MEAN_WND);

float brightnessPercent = 0.0;
static void fetchPvPowerFromApi();
static bool refreshPvToken();
static bool tokenInvalid(int httpCode, const String &response);
static void reloadPvDemoSettings();
static void applyPvDemoValues();
static uint32_t lastPvPoll = 0;
static const uint32_t pvPollIntervalMs = 60000; // poll every 60 seconds
static const char *pvRefreshUrl = "https://gateway.isolarcloud.eu/openapi/auth/refreshToken";

PeripheryManager_::PeripheryManager_()
{
    this->buttonL = &button_left;
    this->buttonR = &button_right;
    this->buttonS = &button_select;
    this->buttonRST = &button_reset;
}

// The getter for the instantiated singleton instance
PeripheryManager_ &PeripheryManager_::getInstance()
{
    static PeripheryManager_ instance;
    return instance;
}

// Initialize the global shared instance
PeripheryManager_ &PeripheryManager = PeripheryManager.getInstance();

void left_button_pressed()
{
    if (!BLOCK_NAVIGATION)
    {
        if (DFPLAYER_ACTIVE)
            PeripheryManager.playFromFile(DFMINI_MP3_CLICK);

        DisplayManager.leftButton();
        MenuManager.leftButton();
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Left button clicked"));
    }
    else
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Left button clicked but blocked"));
    }
}

void right_button_pressed()
{
    if (!BLOCK_NAVIGATION)
    {
        if (DFPLAYER_ACTIVE)
            PeripheryManager.playFromFile(DFMINI_MP3_CLICK);

        DisplayManager.rightButton();
        MenuManager.rightButton();
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Right button clicked"));
    }
    else
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Right button clicked but blocked"));
    }
}

void select_button_pressed()
{
    if (!BLOCK_NAVIGATION)
    {
        if (DFPLAYER_ACTIVE)
            PeripheryManager.playFromFile(DFMINI_MP3_CLICK);

        DisplayManager.selectButton();
        MenuManager.selectButton();
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Select button clicked"));
    }
    else
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Select button clicked but blocked"));
    }
}

void reset_button_pressed_long()
{
    ServerManager.erase();
    ESP.restart();
}

void select_button_pressed_long()
{
    if (DFPLAYER_ACTIVE)
        PeripheryManager.playFromFile(DFMINI_MP3_CLICK);
    if (AP_MODE)
    {
        ++MATRIX_LAYOUT;
        if (MATRIX_LAYOUT < 0)
            MATRIX_LAYOUT = 2;
        saveSettings();
        ESP.restart();
    }
    else if (!BLOCK_NAVIGATION)
    {
        MenuManager.selectButtonLong();
        DisplayManager.selectButtonLong();
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Select button pressed long"));
    }
}

void select_button_double()
{
    if (DEBUG_MODE)
        DEBUG_PRINTLN(F("Select button double pressed"));
    if (!BLOCK_NAVIGATION)
    {
        if (DFPLAYER_ACTIVE)
            PeripheryManager.playFromFile(DFMINI_MP3_CLICK);

        if (MATRIX_OFF)
        {
            DisplayManager.setPower(true);
        }
        else
        {
            DisplayManager.setPower(false);
        }
    }
}

void PeripheryManager_::playBootSound()
{
    if (DEBUG_MODE)
        DEBUG_PRINTLN(F("Playing bootsound"));
    if (!SOUND_ACTIVE)
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Sound output disabled"));
        return;
    }

    if (BOOT_SOUND == "")
    {
        if (DFPLAYER_ACTIVE)
        {
            playFromFile(DFMINI_MP3_BOOT);
        }
        else
        {
            const int nNotes = 6;
            String notes[nNotes] = {"E5", "C5", "G4", "E4", "G4", "C5"};
            const int timeUnit = 150;
            Melody melody = MelodyFactory.load("Bootsound", timeUnit, notes, nNotes);
            player.playAsync(melody);
        }
    }
    else
    {
        playFromFile(BOOT_SOUND);
    }
}

void PeripheryManager_::stopSound()
{
    if (DFPLAYER_ACTIVE)
    {
        dfmp3.stopAdvertisement();
        delay(50);
        dfmp3.stop();
    }
    else
    {
        player.stop();
    }
}

void PeripheryManager_::setVolume(uint8_t vol)
{
    if (DFPLAYER_ACTIVE)
    {
        uint8_t curVolume = dfmp3.getVolume(); // need to read volume in order to work. Donno why! :(
        dfmp3.setVolume(vol);
        delay(50);
    }
    else
    {
        int scaledVol = (vol * 255) / 30;
        player.setVolume(scaledVol);
    }
}

bool PeripheryManager_::parseSound(const char *json)
{
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error)
    {
        return playFromFile(String(json));
    }
    if (doc.containsKey("sound"))
    {
        return playFromFile(doc["sound"].as<String>());
    }
    return false;
}

const char *PeripheryManager_::playRTTTLString(String rtttl)
{
    if (!DFPLAYER_ACTIVE)
    {
        static char melodyName[64];
        Melody melody = MelodyFactory.loadRtttlString(rtttl.c_str());
        player.playAsync(melody);
        strncpy(melodyName, melody.getTitle().c_str(), sizeof(melodyName));
        melodyName[sizeof(melodyName) - 1] = '\0';
        return melodyName;
    }
}

const char *PeripheryManager_::playFromFile(String file)
{
    if (!SOUND_ACTIVE)
        return "";

    if (DFPLAYER_ACTIVE)
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Playing MP3 file"));
        if (!DFPLAYER_ACTIVE)
            return NULL;
        dfmp3.stop();
        delay(50);
        dfmp3.playMp3FolderTrack(file.toInt());

        return file.c_str();
    }
    else
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Playing RTTTL sound file"));
        if (LittleFS.exists("/MELODIES/" + String(file) + ".txt"))
        {
            static char melodyName[64];
            Melody melody = MelodyFactory.loadRtttlFile("/MELODIES/" + String(file) + ".txt");
            player.playAsync(melody);
            strncpy(melodyName, melody.getTitle().c_str(), sizeof(melodyName));
            melodyName[sizeof(melodyName) - 1] = '\0';
            return melodyName;
        }
        else
        {
            return NULL;
        }
    }
}

bool PeripheryManager_::isPlaying()
{
    if (DFPLAYER_ACTIVE)
    {
        if ((dfmp3.getStatus() & 0xff) == 0x01) // 0x01 = DfMp3_StatusState_Playing
            return true;
        else
            return false;
    }
    else
    {
        return player.isPlaying();
    }
}

void PeripheryManager_::setup()
{
    if (DEBUG_MODE)
        DEBUG_PRINTLN(F("Setup periphery"));
    startTime = millis();
    pinMode(LDR_PIN, INPUT);
    pinMode(RESET_PIN, INPUT);
    if (DFPLAYER_ACTIVE)
    {
        dfmp3.begin();
        delay(100);
        setVolume(SOUND_VOLUME);
    }
    button_left.begin();
    button_right.begin();
    button_select.begin();
    button_reset.begin();

    if (ROTATE_SCREEN)
    {
        Serial.println("Button rotation");
        button_left.onPressed(right_button_pressed);
        button_right.onPressed(left_button_pressed);
    }
    else
    {
        button_left.onPressed(left_button_pressed);
        button_right.onPressed(right_button_pressed);
    }

    button_select.onPressed(select_button_pressed);
    button_select.onPressedFor(1000, select_button_pressed_long);
    button_select.onSequence(2, 300, select_button_double);

#ifdef ULANZI
    button_reset.onPressedFor(5000, reset_button_pressed_long);
#endif

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    if (bme280.begin(BME280_ADDRESS) || bme280.begin(BME280_ADDRESS_ALTERNATE))
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("BME280 sensor detected"));
        TEMP_SENSOR_TYPE = TEMP_SENSOR_TYPE_BME280;
    }
    else if (bmp280.begin(BMP280_ADDRESS) || bmp280.begin(BMP280_ADDRESS_ALT))
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("BMP280 sensor detected"));
        TEMP_SENSOR_TYPE = TEMP_SENSOR_TYPE_BMP280;
    }
    else if (htu21df.begin())
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("HTU21DF sensor detected"));
        TEMP_SENSOR_TYPE = TEMP_SENSOR_TYPE_HTU21DF;
    }
    else if (sht31.begin(0x44))
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("SHT31 sensor detected"));
        TEMP_SENSOR_TYPE = TEMP_SENSOR_TYPE_SHT31;
    }

#ifdef awtrix2_upgrade
    dfmp3.begin();
#else

#endif
    photocell.setPhotocellPositionOnGround(false);
}

void PeripheryManager_::tick()
{
    if (!MenuManager.inMenu)
    {
        if (ROTATE_SCREEN)
        {
            MQTTManager.sendButton(2, button_left.read());
            ServerManager.sendButton(2, button_left.read());
            MQTTManager.sendButton(0, button_right.read());
            ServerManager.sendButton(0, button_right.read());
        }
        else
        {
            MQTTManager.sendButton(0, button_left.read());
            MQTTManager.sendButton(2, button_right.read());
            ServerManager.sendButton(0, button_left.read());
            ServerManager.sendButton(2, button_right.read());
        }

        MQTTManager.sendButton(1, button_select.read());
        ServerManager.sendButton(1, button_select.read());
    }
    else
    {
        button_left.read();
        button_select.read();
        button_right.read();
    }

    button_reset.read();

    unsigned long currentMillis_BatTempHum = millis();
    if (currentMillis_BatTempHum - previousMillis_BatTempHum >= interval_BatTempHum)
    {
        previousMillis_BatTempHum = currentMillis_BatTempHum;
#ifndef awtrix2_upgrade
        uint16_t ADCVALUE = analogRead(BATTERY_PIN);
        // Discard values that are totally out of range, especially the first value read after a reboot.
        // Meaningful values for an Ulanzi clock are in the range 400..700
        if ((ADCVALUE > 100) && (ADCVALUE < 1000))
        {
            // Send ADC values through median filter to get rid of the remaining spikes and then calculate the average
            BATTERY_RAW = meanFilterBatt.AddValue(medianFilterBatt.AddValue(ADCVALUE));
            BATTERY_PERCENT = max(min((int)map(BATTERY_RAW, MIN_BATTERY, MAX_BATTERY, 0, 100), 100), 0);
            SENSORS_STABLE = true;
        }
        fetchPvPowerFromApi();

#else
        SENSORS_STABLE = true;
#endif
        if (SENSOR_READING)
        {
            switch (TEMP_SENSOR_TYPE)
            {
            case TEMP_SENSOR_TYPE_BME280:
                CURRENT_TEMP = bme280.readTemperature();
                CURRENT_HUM = bme280.readHumidity();
                break;
            case TEMP_SENSOR_TYPE_BMP280:
                CURRENT_TEMP = bmp280.readTemperature();
                CURRENT_HUM = 0;
                break;
            case TEMP_SENSOR_TYPE_HTU21DF:
                CURRENT_TEMP = htu21df.readTemperature();
                CURRENT_HUM = htu21df.readHumidity();
                break;
            case TEMP_SENSOR_TYPE_SHT31:
                sht31.readBoth(&CURRENT_TEMP, &CURRENT_HUM);
                break;
            default:
                CURRENT_TEMP = 0;
                CURRENT_HUM = 0;
                break;
            }

            CURRENT_TEMP += TEMP_OFFSET;
            CURRENT_HUM += HUM_OFFSET;
        }
        else
        {
            SENSORS_STABLE = true;
        }
    }

    unsigned long currentMillis_LDR = millis();
    if (currentMillis_LDR - previousMillis_LDR >= interval_LDR)
    {
        previousMillis_LDR = currentMillis_LDR;

        uint16_t LDRVALUE = analogRead(LDR_PIN);

        // Send LDR values through median filter to get rid of the remaining spikes and then calculate the average
        LDR_RAW = meanFilterLDR.AddValue(medianFilterLDR.AddValue(LDRVALUE));
        CURRENT_LUX = (roundf(photocell.getSmoothedLux() * 1000) / 1000);
        if (AUTO_BRIGHTNESS && !MATRIX_OFF)
        {
            brightnessPercent = (LDR_RAW * LDR_FACTOR) / 1023.0 * 100.0;
            brightnessPercent = pow(brightnessPercent, LDR_GAMMA) / pow(100.0, LDR_GAMMA - 1);
            BRIGHTNESS = map(brightnessPercent, 0, 100, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
            DisplayManager.setBrightness(BRIGHTNESS);
        }
    }
}

static void fetchPvPowerFromApi()
{
    reloadPvDemoSettings();

    if (PV_DEMO_MODE)
    {
        applyPvDemoValues();
        return;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("PV API: skip, WiFi not connected"));
        return;
    }

    if (PV_ACCESS_TOKEN.isEmpty() || PV_ACCESS_KEY.isEmpty() || PV_DEVICE_APPKEY.isEmpty() || PV_DEVICE_SN.isEmpty())
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("PV API: skip, missing credentials"));
        return;
    }

    uint32_t now = millis();
    if (lastPvPoll != 0 && (uint32_t)(now - lastPvPoll) < pvPollIntervalMs)
        return;

    lastPvPoll = now;

    WiFiClientSecure client;
    client.setInsecure(); // TODO: pin the gateway certificate for production use

    HTTPClient http;
    const char *url = "https://gateway.isolarcloud.eu/openapi/platform/getDeviceRealTimeData";
    if (!http.begin(client, url))
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("PV API: begin failed"));
        return;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", ("Bearer " + PV_ACCESS_TOKEN).c_str());
    http.addHeader("User-Agent", "awtrix3/1.0");
    http.addHeader("lang", "_en_US");
    http.addHeader("x-access-key", PV_ACCESS_KEY.c_str());

    StaticJsonDocument<512> payload;
    payload["appkey"] = PV_DEVICE_APPKEY;
    payload["device_type"] = "14";
    payload["is_get_point_dict"] = "0";
    JsonArray pointIds = payload.createNestedArray("point_id_list");
    pointIds.add("13112");
    pointIds.add("13141");
    pointIds.add("13003");
    JsonArray snList = payload.createNestedArray("sn_list");
    snList.add(PV_DEVICE_SN);

    String body;
    serializeJson(payload, body);

    int httpCode = http.POST(body);
    String responseBody = http.getString();
    if (DEBUG_MODE)
    {
        DEBUG_PRINTF("PV API: HTTP %d, payload bytes %u, resp bytes %u", httpCode, (unsigned int)body.length(), (unsigned int)responseBody.length());
        DEBUG_PRINTF("PV API: response body: %s", responseBody.c_str());
    }
    if (httpCode == HTTP_CODE_OK)
    {
        DynamicJsonDocument resp(4096);
        DeserializationError err = deserializeJson(resp, responseBody);
        if (err)
        {
            if (DEBUG_MODE)
                DEBUG_PRINTF("PV API: parse failed (%s)", err.c_str());
        }
        else
        {
            uint16_t total = 0;
            float soc = 0;
            float energyWh = 0;
            JsonArray devicePoints = resp["result_data"]["device_point_list"];
            if (!devicePoints.isNull() && devicePoints.size() > 0)
            {
                for (JsonObject item : devicePoints)
                {
                    JsonObject dp = item["device_point"];
                    if (dp.isNull())
                        continue;
                    float power = dp["p13003"].as<float>();
                    soc = dp["p13141"].as<float>();
                    energyWh = dp["p13112"].as<float>();
                    total += static_cast<uint16_t>(power);
                    if (DEBUG_MODE)
                        DEBUG_PRINTF("PV API: device_point p13003=%.2f p13141=%.2f p13112=%.2f", power, soc, energyWh);
                }
            }
            else if (DEBUG_MODE)
            {
                DEBUG_PRINTLN(F("PV API: no point list in response"));
            }

            PV_Power_total = total;
            PV_Battery_SOC = soc;
            // API energy is reported in Wh; convert to kWh for display
            PV_Energy_Daily = energyWh / 1000.0f;
            if (DEBUG_MODE)
                DEBUG_PRINTF("PV API: total %u W, SOC %.2f, Energy %.3f kWh", PV_Power_total, PV_Battery_SOC, PV_Energy_Daily);
        }
    }
    else if (DEBUG_MODE)
    {
        DEBUG_PRINTF("PV API: HTTP %d", httpCode);
    }

    http.end();

    // Handle token expiry and retry once with refreshed token
    if (tokenInvalid(httpCode, responseBody))
    {
        if (refreshPvToken())
        {
            lastPvPoll = 0; // allow immediate retry
            fetchPvPowerFromApi();
        }
    }
}

static bool tokenInvalid(int httpCode, const String &response)
{
    if (httpCode == HTTP_CODE_UNAUTHORIZED)
        return true;

    if (response.indexOf("Invalid access token") >= 0)
        return true;

    return false;
}

static bool refreshPvToken()
{
    if (WiFi.status() != WL_CONNECTED)
        return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    if (!http.begin(client, pvRefreshUrl))
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("PV token refresh: begin failed"));
        return false;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("User-Agent", "awtrix3/1.0");

    StaticJsonDocument<256> payload;
    payload["appkey"] = PV_DEVICE_APPKEY;
    if (!PV_REFRESH_TOKEN.isEmpty())
        payload["refresh_token"] = PV_REFRESH_TOKEN;
    else if (DEBUG_MODE)
        DEBUG_PRINTLN(F("PV token refresh: no refresh_token set, sending without token"));

    String body;
    serializeJson(payload, body);

    if (DEBUG_MODE)
    {
        DEBUG_PRINTF("PV token refresh: POST %s body len %u", pvRefreshUrl, body.length());
        DEBUG_PRINTF("PV token refresh body: %s", body.c_str());
    }

    int httpCode = http.POST(body);
    String resp = http.getString();
    http.end();

    if (httpCode != HTTP_CODE_OK)
    {
        if (DEBUG_MODE)
            DEBUG_PRINTF("PV token refresh failed: HTTP %d, resp len %u", httpCode, resp.length());
        return false;
    }

    DynamicJsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, resp);
    if (err)
    {
        if (DEBUG_MODE)
            DEBUG_PRINTF("PV token refresh parse error: %s, raw resp (trunc): %s", err.c_str(), resp.substring(0, 200).c_str());
        return false;
    }

    bool updated = false;
    if (doc["data"]["access_token"].is<String>())
    {
        PV_ACCESS_TOKEN = doc["data"]["access_token"].as<String>();
        updated = true;
    }
    else if (doc["access_token"].is<String>())
    {
        PV_ACCESS_TOKEN = doc["access_token"].as<String>();
        updated = true;
    }

    if (doc["data"]["refresh_token"].is<String>())
    {
        PV_REFRESH_TOKEN = doc["data"]["refresh_token"].as<String>();
    }
    else if (doc["refresh_token"].is<String>())
    {
        PV_REFRESH_TOKEN = doc["refresh_token"].as<String>();
    }

    if (doc["data"]["access_key"].is<String>())
    {
        PV_ACCESS_KEY = doc["data"]["access_key"].as<String>();
    }
    else if (doc["access_key"].is<String>())
    {
        PV_ACCESS_KEY = doc["access_key"].as<String>();
    }

    if (DEBUG_MODE)
        DEBUG_PRINTF("PV token refresh %s; resp len %u", updated ? "succeeded" : "no new token", resp.length());

    return updated;
}

static void reloadPvDemoSettings()
{
    static bool prevMode = PV_DEMO_MODE;
    static float prevPower = PV_DEMO_POWER_W;
    static float prevSoc = PV_DEMO_SOC;
    static float prevEnergy = PV_DEMO_ENERGY_KWH;
    bool changed = false;

    File file = LittleFS.open("/DoNotTouch.json", "r");
    if (!file)
        return;

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error)
        return;

    if (doc.containsKey("PV Demo Mode"))
        PV_DEMO_MODE = doc["PV Demo Mode"].as<bool>();
    if (doc.containsKey("PV Demo Power (W)"))
        PV_DEMO_POWER_W = doc["PV Demo Power (W)"].as<float>();
    if (doc.containsKey("PV Demo SOC (%)"))
        PV_DEMO_SOC = doc["PV Demo SOC (%)"].as<float>();
    if (doc.containsKey("PV Demo Energy (kWh)"))
        PV_DEMO_ENERGY_KWH = doc["PV Demo Energy (kWh)"].as<float>();

    if (prevMode != PV_DEMO_MODE || prevPower != PV_DEMO_POWER_W || prevSoc != PV_DEMO_SOC || prevEnergy != PV_DEMO_ENERGY_KWH)
        changed = true;

    prevMode = PV_DEMO_MODE;
    prevPower = PV_DEMO_POWER_W;
    prevSoc = PV_DEMO_SOC;
    prevEnergy = PV_DEMO_ENERGY_KWH;

    if (changed && DEBUG_MODE && PV_DEMO_MODE)
    {
        DEBUG_PRINTF("PV Demo reload: power %.2f W, SOC %.2f, energy %.3f kWh", PV_DEMO_POWER_W, PV_DEMO_SOC, PV_DEMO_ENERGY_KWH);
    }

    // apply immediately if demo mode is active
    if (PV_DEMO_MODE && changed)
        applyPvDemoValues();
    else if (!PV_DEMO_MODE && changed)
    {
        // force next live poll without waiting interval
        lastPvPoll = 0;
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("PV Demo disabled, forcing live poll"));
    }
}

static void applyPvDemoValues()
{
    PV_Power_total = (uint16_t)PV_DEMO_POWER_W;
    PV_Battery_SOC = PV_DEMO_SOC;
    PV_Energy_Daily = PV_DEMO_ENERGY_KWH;
    if (DEBUG_MODE)
        DEBUG_PRINTF("PV API: demo mode values power %u W, SOC %.2f, energy %.3f kWh", PV_Power_total, PV_Battery_SOC, PV_Energy_Daily);
}

unsigned long long PeripheryManager_::readUptime()
{
    static unsigned long lastTime = 0;
    static unsigned long long totalElapsed = 0;

    unsigned long currentTime = millis();
    if (currentTime < lastTime)
    {
        // millis() overflow
        totalElapsed += 4294967295UL - lastTime + currentTime + 1;
    }
    else
    {
        totalElapsed += currentTime - lastTime;
    }
    lastTime = currentTime;

    unsigned long long uptimeSeconds = totalElapsed / 1000;
    return uptimeSeconds;
}

void PeripheryManager_::r2d2(const char *msg)
{
#ifdef ULANZI
    for (int i = 0; msg[i] != '\0'; i++)
    {
        char c = msg[i];
        tone(BUZZER_PIN, (c - 'A' + 1) * 50);
        delay(baudRate + 10);
    }
    noTone(BUZZER_PIN);
#endif
}
