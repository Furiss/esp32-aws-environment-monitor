#pragma once

#include <Arduino.h>
#include <DHT.h>

struct DHTread
{
    float temperature;
    float humidity;
    bool valid = false;
};

extern DHT dht;
extern DHTread dhtpayload;

struct LDRData
{
    int raw;
    float lux;
};

extern LDRData ldrpayload;

float adcToLux(int adcValue);
void readSensors();
