#include <ArduinoJson.h>
#include "led.h"
#include "config/config.h"
#include "mqtt/mqtt.h"

RTC_DATA_ATTR char led[8] = "off";

char commandAction[16] = "";
char commandValue[16] = "";
bool commandReceived = false;

void onMessage(char* topic, byte* payload, unsigned int length)
{
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, payload, length);

    if (error)
    {
        Serial.print("JSON parsing error: ");
        Serial.println(error.c_str());
        return;
    }

    strlcpy(commandAction, doc["action"] | "", sizeof(commandAction));
    strlcpy(commandValue, doc["value"] | "", sizeof(commandValue));

    commandReceived = true;
}

bool led_set(const char* state)
{
    if (strcmp(state, "on") == 0)
    {
        digitalWrite(LED_PIN, HIGH);
        strcpy(led, "on");
        Serial.println("[LED] ON");
        return true;
    }
    else if (strcmp(state, "off") == 0)
    {
        digitalWrite(LED_PIN, LOW);
        strcpy(led, "off");
        Serial.println("[LED] OFF");
        return true;
    }
    else
    {
        Serial.println("[LED] Unknown state");
        return false;
    }
}

void handleCommand()
{
    if (commandReceived)
    {
        if (strcmp(commandAction, "set_led") == 0)
        {
            if (led_set(commandValue))
            {
                publishEvent("led_changed", commandValue);
            }
        }
        else
        {
            Serial.println("Incorrect command");
        }

        commandReceived = false;
    }
}
