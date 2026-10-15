#pragma once
#include <Arduino.h>

void onMessage(char* topic, byte* payload, unsigned int length);
void handleCommand();
bool led_set(const char* state);
extern char led[8];