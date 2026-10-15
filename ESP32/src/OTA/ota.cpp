#include "ota.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_task_wdt.h>
#include <esp_idf_version.h>
#include <ArduinoJson.h>
#include "../mqtt/mqtt.h"
#include "../version.h"

#define MIN_FIRMWARE_SIZE 100000

volatile bool otaInProgress = false;

static int open_stream(HTTPClient& http, WiFiClientSecure& client, const char* url, size_t from)
{
    http.begin(client, url);

    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int expected = HTTP_CODE_OK;
    if (from > 0)
    {
        char range[32];
        snprintf(range, sizeof(range), "bytes=%u-", (unsigned)from);
        http.addHeader("Range", range);
        expected = HTTP_CODE_PARTIAL_CONTENT;
    }

    int code = http.GET();
    Serial.print("[OTA] HTTP GET -> ");
    Serial.println(code);

    if (code != expected)
    {
        http.end();
        return -1;
    }
    return http.getSize();
}

static size_t pump_stream(WiFiClientSecure& client, WiFiClient* stream, size_t written,
                          size_t total)
{
    const size_t CHUNK = 4096;
    static uint8_t buf[CHUNK];

    int emptyReads = 0;
    const int MAX_EMPTY_READS = 200;

    while (written < total)
    {
        delay(1);

        size_t avail = stream->available();

        if (avail == 0)
        {
            if (!client.connected() && stream->available() == 0)
            {
                Serial.println("[OTA] Connection closed before download completed");
                break;
            }
            if (++emptyReads > MAX_EMPTY_READS)
            {
                Serial.println("[OTA] Download stalled: no data received");
                break;
            }
            delay(50);
            continue;
        }

        emptyReads = 0;

        size_t toRead = avail < CHUNK ? avail : CHUNK;
        int got = stream->readBytes(buf, toRead);
        if (got <= 0)
        {
            delay(50);
            continue;
        }

        size_t w = Update.write(buf, got);
        if (w != (size_t)got)
        {
            Serial.println("[OTA] Update.write failed to write all bytes");
            Update.printError(Serial);
            break;
        }

        written += w;
    }
    return written;
}

#define MAX_RESUMES 3

static bool download_and_flash(const char* url)
{
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    int len = open_stream(http, client, url, 0);
    if (len < 0)
        return false;

    Serial.print("[OTA] Content-Length: ");
    Serial.println(len);

    if (len < MIN_FIRMWARE_SIZE)
    {
        Serial.print("[OTA] Firmware file is too small: ");
        Serial.println(len);
        http.end();
        return false;
    }

    if (!Update.begin(len))
    {
        Update.printError(Serial);
        http.end();
        return false;
    }

    Serial.println("[OTA] Writing firmware...");
    size_t written = pump_stream(client, http.getStreamPtr(), 0, len);

    for (int attempt = 1; written < (size_t)len && attempt <= MAX_RESUMES; attempt++)
    {
        if (Update.hasError())
            break;
        http.end();

        Serial.print("[OTA] Download interrupted at ");
        Serial.print(written);
        Serial.print(" of ");
        Serial.print(len);
        Serial.print(" bytes - resuming, attempt ");
        Serial.println(attempt);

        int rest = open_stream(http, client, url, written);
        if (rest != (int)(len - written))
        {
            Serial.println("[OTA] Unexpected remaining size from server - aborting");
            break;
        }
        written = pump_stream(client, http.getStreamPtr(), written, len);
    }

    if (written != (size_t)len)
    {
        Serial.print("[OTA] Only wrote ");
        Serial.print(written);
        Serial.print(" of ");
        Serial.println(len);
        Update.abort();
        http.end();
        return false;
    }

    Serial.print("[OTA] Downloaded bytes: ");
    Serial.println(written);

    if (!Update.end(true))
    {
        Update.printError(Serial);
        http.end();
        return false;
    }

    http.end();
    return true;
}

void ota_handle_job(const char* jobDocument)
{
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, jobDocument);
    if (err)
    {
        Serial.print("[OTA] JSON: ");
        Serial.println(err.c_str());
        return;
    }

    if (!doc["execution"].is<JsonObject>())
    {
        Serial.println("[OTA] No pending jobs");
        return;
    }

    const char* jobId = doc["execution"]["jobId"];
    const char* version = doc["execution"]["jobDocument"]["version"];
    const char* url = doc["execution"]["jobDocument"]["url"];

    if (!jobId || !version || !url)
    {
        Serial.println("[OTA] Job document missing jobId/version/url - skipping");
        return;
    }

    if (strcmp(version, FIRMWARE_VERSION) == 0)
    {
        Serial.println("[OTA] Already running this version - job completed");
        mqtt_report_status(jobId, "SUCCEEDED");
        return;
    }

    Serial.print("[OTA] New version ");
    Serial.print(version);
    Serial.print(" (current: ");
    Serial.print(FIRMWARE_VERSION);
    Serial.println(") - starting update");

    otaInProgress = true;

    mqtt_report_status(jobId, "IN_PROGRESS");

    mqtt_disconnect();

#if ESP_IDF_VERSION_MAJOR >= 5
    esp_task_wdt_config_t wdtOta = {60000, 1, true};
    esp_task_wdt_reconfigure(&wdtOta);
#else
    esp_task_wdt_init(60, true);
#endif

    bool ok = download_and_flash(url);

#if ESP_IDF_VERSION_MAJOR >= 5
    esp_task_wdt_config_t wdtNormal = {CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000, 1, true};
    esp_task_wdt_reconfigure(&wdtNormal);
#else
    esp_task_wdt_init(CONFIG_ESP_TASK_WDT_TIMEOUT_S, true);
#endif

    if (ok)
    {
        Serial.println("[OTA] Update complete. Restarting...");
        Serial.flush();
        delay(500);

        esp_sleep_enable_timer_wakeup(100000);
        esp_deep_sleep_start();
    }

    Serial.println("[OTA] Update failed. Continuing with the current firmware");
    otaInProgress = false;
    connectMQTT(false);
    mqtt_report_status(jobId, "FAILED");
}