#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "mqtt_client.h"
#include "ota.h"
#include "led.h"

// ============================================================================
//  ÉTAT GLOBAL
// ============================================================================
static WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// Topics (construits depuis DEVICE_ID au mqttBegin)
String topicAlert;
String topicTelemetry;
String topicOtaNotify;
String topicOtaStatus;
String topicLwt;
String topicState;

static String mqttBroker;
static uint16_t mqttPort;
static String deviceIdStr;

// ============================================================================
//  CALLBACK : réception d'une notification OTA
// ============================================================================
// Payload attendu :
//   {
//     "version": "1.1.0",
//     "url":     "http://192.168.x.x:5000/firmware/v1.1.0.bin",
//     "sha256":  "abcd...64 hex chars...",
//     "size":    945632
//   }
static void onMqttMessage(char *topic, byte *payload, unsigned int length)
{
    Serial.printf("\n[MQTT] Message received on '%s' (%u bytes)\n", topic, length);

    // On ne traite que les notifications OTA pour l'instant
    String topicStr = String(topic);
    if (topicStr != topicOtaNotify)
    {
        Serial.println("[MQTT] Ignored (not an OTA notify)");
        return;
    }

    if (otaInProgress)
    {
        Serial.println("[MQTT] OTA already in progress, ignoring");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err)
    {
        Serial.printf("[MQTT] JSON parse error: %s\n", err.c_str());
        return;
    }

    String version = doc["version"].as<String>();
    String url = doc["url"].as<String>();
    String sha256 = doc["sha256"].as<String>();
    size_t size = doc["size"] | 0;

    Serial.printf("[MQTT] OTA notify: v%s, size=%u\n", version.c_str(), size);

    if (version == FIRMWARE_VERSION)
    {
        Serial.println("[MQTT] Already on this version, ignoring");
        return;
    }

    if (url.length() == 0 || sha256.length() != 64)
    {
        Serial.println("[MQTT] Invalid payload");
        return;
    }

    otaInProgress = true;
    performOTA(url, sha256, size);
    otaInProgress = false;
}

// ============================================================================
//  INIT
// ============================================================================
void mqttBegin(const char *broker, uint16_t port, const char *deviceId)
{
    mqttBroker = String(broker);
    mqttPort = port;
    deviceIdStr = String(deviceId);

    topicAlert = "devices/" + deviceIdStr + "/alert";
    topicTelemetry = "devices/" + deviceIdStr + "/telemetry";
    topicOtaNotify = "devices/" + deviceIdStr + "/ota/notify";
    topicOtaStatus = "devices/" + deviceIdStr + "/ota/status";
    topicLwt = "devices/" + deviceIdStr + "/lwt";
    topicState = "devices/" + deviceIdStr + "/state";

    mqtt.setServer(mqttBroker.c_str(), mqttPort);
    mqtt.setCallback(onMqttMessage);
    mqtt.setBufferSize(2048); // assez pour le JSON d'alerte (~1500 octets)
    mqtt.setKeepAlive(30);    // 30s, compatible LTE PSM

    Serial.printf("[MQTT] Config: broker=%s:%u, device=%s\n",
                  mqttBroker.c_str(), mqttPort, deviceIdStr.c_str());
}

// ============================================================================
//  CONNEXION (avec LWT)
// ============================================================================
bool connectMqtt()
{
    if (mqtt.connected())
        return true;
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[MQTT] Wi-Fi down, skip MQTT connect");
        return false;
    }

    Serial.printf("[MQTT] Connecting to %s:%u...\n",
                  mqttBroker.c_str(), mqttPort);
    setLED(LED_MQTT_CONNECTING);

    bool connected = mqtt.connect(
        deviceIdStr.c_str(),
        nullptr, nullptr,          // pas d'auth pour les tests
        topicLwt.c_str(), 1, true, // LWT QoS 1 + retain
        "{\"state\":\"offline\"}");

    if (!connected)
    {
        Serial.printf("[MQTT] Failed, rc=%d\n", mqtt.state());
        return false;
    }

    Serial.println("[MQTT] Connected");
    setLED(LED_MQTT_OK);

    // Publier "online" (retain) et s'abonner aux notifs OTA
    mqtt.publish(topicLwt.c_str(), "{\"state\":\"online\"}", true);
    mqtt.subscribe(topicOtaNotify.c_str(), 1);
    Serial.printf("[MQTT] Subscribed to %s\n", topicOtaNotify.c_str());

    return true;
}

// ============================================================================
//  POMPE DES MESSAGES ENTRANTS
// ============================================================================
void mqttLoopTick()
{
    mqtt.loop();
}

bool isMqttConnected()
{
    return mqtt.connected();
}

// ============================================================================
//  PUBLICATIONS
// ============================================================================

bool mqttPublishAlert(const String &jsonPayload)
{
    if (!mqtt.connected())
    {
        Serial.println("[MQTT] Cannot publish alert: not connected");
        return false;
    }

    Serial.printf("[MQTT] PUB alert (%u bytes) on %s\n",
                  jsonPayload.length(), topicAlert.c_str());

    setLED(LED_WIFI_SENDING);
    bool ok = mqtt.publish(topicAlert.c_str(),
                           (const uint8_t *)jsonPayload.c_str(),
                           jsonPayload.length(),
                           false); // pas retain

    if (ok)
    {
        Serial.println("[MQTT] Alert published OK");
        setLED(LED_SUCCESS);
    }
    else
    {
        Serial.printf("[MQTT] PUB failed, state=%d\n", mqtt.state());
        setLED(LED_ERROR);
    }
    return ok;
}

bool mqttPublishTelemetry(const String &jsonPayload)
{
    if (!mqtt.connected())
        return false;

    Serial.printf("[MQTT] PUB telemetry (%u bytes)\n", jsonPayload.length());
    return mqtt.publish(topicTelemetry.c_str(),
                        (const uint8_t *)jsonPayload.c_str(),
                        jsonPayload.length(),
                        false);
}

bool mqttPublishOtaStatus(const char *jsonPayload)
{
    if (!mqtt.connected())
        return false;
    return mqtt.publish(topicOtaStatus.c_str(), jsonPayload, false);
}
bool mqttPublishState(const String &jsonPayload)
{
    if (!mqtt.connected())
    {
        Serial.println("[MQTT] Cannot publish state: not connected");
        return false;
    }

    Serial.printf("[MQTT] PUB state (%u bytes) on %s\n",
                  jsonPayload.length(), topicState.c_str());

    bool ok = mqtt.publish(topicState.c_str(),
                           (const uint8_t *)jsonPayload.c_str(),
                           jsonPayload.length(),
                           true); // ← retain=true (point clé !)

    if (ok)
    {
        Serial.println("[MQTT] State published OK (retained)");
    }
    else
    {
        Serial.printf("[MQTT] State PUB failed, state=%d\n", mqtt.state());
    }
    return ok;
}