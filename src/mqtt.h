#include "utility.h"
#include <ArduinoJson.h>
#include <AsyncMqttClient.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>

static bool action_power = 0;
static bool action_power_force = 0;
static bool action_reset = 0;
static bool action_config = false;
static AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;
StaticJsonDocument<1024> aliveJson;
StaticJsonDocument<1024> ackJson;
static char mqtt_state_topic[128] = "";
static char mqtt_ack_topic[128] = "";
static char mqtt_config_topic[128] = "";

Ticker mqttAliveTimer;

void mqttSetup();
void callbackMqttAliveTimer();
void mqttAckPublish();

void connectToMqtt() {
  Serial.println("*MQTT: Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("*MQTT: Connected to MQTT.");
  Utility::Serial::printLn("*MQTT: Session present: ", sessionPresent);
  if (strlen(mqtt_topic) == 0) {
    strcpy(mqtt_topic, "wakeonesp/wake");
  };
  strcpy(mqtt_state_topic, mqtt_topic);
  strcat(mqtt_state_topic, "/state");
  strcpy(mqtt_ack_topic, mqtt_topic);
  strcat(mqtt_ack_topic, "/ack");
  strcpy(mqtt_config_topic, mqtt_topic);
  strcat(mqtt_config_topic, "/config");
  uint16_t packetIdSub = mqttClient.subscribe(mqtt_topic, 2);
  Utility::Serial::printLn("*MQTT: Subscribing to ", mqtt_topic, " at QoS 2, packetId: ", packetIdSub);
  packetIdSub = mqttClient.subscribe(mqtt_config_topic, 2);
  Utility::Serial::printLn("*MQTT: Subscribing to ", mqtt_config_topic, " at QoS 2, packetId: ", packetIdSub);

  aliveJson["mqtt"]["connectMillis"] = millis();

  callbackMqttAliveTimer();

  mqttAliveTimer.attach(15.0f, callbackMqttAliveTimer);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("*MQTT: Disconnected from MQTT.");

  aliveJson["mqtt"]["disconnectMillis"] = millis();

  mqttAliveTimer.detach();
  // if (reason == AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT) {
  //   Serial.println("Bad server fingerprint.");
  // }

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(5, connectToMqtt);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("*MQTT: Subscribe acknowledged.");
  Utility::Serial::printLn("*MQTT:   packetId: ", packetId);
  Utility::Serial::printLn("*MQTT:   qos: ", qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("*MQTT: Unsubscribe acknowledged.");
  Utility::Serial::printLn("*MQTT:   packetId: ", packetId);
}

void onMqttActionMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  if (!strncmp(payload, "on", len)) {
    action_power = 1;
    ackJson["type"] = "power";
    ackJson["millis"] = millis();
    mqttAckPublish();
  } else if (!strncmp(payload, "force_off", len)) {
    action_power_force = 1;
    ackJson["type"] = "force_off";
    ackJson["millis"] = millis();
    mqttAckPublish();
  } else if (!strncmp(payload, "reset", len)) {
    action_reset = 1;
    ackJson["type"] = "reset";
    ackJson["millis"] = millis();
    mqttAckPublish();
  }
}

void onMqttConfigMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  if (payload && 0 == strncmp(payload, "vermackelt", len)) {
    action_config = true;
    ackJson["type"] = "config";
    ackJson["millis"] = millis();
    mqttAckPublish();
  }
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  std::array<char, 128> pl;

  // Not sure if this is really needed, but to not tamper with the parameter passed down to other functions make a zero-terminated copy for display.
  const size_t max = len < (pl.size() - 1) ? len : (pl.size() - 1);
  strncpy(&pl[0], payload, max);
  pl[max] = 0;

  Serial.println("*MQTT: Publish received.");
  Utility::Serial::printLn("*MQTT:   topic: ", topic);
  Utility::Serial::printLn("*MQTT:   qos: ", properties.qos);
  Utility::Serial::printLn("*MQTT:   dup: ", properties.dup);
  Utility::Serial::printLn("*MQTT:   retain: ", properties.retain);
  Utility::Serial::printLn("*MQTT:   len: ", len);
  Utility::Serial::printLn("*MQTT:   index: ", index);
  Utility::Serial::printLn("*MQTT:   total: ", total);
  if (nullptr == payload )
    Serial.println("*MQTT:   payload: (null)");
  else
    Utility::Serial::printLn("*MQTT:   payload: ", &pl[0]);
  Serial.println();

  if (0 == strcmp(mqtt_topic, topic)) {
      onMqttActionMessage(topic, payload, properties, len, index, total);
  } else if (0 == strcmp(mqtt_config_topic, topic)) {
      onMqttConfigMessage(topic, payload, properties, len, index, total);
  }
}

void onMqttPublish(uint16_t packetId) {
  Serial.println("*MQTT: Publish acknowledged.");
  Utility::Serial::printLn("*MQTT:   packetId: ", packetId);
}

static char mqtt_clientid[24] = "";

void mqttSetup() {
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);

  IPAddress mqtt_ip;
  mqtt_ip.fromString(mqtt_server);

  Utility::Serial::printLn("*MQTT: Connecting to: ", mqtt_ip);

  mqttClient.setServer(mqtt_ip, atoi(mqtt_port));
  mqttClient.setCredentials(mqtt_user, mqtt_password);
  String clientIdStr = "WoE-" + WiFi.macAddress();
  // Remove (possibly) illegal chars, see http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html chapter 3.1.3.1
  clientIdStr.replace(":", "");
  // Need a static char array, since connect will use the client id after mqttSetup exits
  clientIdStr.toCharArray(mqtt_clientid, 24);
  mqttClient.setClientId(mqtt_clientid);

  // if (strncmp(mqtt_port, "8883", 4)) {
  //   mqttClient.setSecure(true);
  //   byte target_fp[128];
  //   StringToBytes((String)mqtt_fingerprint, target_fp);
  //   mqttClient.addServerFingerprint(target_fp);
  // }
  aliveJson["clientId"] = mqtt_clientid;
  aliveJson["uptimeMillis"] = millis();
  ackJson["type"] = "setup";
  ackJson["millis"] = millis();
  aliveJson["lastAction"] = ackJson;
  aliveJson["mqtt"]["disconnectMillis"] = 0;
  aliveJson["mqtt"]["connectMillis"] = 0;

  mqttClient.connect();
}

void mqttAckPublish() {
  if (mqttClient.connected()) {
    String str;

    serializeJson(ackJson, str);
    mqttClient.publish(mqtt_ack_topic, 0, false, str.c_str());
  }
}
void callbackMqttAliveTimer() {
  if (mqttClient.connected()) {
    String str;
    aliveJson["uptimeMillis"] = millis();
    aliveJson["lastAction"] = ackJson;
    serializeJson(aliveJson, str);
    mqttClient.publish(mqtt_state_topic, 0, false, str.c_str());
  }
}
