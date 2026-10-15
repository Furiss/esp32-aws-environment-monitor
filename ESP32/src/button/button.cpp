#include <Arduino.h>
#include "button.h"
#include "config/config.h"

static unsigned long lastDebounce = 0;
static bool lastButtonState = HIGH;
static bool buttonState = HIGH;

bool buttonHandler()
{
    bool pressed = false;
    bool reading = digitalRead(BUTTON_PIN);

    if (reading != lastButtonState)
    {
        lastDebounce = millis();
    }
    if ((millis() - lastDebounce) > DEBOUNCE)
    {
        if (reading != buttonState)
        {
            buttonState = reading;
            if (buttonState == LOW)
            {
                pressed = true;
            }
        }
    }

    lastButtonState = reading;
    return pressed;
}
