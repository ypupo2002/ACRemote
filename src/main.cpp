#include <string.h>
#include <Arduino.h>
#include <Schedule.h>
#include <IRremoteESP8266.h>
#include <LittleFS.h>
#include <IRsend.h>
#include <IRtext.h>
#include <ir_Gree.h>
#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPmDNS.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESP8266mDNS.h>
#endif
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define DHT_PIN D1

const uint16_t kIrLed = D5;
const uint16_t httpPort = 80;

const char *ssid = "Nest";
const char *password = "doldshumed";

const char *PARAM_MESSAGE = "message";

IRGreeAC ac(kIrLed);
AsyncWebServer server(httpPort);
DHT dht(DHT_PIN, DHT11);

void printState()
{
    Serial.println("A/C remote is in the following state:");
    Serial.printf("  %s\n", ac.toString().c_str());
}

void setupWifi()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        Serial.printf("WiFi Failed!\n");
        return;
    }

    MDNS.begin("remote");

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}

void notFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
}

String getModeStr()
{
    String strMode;
    switch (ac.getMode())
    {
    case kGreeAuto:
        strMode = "auto";
        break;
    case kGreeCool:
        strMode = "cool";
        break;
    case kGreeDry:
        strMode = "dry";
    case kGreeFan:
        strMode = "fan";
    default:
        strMode = "unkown";
        break;
    }
    return strMode;
}

String getFanStr()
{
    String fan;
    switch (ac.getFan())
    {
    case kGreeFanAuto:
        fan = "auto";
        break;
    case kGreeFanMin:
        fan = "min";
        break;
    case kGreeFanMed:
        fan = "med";
        break;
    case kGreeFanMax:
        fan = "max";
        break;
    default:
        fan = "unknown";
        break;
    }
    return fan;
}

String getStateStr()
{
    String result;
    StaticJsonDocument<200> doc;

    doc["power"] = ac.getPower() ? kOnStr : kOffStr;
    doc["mode"] = getModeStr();
    doc["temp"] = ac.getTemp();
    doc["fan"] = getFanStr();
    doc["light"] = ac.getLight() ? kOnStr : kOffStr;
    doc["swing"] = ac.getSwingVerticalAuto() ? kOnStr : kOffStr;

    serializeJson(doc, result);

    return result;
}

void getState(AsyncWebServerRequest *request)
{
    request->send(200, "application/json", getStateStr());
}

void send()
{
    printState();
    ac.send();
}

void setState(AsyncWebServerRequest *request)
{
    if (request->hasParam("body", true))
    {
        String body = request->getParam("body", true)->value();
    }
    else
    {
        request->send(400);
    }

    schedule_function(send);
    request->send(200, "application/json", "{}");
}

void setTemp(AsyncWebServerRequest *request)
{
    if (request->hasParam("temp", true))
    {
        String value = request->getParam("temp", true)->value();
        ac.setTemp(value.toInt());
        schedule_function(send);
        request->send(204);
    }
    else
    {
        request->send(400);
    }
}

void setFan(AsyncWebServerRequest *request)
{
    if (request->hasParam("fan", true))
    {
        auto value = request->getParam("fan", true)->value();
        auto fan = kGreeFanAuto;
        if (value.equals("min"))
        {
            fan = kGreeFanMin;
        }
        else if (value.equals("med"))
        {
            fan = kGreeFanMed;
        }
        else if (value.equals("max"))
        {
            fan = kGreeFanMax;
        }
        ac.setFan(fan);
        schedule_function(send);
        request->send(204);
    }
    else
    {
        request->send(400);
    }
}

void setLight(AsyncWebServerRequest *request)
{
    if (request->hasParam("light", true))
    {
        auto value = request->getParam("light", true)->value();
        auto lightOn = value.equals("on");
        ac.setLight(lightOn);
        schedule_function(send);
        request->send(204);
    }
    else
    {
        request->send(400);
    }
}

void setPower(AsyncWebServerRequest *request)
{
    if (request->hasParam("power", true))
    {
        auto value = request->getParam("power", true)->value();
        auto powerOn = value.equals("on");
        ac.setPower(powerOn);
        schedule_function(send);
        request->send(204);
    }
    else
    {
        request->send(400);
    }
}

void setSwing(AsyncWebServerRequest *request)
{
    if (request->hasParam("swing", true))
    {
        auto value = request->getParam("swing", true)->value();
        auto on = value.equals("on");
        ac.setSwingVertical(on, kGreeSwingLastPos);
        schedule_function(send);
        request->send(204);
    }
    else
    {
        request->send(400);
    }
}

