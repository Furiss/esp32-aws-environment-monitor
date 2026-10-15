#pragma once

// Pins
#define LED_PIN 2
#define BUTTON_PIN 25
#define LDR_PIN 34
#define DHTT_PIN 4
#define DHTT_TYPE DHT22

// Intervals
#define RECONNECT_INTERVAL 1000
#define MAX_RECONNECT_INTERVAL 30000
#define DEBOUNCE 50
#define WAIT_JOB_MS 2000

// WiFi
#define WIFI_TIMEOUT 10000

// Time
#define TIME_SYNC_TIMEOUT 15000

// MQTT
#define MQTT_PORT 8883
#define TOPIC_SENSORS "iot-course/vlasenko/telemetry"
#define TOPIC_COMMANDS "iot-course/vlasenko/commands/led"
#define TOPIC_EVENTS "iot-course/vlasenko/events"
