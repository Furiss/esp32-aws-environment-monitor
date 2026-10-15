#include "mqtt.h"
#include "led/led.h"
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>
#include "config/config.h"
#include "secrets.h"
#include "version.h"
#include "device_shadow/device_shadow.h"

static WiFiClientSecure net;
PubSubClient mqttClient(net);

static char jobBuf[3072];
static bool jobReady = false;

static void mqttOnMessage(char* topic, byte* payload, unsigned int length)
{
    if (shadow_on_messege(topic, payload, length))
    {
        return;
    }

    if (strcmp(topic, TOPIC_COMMANDS) == 0)
    {
        onMessage(topic, payload, length);
        return;
    }
    if (strcmp(topic, TOPIC_JOBS_NOTIFY) == 0)
    {
        mqtt_request_next_job();
        return;
    }
    if (strcmp(topic, TOPIC_JOBS_ACCEPT) == 0)
    {
        if (length >= sizeof(jobBuf))
        {
            Serial.println("[OTA] Job document is too large");
            return;
        }
        memcpy(jobBuf, payload, length);
        jobBuf[length] = '\0';
        jobReady = true;
    }
}

void setupMQTT()
{
    net.setCACert(AWS_CERT_CA);
    net.setCertificate(AWS_CERT_CRT);
    net.setPrivateKey(AWS_CERT_PRIVATE);

    mqttClient.setServer(AWS_IOT_ENDPOINT, MQTT_PORT);
    mqttClient.setBufferSize(4096);
    mqttClient.setCallback(mqttOnMessage);
}

bool connectMQTT(bool requestJob)
{
    Serial.print("[MQTT] Connecting to AWS IoT Core...");

    if (mqttClient.connect(THINGNAME))
    {
        Serial.println("SUCCESS");
        mqttClient.subscribe(TOPIC_COMMANDS, 1);
        Serial.println("[MQTT] Subscribed to Commands");

        mqttClient.subscribe(TOPIC_JOBS_NOTIFY, 1);
        mqttClient.subscribe(TOPIC_JOBS_ACCEPT, 1);
        Serial.println("[MQTT] Subscribe to JOBS");

        mqttClient.subscribe(TOPIC_SHADOW_DELTA, 1);
        mqttClient.subscribe(TOPIC_SHADOW_GET_ACC, 1);
        mqttClient.subscribe(TOPIC_SHADOW_UPD_ACC, 1);
        mqttClient.subscribe(TOPIC_SHADOW_UPD_REJ, 1);
        mqttClient.subscribe(TOPIC_SHADOW_GET_REJ, 1);
        Serial.println("[MQTT] Subscribe to SHADOW");

        unsigned long start = millis();

        while (millis() - start < 1000)
        {
            mqttClient.loop();
            delay(10);
        }

        if (requestJob)
            mqtt_request_next_job();
        return true;
    }

    Serial.print("[MQTT] Connection failed. Error: ");
    Serial.println(mqttClient.state());

    net.stop();
    return false;
}

void publishData(float temperature, float humidity, float light_level)
{
    if (!mqttClient.connected())
    {
        Serial.println("No connection, skipping");
        return;
    }

    time_t now;
    time(&now);

    JsonDocument doc;

    doc["device_id"] = THINGNAME;
    doc["timestamp"] = (unsigned long)now;
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    doc["light_level"] = light_level;
    doc["firmware_version"] = FIRMWARE_VERSION;
    doc["uptime_sec"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();

    char payload[384];

    serializeJson(doc, payload, sizeof(payload));

    Serial.print("Publishing: ");
    Serial.println(payload);

    bool ok = mqttClient.publish(TOPIC_SENSORS, payload);

    Serial.println(ok ? "SUCCESS" : "Publishing ERROR");
}

void publishEvent(const char* event, const char* value)
{
    time_t now;
    time(&now);

    JsonDocument doc;

    doc["device_id"] = THINGNAME;
    doc["timestamp"] = (unsigned long)now;
    doc["event"] = event;
    doc["firmware_version"] = FIRMWARE_VERSION;
    doc["value"] = value;

    char payload[256];

    serializeJson(doc, payload, sizeof(payload));
    bool ok = mqttClient.publish(TOPIC_EVENTS, payload);
    Serial.println(ok ? "Event published on iot-course/vlasenko/events" : "publish failed");
}

const char* mqtt_take_job()
{
    if (!jobReady)
        return NULL;
    jobReady = false;
    return jobBuf;
}

void mqtt_disconnect()
{
    mqttClient.disconnect();
    net.stop();
    Serial.println("[MQTT] Connection closed");
}

void mqtt_request_next_job()
{
    mqttClient.publish(TOPIC_JOBS_GET, "{}");
}

bool mqtt_wait_for_job(unsigned long timeoutMs)
{
    unsigned long start = millis();

    while (millis() - start < timeoutMs)
    {
        mqttClient.loop();

        if (jobReady)
        {
            Serial.print("[JOBS] Response in ");
            Serial.print(millis() - start);
            Serial.println(" ms");
            return true;
        }
        delay(10);
    }
    Serial.println("[JOBS] Timeout waiting for response");
    return false;
}

void mqtt_report_status(const char* jobId, const char* status)
{
    char topic[160];
    snprintf(topic, sizeof(topic), "$aws/things/" THINGNAME "/jobs/%s/update", jobId);

    char payload[64];
    snprintf(payload, sizeof(payload), "{\"status\":\"%s\"}", status);

    mqttClient.publish(topic, payload);
    Serial.print("[JOBS] Status ");
    Serial.print(status);
    Serial.print(" for ");
    Serial.println(jobId);
}