void setVerticalPos(AsyncWebServerRequest *request)
{
    if (request->hasParam("pos", true))
    {
        auto value = request->getParam("pos", true)->value();
        auto pos = kGreeSwingLastPos;
        if (value.equals("up"))
        {
            pos = kGreeSwingUp;
        }
        else if (value.equals("middleup"))
        {
            pos = kGreeSwingMiddleUp;
        }
        else if (value.equals("middle"))
        {
            pos = kGreeSwingMiddle;
        }
        else if (value.equals("middledown"))
        {
            pos = kGreeSwingMiddleDown;
        }
        else if (value.equals("down"))
        {
            pos = kGreeSwingDown;
        }
        ac.setSwingVertical(false, pos);
        schedule_function(send);
        request->send(204);
    }
    else
    {
        request->send(400);
    }
}

struct presetData
{
    bool power;
    uint8_t temp;
    uint8_t fan;
    bool light;
    struct
    {
        bool swing;
        uint8_t pos;
    } swing;
} PRESET_DATA;

void savePresetToFS(String name)
{
    presetData data;
    data.power = ac.getPower();
    data.temp = ac.getTemp();
    data.fan = ac.getFan();
    data.light = ac.getLight();
    data.swing.swing = ac.getSwingVerticalAuto();
    data.swing.pos = ac.getSwingVerticalPosition();

    auto presetFile = LittleFS.open("/presets/" + name, "w");
    presetFile.write((uint8_t*)&data, sizeof(data));
    presetFile.close();
}

void loadPresetFromFS(String name) {
    presetData data;
    if (LittleFS.exists("/presets/"+name)) {
        auto presetFile = LittleFS.open("/presets/"+name, "r");
        auto readed = presetFile.read((uint8_t*)&data, sizeof(data));
        if (readed == sizeof(data)) {
            ac.setPower(data.power);
            ac.setTemp(data.temp);
            ac.setFan(data.fan);
            ac.setLight(data.light);
            ac.setSwingVertical(data.swing.swing, data.swing.pos);
        }
    }
}

void savePreset(AsyncWebServerRequest *request)
{
    if (request->hasParam("name", true))
    {
        auto value = request->getParam("name", true)->value();
        savePresetToFS(value);
        request->send(204);
    }
    else
    {
        request->send(400);
    }
}

void loadPreset(AsyncWebServerRequest *request)
{
    if (request->hasParam("name", true))
    {
        auto value = request->getParam("name", true)->value();
        loadPresetFromFS(value);
        schedule_function(send);
        request->send(204);
    }
    else
    {
        request->send(400);
    }
}

void getDTHSensorValues(AsyncWebServerRequest *request) 
{
    auto temp = dht.readTemperature();
    auto hum = dht.readHumidity();
    Serial.println("readed");
    StaticJsonDocument<200> doc;

    doc["temperature"] = temp;
    doc["humidity"] = hum;

    String result;
    serializeJson(doc, result);

    request->send(200, "application/json", result);
}

void setupAPI()
{
    server.on("/ac/power", HTTP_POST, setPower);
    server.on("/ac/temp", HTTP_POST, setTemp);
    server.on("/ac/fan", HTTP_POST, setFan);
    server.on("/ac/light", HTTP_POST, setLight);
    server.on("/ac/swing", HTTP_POST, setSwing);
    server.on("/ac/swingPos", HTTP_POST, setVerticalPos);
    server.on("/ac/savePreset", HTTP_POST, savePreset);
    server.on("/ac/loadPreset", HTTP_POST, loadPreset);
    server.on("/ac", HTTP_GET, getState);
    server.on("/sensor", HTTP_GET, getDTHSensorValues);

    server.onNotFound(notFound);
}

void setupACRemote() {
    ac.begin();
    delay(200);

    printState();
    ac.setModel(YBOFB);
    ac.on();
    ac.setFan(kGreeFanAuto);
    ac.setMode(kGreeCool);
    ac.setTemp(25);
    ac.setXFan(false);
    ac.setSleep(false);
    ac.setTurbo(false);
    ac.setEcono(false);
    // printState();
}

void setup()
{
    Serial.begin(115200);

    LittleFS.begin();

    setupWifi();

    setupAPI();

    setupACRemote();

    dht.begin();
}

void loop()
{
    server.begin();
    //  ac.setLight(true);
    //  ac.setDisplayTempSource(kGreeDisplayTempSet);
    //  ac.send();
    //  delay(10000);
    // ac.setDisplayTempSource(kGreeDisplayTempInside);
    // ac.setUseFahrenheit(!ac.getUseFahrenheit());
    // ac.send();
    // delay(10000);
    // ac.setDisplayTempSource(kGreeDisplayTempOutside);
    // ac.send();
    // delay(10000);
    // ac.setDisplayTempSource(kGreeDisplayTempOff);
    // ac.send();
    // delay(10000);
}