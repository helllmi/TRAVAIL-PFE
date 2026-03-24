#include <Arduino.h>
#include <WiFi.h>
#include "wifi_manager.h"

// Variables externes déclarées dans main.cpp
extern String ssid;
extern String pass;
extern String ip;
extern String gateway;
extern IPAddress localIP;
extern IPAddress localGateway;
extern IPAddress subnet;
extern unsigned long previousMillis;
;

// Initialize WiFi
bool initWiFi() {
  if(ssid=="" || ip==""){
    Serial.println("Undefined SSID or IP address.");
    return false;
  }

  WiFi.mode(WIFI_STA);
 if(ip != "") {
    localIP.fromString(ip.c_str());
    localGateway.fromString(gateway.c_str());
    if (!WiFi.config(localIP, localGateway, subnet)){
      Serial.println("STA Failed to configure");
      return false;
    }
    Serial.println("IP statique configurée");
  } else {
    Serial.println("DHCP automatique");  // ✅ IP auto
  }

  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Connecting to WiFi...");

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while(WiFi.status() != WL_CONNECTED) {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      Serial.println("Failed to connect.");
      return false;
    }
  }

  Serial.println(WiFi.localIP());
  return true;
}