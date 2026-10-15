#pragma once
#include <Arduino.h>
#include "../secrets.h"

#define TOPIC_SHADOW_GET "$aws/things/" THINGNAME "/shadow/get"
#define TOPIC_SHADOW_GET_ACC "$aws/things/" THINGNAME "/shadow/get/accepted"
#define TOPIC_SHADOW_UPDATE "$aws/things/" THINGNAME "/shadow/update"
#define TOPIC_SHADOW_DELTA "$aws/things/" THINGNAME "/shadow/update/delta"
#define TOPIC_SHADOW_UPD_ACC "$aws/things/" THINGNAME "/shadow/update/accepted"
#define TOPIC_SHADOW_UPD_REJ "$aws/things/" THINGNAME "/shadow/update/rejected"
#define TOPIC_SHADOW_GET_REJ "$aws/things/" THINGNAME "/shadow/get/rejected"

typedef struct
{
    bool hasLed;
    char led[9];
    bool hasInterval;
    int interval;
} ShadowDelta;

bool shadow_on_messege(char* topic, byte* payload, unsigned int lengh);

void shadow_request();

bool shadow_wait_for_delta(unsigned long timeoutMs);

bool shadow_take_delta(ShadowDelta* out);

bool shadow_report(const char* led, int interval);
