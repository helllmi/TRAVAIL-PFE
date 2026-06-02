#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#include <PubSubClient.h>

// ============================================================================
//  CONFIGURATION
// ============================================================================
// Le broker, le port et le DEVICE_ID sont définis dans main.cpp comme const
// globaux et utilisés ici via setMqttConfig().

extern PubSubClient mqtt;

// Topics construits dynamiquement à partir de DEVICE_ID
extern String topicAlert;
extern String topicTelemetry;
extern String topicOtaNotify;
extern String topicOtaStatus;
extern String topicLwt;
extern String topicState;

// ============================================================================
//  API
// ============================================================================
// À appeler une fois au setup, AVANT connectMqtt()
void mqttBegin(const char* broker, uint16_t port, const char* deviceId);

// Connexion (LWT inclus). Retourne true si OK.
bool connectMqtt();

// Doit être appelé dans loop() pour pomper les messages entrants
void mqttLoopTick();

// État
bool isMqttConnected();

// ── PUBLICATIONS ───────────────────────────────────────────────────────────
// Publie l'alerte complète (JSON ~1500 octets) - QoS 1, non retenu
bool mqttPublishAlert(const String& jsonPayload);

// Publie la télémétrie périodique (JSON court) - QoS 0, non retenu
bool mqttPublishTelemetry(const String& jsonPayload);

// Publie un état OTA (downloading / installing / failed / ...) - QoS 1
bool mqttPublishOtaStatus(const char* jsonPayload);

// Publie l'état du dispositif fsm+metrique-retain=true - QoS 1, retenu+
bool mqttPublishState(const String& jsonPayload);

#endif
