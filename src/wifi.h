#include <WiFiManager.h>
// #include "config.h"

void wifiSetup(bool onDemand);

void wifiSetup(bool onDemand = false) {
  String mac = "<p>This devices MAC-Address is: ";
  mac += WiFi.macAddress();
  mac += "</p>";

  String fp_info = "<p>Run <pre>echo | openssl s_client -connect MYMQTTSERVER:8883 | openssl x509 -fingerprint -noout</pre> to get your TLS fingerprint.</p>";

  WiFiManagerParameter custom_text(mac.c_str());
  WiFiManagerParameter custom_text_ssl(fp_info.c_str());
  WiFiManagerParameter custom_mqtt_server("server", "MQTT Server IP", mqtt_server, 128);
  WiFiManagerParameter custom_mqtt_port("port", "MQTT Port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "MQTT Username", mqtt_user, 128);
  WiFiManagerParameter custom_mqtt_password("password", "MQTT Password", mqtt_password, 128);
  WiFiManagerParameter custom_mqtt_topic("topic", "MQTT Topic", mqtt_topic, 128);
  WiFiManagerParameter custom_mqtt_fingerprint("fingerprint", "MQTT Fingerprint", mqtt_fingerprint, 128);

  WiFiManager wifiManager;
  bool config_portal = onDemand || !wifiManager.getWiFiIsSaved();

  wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  wifiManager.setTimeout(CONFIG_PORTAL_TIMEOUT);
  wifiManager.setEnableConfigPortal(config_portal);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&custom_text);
  // wifiManager.addParameter(&custom_text_ssl);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_password);
  wifiManager.addParameter(&custom_mqtt_topic);
  // wifiManager.addParameter(&custom_mqtt_fingerprint);

configPortal:
  if (config_portal) {
    // Re-Enable the AP mode if it was disabled by the autoconnect code below.
    WiFi.mode(WIFI_AP_STA);
    wifiManager.startConfigPortal("Wake-on-ESP");
  } else {
    bool connected = false;

    // This loop allows the user more time to short D4 to request the config portal before reboot.
    // We do not use setConnectRetries, since the config portal wouldn't open until all retries
    // are exausted, which can be quite a while. We want to react to D4 as soon as possible though.
    for (int i = 0; !connected && i < 10; ++i) {
      connected = wifiManager.autoConnect();

      // If connection failed, and the user requested opening the config portal by shorting D4,
      //  go to the config portal.
      if (!connected && configPortalRequested) {
        config_portal = true;
        configPortalRequested = false;
        goto configPortal;
      }
    }

    if (!connected) {
      // Assume the wifi may be temporarily unavailable,
      // wait a while then reboot to retry connection.
      Serial.println("Could not connect to WLAN, restarting...");
      ESP.restart();
    }

    // We should get here only if we're connected.
    // Otherwise an **OPEN** AP is present where everyone can connect...
    // Suggested by https://github.com/tzapu/WiFiManager/issues/833
    WiFi.mode(WIFI_STA);
    // Suggested by https://forum.arduino.cc/index.php?topic=557669.0
    // WiFi.softAPdisconnect(true);
  }

  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());
  strcpy(mqtt_topic, custom_mqtt_topic.getValue());
  strcpy(mqtt_fingerprint, custom_mqtt_fingerprint.getValue());
  configPortalRequested = false;
}
