#include <Arduino.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include "OTA/ota.h"
#include "version.h"
#include "device_shadow/device_shadow.h"
#include "config/config.h"
#include "sensor/sensor.h"
#include "net/net.h"
#include "mqtt/mqtt.h"
#include "button/button.h"
#include "led/led.h"
#include <esp_sleep.h>
#include "deep_sleep/deep_sleep.h"
#include <driver/rtc_io.h>

unsigned long lastReconnect = 0;
unsigned long reconnectInterval = RECONNECT_INTERVAL;
int reconnectAttempts = 1;
bool retryMessagePrinted = false;

extern "C" bool verifyRollbackLater()
{
    return true;
}

bool imagePending = false;
const unsigned long CONFIRM_TIMEOUT_MS = 120000;

RTC_DATA_ATTR unsigned int bootCount = 0;
RTC_DATA_ATTR int sleepSeconds = 60;

void handle_ota_confirmation()
{
    unsigned long now = millis();
    if (imagePending)
    {
        if (mqttClient.connected())
        {
            if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK)
            {
                imagePending = false;
                Serial.println("[OTA] Firmware confirmed");
            }
        }
        else if (now > CONFIRM_TIMEOUT_MS)
        {
            Serial.println("[OTA] MQTT unavailable: rolling back");
            esp_ota_mark_app_invalid_rollback_and_reboot();
        }
    }
}

void work_cycle()
{
    mqtt_wait_for_job(WAIT_JOB_MS);
    const char* job = mqtt_take_job();
    if (job)
    {
        ota_handle_job(job);
    }
    if (!mqttClient.connected())
        return;

    readSensors();
    if (dhtpayload.valid)
    {
        publishData(dhtpayload.temperature, dhtpayload.humidity, ldrpayload.lux);
    }
    else
    {
        Serial.println("Publish skipped: invalid sensor data");
    }

    shadow_request();
    shadow_wait_for_delta(10000);

    ShadowDelta delta;
    if (shadow_take_delta(&delta))
    {
        if (delta.hasLed)
        {
            bool ledChanged = strcmp(led, delta.led) != 0;
            if (led_set(delta.led) && ledChanged)
            {
                publishEvent("led_changed", led);
            }
        }
        if (delta.hasInterval)
        {
            sleepSeconds = delta.interval;
        }
    }

    handleCommand();

    // Звітуємо також без desired/delta, зокрема після OTA.
    shadow_report(led, sleepSeconds);

    if (!imagePending && mqttClient.connected())
    {
        if (!otaInProgress)
        {
            // Змустити девайс прокидатись при натисканні кнопки в мене не вийшло (можливо обмеження симулятоу wokwi)
            // Тому я зробив таку логіку: якщо кнопка утримується під час пробудження відправляється manual_trigger та очикується відпускання кнопки
            if (digitalRead(BUTTON_PIN) == LOW)
            {
                delay(DEBOUNCE);

                if (digitalRead(BUTTON_PIN) == LOW)
                {
                    readSensors();

                    if (dhtpayload.valid)
                    {
                        publishData(dhtpayload.temperature, dhtpayload.humidity, ldrpayload.lux);
                    }

                    publishEvent("manual_trigger", "pressed");
                    Serial.println("[SLEEP] Waiting for button release");

                    do
                    {
                        while (digitalRead(BUTTON_PIN) == LOW)
                        {
                            mqttClient.loop();
                            delay(10);
                        }

                        delay(DEBOUNCE);
                    } while (digitalRead(BUTTON_PIN) == LOW);
                }
            }
        }
        go_to_sleep(sleepSeconds);
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    bootCount++;
    Serial.print("[BOOT] Count: ");
    Serial.println(bootCount);

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER)
    {
        Serial.println("[BOOT] Woke up by timer");
    }
    else if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0)
    {
        Serial.println("[BOOT] Woke up by button");
    }
    else
    {
        Serial.println("[BOOT] Startup or another wakeup cause");
    }

    Serial.println("starting session");
    Serial.print("Firmware: ");
    Serial.println(FIRMWARE_VERSION);
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY)
    {
        imagePending = true;
        Serial.println("[OTA] New firmware: waiting for MQTT confirmation");
    }

    dht.begin();

    pinMode(LED_PIN, OUTPUT);
    led_set(led);
    pinMode(DHTT_PIN, INPUT);
    pinMode(LDR_PIN, INPUT);
    rtc_gpio_deinit((gpio_num_t)BUTTON_PIN);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    if (connectWiFi())
    {
        syncTime();
    }
    setupMQTT();
    mqttClient.setKeepAlive(60);
    mqttClient.setSocketTimeout(30);
    if (WiFi.status() == WL_CONNECTED)
    {
        connectMQTT();
    }

    lastReconnect = millis();

    if (mqttClient.connected())
    {
        handle_ota_confirmation();
        work_cycle();
    }
}

void loop()
{
    unsigned long now = millis();
    handle_ota_confirmation();

    if (buttonHandler())
    {
        readSensors();
        if (dhtpayload.valid)
        {
            publishData(dhtpayload.temperature, dhtpayload.humidity, ldrpayload.lux);
        }
        publishEvent("manual_trigger", "pressed");
    }

    if (mqttClient.connected())
    {
        reconnectAttempts = 1;
        reconnectInterval = RECONNECT_INTERVAL;
        lastReconnect = now;
        retryMessagePrinted = false;

        work_cycle();
    }
    else
    {
        if (!retryMessagePrinted)
        {
            Serial.print(WiFi.status() != WL_CONNECTED ? "[Wi-Fi] Disconnected. "
                                                       : "[MQTT] Disconnected. ");
            Serial.print("Retry #");
            Serial.print(reconnectAttempts);
            Serial.print(" in ");
            Serial.print(reconnectInterval / 1000);
            Serial.println(" s.");
            retryMessagePrinted = true;
        }

        if (now - lastReconnect >= reconnectInterval)
        {
            bool wifiConnected = WiFi.status() == WL_CONNECTED;

            if (!wifiConnected)
            {
                wifiConnected = connectWiFi();

                if (wifiConnected)
                {
                    syncTime();
                }
            }

            if (wifiConnected && connectMQTT())
            {
                reconnectAttempts = 1;
                reconnectInterval = RECONNECT_INTERVAL;
            }
            else
            {
                reconnectAttempts++;
                reconnectInterval =
                    min(reconnectInterval * 2, (unsigned long)MAX_RECONNECT_INTERVAL);
            }
            lastReconnect = millis();
            retryMessagePrinted = false;
        }
    }
}
