#include "net.h"
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "config/config.h"
#include "secrets.h"

bool connectWiFi()
{
    Serial.print("[Wi-Fi] Connecting.");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);

    unsigned long start = millis();

    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - start > WIFI_TIMEOUT)
        {
            Serial.println(" [Wi-Fi] Connection failed: timeout!");
            return false;
        }

        delay(500);
        Serial.print(".");
    }

    Serial.println("SUCCESS");
    Serial.print("[Wi-Fi] IP: ");
    Serial.println(WiFi.localIP());

    return true;
}

bool syncTime()
{
    Serial.print("Synchronize time...");

    configTime(0, 0, "pool.ntp.org");

    struct tm timeinfo;
    unsigned long start = millis();

    while (!getLocalTime(&timeinfo))
    {
        if (millis() - start > TIME_SYNC_TIMEOUT)
        {
            Serial.println("TIMEOUT");
            return false;
        }

        delay(500);
        Serial.print(".");
    }

    Serial.println("SUCCESS");
    return true;
}
