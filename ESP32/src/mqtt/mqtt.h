#pragma once

#include <Arduino.h>
#include <PubSubClient.h>

#define TOPIC_JOBS_NOTIFY "$aws/things/" THINGNAME "/jobs/notify-next"
#define TOPIC_JOBS_GET "$aws/things/" THINGNAME "/jobs/$next/get"
#define TOPIC_JOBS_ACCEPT "$aws/things/" THINGNAME "/jobs/$next/get/accepted"

extern PubSubClient mqttClient;

void setupMQTT();
bool connectMQTT(bool requestJob = true);
void publishData(float temperature, float humidity, float light_level);
void publishEvent(const char* event, const char* value);

bool mqtt_wait_for_job(unsigned long timeoutMs);
const char* mqtt_take_job();
void mqtt_disconnect();
void mqtt_request_next_job();
void mqtt_report_status(const char* jobId, const char* status);
