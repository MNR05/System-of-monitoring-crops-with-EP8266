// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.

#include <ESP8266WiFi.h>
#include "src/iotc/common/string_buffer.h"
#include "src/iotc/iotc.h"
#include "DHT.h"
#include <Wire.h>
#include <cmath>
#include <Adafruit_ADS1X15.h>

#define DHTPIN 2
#define DHTTYPE DHT11   // DHT 11

#define MOSPIN A0
Adafruit_ADS1115 ads;
//#define MQ135PIN 14

#define WIFI_SSID "mknr"
#define WIFI_PASSWORD "56715681NR"

const char* SCOPE_ID = "0ne00A0F2E3";
const char* DEVICE_ID = "cropmonitoring";
const char* DEVICE_KEY = "oY4EbmDdylrUkKXk87utmNihLCRZBYqh86ufvcqjq3s=";

DHT dht(DHTPIN, DHTTYPE);

void on_event(IOTContext ctx, IOTCallbackInfo* callbackInfo);
#include "src/connection.h"

void on_event(IOTContext ctx, IOTCallbackInfo* callbackInfo) {
  // ConnectionStatus
  if (strcmp(callbackInfo->eventName, "ConnectionStatus") == 0) {
    LOG_VERBOSE("Is connected ? %s (%d)", callbackInfo->statusCode == IOTC_CONNECTION_OK ? "YES" : "NO", callbackInfo->statusCode);
    isConnected = callbackInfo->statusCode == IOTC_CONNECTION_OK;
    return;
  }

  // payload buffer doesn't have a null ending.
  // add null ending in another buffer before print
  AzureIOT::StringBuffer buffer;
  if (callbackInfo->payloadLength > 0) {
    buffer.initialize(callbackInfo->payload, callbackInfo->payloadLength);
  }

  LOG_VERBOSE("- [%s] event was received. Payload => %s\n", callbackInfo->eventName, buffer.getLength() ? *buffer : "EMPTY");

  if (strcmp(callbackInfo->eventName, "Command") == 0) {
    LOG_VERBOSE("- Command name was => %s\r\n", callbackInfo->tag);
  }
}

void setup() {
  Serial.begin(9600);
  ads.begin();
  ads.setGain(GAIN_ONE);
  connect_wifi(WIFI_SSID, WIFI_PASSWORD);
  connect_client(SCOPE_ID, DEVICE_ID, DEVICE_KEY);

  if (context != NULL) {
    lastTick = 0;  // set timer in the past to enable first telemetry a.s.a.p
  }
   dht.begin();
}

void loop() {

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float ahs38 = analogRead(MOSPIN);
  ahs38 = map(ahs38, 0, 556, 0, 100);
  ahs38 = map(ahs38, 0, 100, 100, 0);
  float smoke = ads.readADC_SingleEnded(0);
  smoke = map(smoke, 0, 2590, 0, 100);
  smoke = map(smoke, 0, 100, 100, 0);
  smoke = abs(smoke);
  float light = ads.readADC_SingleEnded(2);
  light = map(light, 0, 25900, 0, 100);
  light = map(light, 0, 100, 100, 0);

  if (isConnected) {

    unsigned long ms = millis();
    if (ms - lastTick > 5000) {  // send telemetry every 10 seconds
      char msg[64] = {0};
      int pos = 0, errorCode = 0;

      lastTick = ms;
      if (loopId++ % 2 == 0) {  // send telemetry
        pos = snprintf(msg, sizeof(msg) - 1, "{\"Temperature\": %f}", t);
        errorCode = iotc_send_telemetry(context, msg, pos);
        
        pos = snprintf(msg, sizeof(msg) - 1, "{\"Humidity\":%f}", h);
        errorCode = iotc_send_telemetry(context, msg, pos);
        
        pos = snprintf(msg, sizeof(msg) - 1, "{\"Moisture\":%f}", ahs38);
        errorCode = iotc_send_telemetry(context, msg, pos);
        
        pos = snprintf(msg, sizeof(msg) - 1, "{\"Smoke\":%f}", smoke);
        errorCode = iotc_send_telemetry(context, msg, pos);

        pos = snprintf(msg, sizeof(msg) - 1, "{\"Light\":%f}", light);
        errorCode = iotc_send_telemetry(context, msg, pos);
          
      } else {  // send property

      }
  
      msg[pos] = 0;

      if (errorCode != 0) {
        LOG_ERROR("Sending message has failed with error code %d", errorCode);
      }
    }

    iotc_do_work(context);  // do background work for iotc
  } else {
    iotc_free_context(context);
    context = NULL;
    connect_client(SCOPE_ID, DEVICE_ID, DEVICE_KEY);
  }

}
