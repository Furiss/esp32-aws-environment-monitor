#ifndef OTA_H
#define OTA_H

#include <Arduino.h>

extern volatile bool otaInProgress;

void ota_handle_job(const char* jobDocument);

#endif