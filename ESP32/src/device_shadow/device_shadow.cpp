#include "device_shadow.h"
#include <ArduinoJson.h>
#include "../mqtt/mqtt.h"
#include "../version.h"

static char shadowBuf[2048];
static volatile bool shadowReady = false;

static volatile bool fromGetAccepted = false;

static volatile bool updateAccepted = false;
static volatile bool updateRejected = false;
static char rejectBuf[160];

bool shadow_on_messege(char* topic, byte* payload, unsigned int length)
{
    if (strstr(topic, "/shadow/update/accepted") != NULL)
    {
        updateAccepted = true;
        return true;
    }

    if (strstr(topic, "/shadow/update/rejected") != NULL)
    {
        unsigned long n = length;
        if (n > sizeof(rejectBuf) - 1)
            n = sizeof(rejectBuf) - 1;
        memcpy(rejectBuf, payload, n);
        rejectBuf[n] = '\0';
        updateRejected = true;
        return true;
    }

    bool isDelta = (strstr(topic, "/shadow/update/delta") != NULL);
    bool isGet = (strstr(topic, "/shadow/get/accepted") != NULL);

    if (!isDelta && !isGet)
    {
        return false;
    }

    if (length >= sizeof(shadowBuf))
    {
        Serial.println("[SHADOW] Document does not fit in buffer");
        return true;
    }
    memcpy(shadowBuf, payload, length);
    shadowBuf[length] = '\0';
    fromGetAccepted = isGet;
    shadowReady = true;

    return true;
}

void shadow_request()
{
    bool sent = mqttClient.publish(TOPIC_SHADOW_GET, "{}");
    if (!sent)
    {
        Serial.println("[SHADOW] Request publish failed");
    }
}

bool shadow_wait_for_delta(unsigned long timeoutMs)
{
    unsigned long start = millis();

    while (millis() - start < timeoutMs)
    {
        mqttClient.loop();

        if (shadowReady)
        {
            Serial.print("[SHADOW] Response in ");
            Serial.print(millis() - start);
            Serial.println(" ms");
            return true;
        }
        delay(10);
    }
    Serial.println("[SHADOW] Timeout waiting for document");
    return false;
}

bool shadow_take_delta(ShadowDelta* out)
{
    if (!shadowReady)
        return false;
    shadowReady = false;

    out->hasLed = false;
    out->hasInterval = false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, shadowBuf);
    if (err)
    {
        Serial.print("[SHADOW] JSON: ");
        Serial.println(err.c_str());
        return false;
    }

    JsonObject delta;
    if (fromGetAccepted)
    {
        if (!doc["state"]["desired"].is<JsonObject>())
        {
            Serial.println("[SHADOW] No desired state");
            return false;
        }
        delta = doc["state"]["desired"];
    }
    else
    {
        if (!doc["state"].is<JsonObject>())
        {
            Serial.println("[SHADOW] Delta without state - skipping");
            return false;
        }
        delta = doc["state"];
    }
    if (delta["led"].is<const char*>())
    {
        const char* v = delta["led"];
        strncpy(out->led, v, sizeof(out->led) - 1);
        out->led[sizeof(out->led) - 1] = '\0';
        out->hasLed = true;

        Serial.print("[SHADOW] Led: ");
        Serial.println(out->led);
    }
    if (delta["sleep_interval_sec"].is<int>())
    {
        out->interval = delta["sleep_interval_sec"];
        out->hasInterval = true;
    }
    return out->hasLed || out->hasInterval;
}

bool shadow_report(const char* led, int interval)
{
    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"state\":{\"reported\":{\"led\":\"%s\",\"sleep_interval_sec\":%d,"
             "\"firmware_version\":\"%s\"}}}",
             led, interval, FIRMWARE_VERSION);
    updateAccepted = false;
    updateRejected = false;

    if (!mqttClient.publish(TOPIC_SHADOW_UPDATE, payload))
    {
        Serial.println("[SHADOW] publish failed - check connection");
        return false;
    }
    Serial.print("[SHADOW] Reported: ");
    Serial.println(payload);

    unsigned long start = millis();

    while (millis() - start < 2000)
    {
        mqttClient.loop();

        if (updateAccepted)
        {
            Serial.print("[SHADOW] Update accepted in ");
            Serial.print(millis() - start);
            Serial.println(" ms - reported updated");
            return true;
        }

        if (updateRejected)
        {
            Serial.print("[SHADOW] Update rejected: ");
            Serial.println(rejectBuf);
            return false;
        }

        delay(10);
    }
    Serial.println("[SHADOW] Timeout waiting for update confirmation");
    return false;
}
