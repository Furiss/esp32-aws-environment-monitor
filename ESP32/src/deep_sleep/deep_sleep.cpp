#include "deep_sleep.h"
#include <WiFi.h>
#include <esp_sleep.h>
#include "mqtt/mqtt.h"
#include "OTA/ota.h"
#include "wdt/wdt.h"
#include "config/config.h"
#include <driver/rtc_io.h>

void go_to_sleep(uint32_t seconds)
{
    if (otaInProgress)
    {
        Serial.println("[SLEEP] OTA in progress, skipping sleep");
        return;
    }

    mqtt_disconnect();
    WiFi.mode(WIFI_OFF);
    delay(100);

    esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
    rtc_gpio_pullup_en((gpio_num_t)BUTTON_PIN);
    rtc_gpio_pulldown_dis((gpio_num_t)BUTTON_PIN);

    // Спробува реалізувати пробудження від натискання кнопки, але не вийшло. Можливо обмеження симулятора
    // "Залишки" коду вирішив не прибирати, бо можливо на фізичному пристрої він би спрацював (не перевіряв)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0);

    Serial.print("[SLEEP] Sleeping for ");
    Serial.print(seconds);
    Serial.println(" s");
    Serial.flush();

    wdt_silence_for_sleep();
    esp_deep_sleep_start();
}
