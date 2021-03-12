#include <Arduino.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <FS.h>

#include "config.h"
#include "hextools.h"
#include "mqtt.h"
#include "pin.h"
#include "wifi.h"
#include "wol.h"

void setup() {
  Serial.begin(115200);
  pinSetup();

  Serial.println("===Wake-on-ESP===");

  loadConfig();

  wifiSetup();

  if (shouldSaveConfig) {
    saveConfig();
  }

  wolSetup();
  mqttSetup();
}

void loop() {
  if (action_power == 1) {
    pinToggle(POWER_PIN, 200);
    action_power = 0;
  }
  if (action_power_force == 1) {
    pinToggle(POWER_PIN, 5000);
    action_power_force = 0;
  }
  if (action_reset == 1) {
    pinToggle(RESET_PIN, 200);
    action_reset = 0;
  }
  if (action_config) {
    configPortalRequested = true;
    action_config = false;
  }

  // is configuration portal requested?
  if (configPortalRequested) {
    wifiSetup(true);
  }
  if (shouldSaveConfig) {
    saveConfig();
  }

  // Check if wifi was disconnected (we should never get here if wifi is in config mode)
  if (WiFi.status() != WL_CONNECTED) {
    // Just restart, this hopefully gets us reconnected
    delay(DELAY_RESTART);
    // if we still have not connected restart and try all over again
    Serial.println("Connection to WLAN lost, restarting...");
    ESP.restart();
  }
}
