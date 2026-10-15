#include "sensor.h"
#include "config/config.h"

DHT dht(DHTT_PIN, DHTT_TYPE);
DHTread dhtpayload;
LDRData ldrpayload;

#define GAMMA 0.7f
#define RL10 50.0f
#define RDIV 10000.0f
#define VCC 3.3f

float adcToLux(int adcValue)
{
    float voltage = adcValue / 4096.0f * VCC;
    float resistance = RDIV * voltage / (VCC - voltage);
    float lux = pow(RL10 * 1e3 * pow(10, GAMMA) / resistance, (1.0f / GAMMA));
    return lux;
}

void readSensors()
{
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    ldrpayload.raw = analogRead(LDR_PIN);
    ldrpayload.lux = adcToLux(ldrpayload.raw);

    if (isnan(temperature) || isnan(humidity))
    {
        dhtpayload.valid = false;
        Serial.println("DHT read error");
    }
    else
    {
        dhtpayload.temperature = temperature;
        dhtpayload.humidity = humidity;
        dhtpayload.valid = true;
    }
}
