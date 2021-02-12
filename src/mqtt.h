#include <ArduinoJson.h>
#include <AsyncMqttClient.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>

static bool action_power = 0;
static bool action_power_force = 0;
static bool action_reset = 0;
static int wlan_disconnects = 0;
static AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;
StaticJsonDocument<1024> aliveJson;
StaticJsonDocument<1024> ackJson;
static char mqtt_state_topic[128] = "";
static char mqtt_ack_topic[128] = "";

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
  Serial.print("*MQTT: Session present: ");
  Serial.println(sessionPresent);
  if (strlen(mqtt_topic) == 0) {
    strcpy(mqtt_topic, "wakeonesp/wake");
  };
  strcpy(mqtt_state_topic, mqtt_topic);
  strcpy(mqtt_ack_topic, mqtt_topic);
  strcat(mqtt_state_topic, "/state");
  strcat(mqtt_ack_topic, "/ack");
  uint16_t packetIdSub = mqttClient.subscribe(mqtt_topic, 2);
  Serial.print("*MQTT: Subscribing at QoS 2, packetId: ");
  Serial.println(packetIdSub);

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
  Serial.print("*MQTT:   packetId: ");
  Serial.println(packetId);
  Serial.print("*MQTT:   qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("*MQTT: Unsubscribe acknowledged.");
  Serial.print("*MQTT:   packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  Serial.println("*MQTT: Publish received.");
  Serial.print("*MQTT:   topic: ");
  Serial.println(topic);
  Serial.print("*MQTT:   qos: ");
  Serial.println(properties.qos);
  Serial.print("*MQTT:   dup: ");
  Serial.println(properties.dup);
  Serial.print("*MQTT:   retain: ");
  Serial.println(properties.retain);
  Serial.print("*MQTT:   len: ");
  Serial.println(len);
  Serial.print("*MQTT:   index: ");
  Serial.println(index);
  Serial.print("*MQTT:   total: ");
  Serial.println(total);
  Serial.println();
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

void onMqttPublish(uint16_t packetId) {
  Serial.println("*MQTT: Publish acknowledged.");
  Serial.print("*MQTT:   packetId: ");
  Serial.println(packetId);
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

  Serial.print("*MQTT: Connecting to: ");
  Serial.println(mqtt_ip);

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
