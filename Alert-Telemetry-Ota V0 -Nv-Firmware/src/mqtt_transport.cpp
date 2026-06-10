#include <Arduino.h>
#include "mqtt_transport.h"
#include "config.h"

#include "mqtt_client.h"     // WiFi (existant) — donne accès aux topics extern
#include "mqtt_lte.h"        // LTE (nouveau)

// On RÉUTILISE les topics globaux déclarés extern dans mqtt_client.h :
//   topicAlert, topicTelemetry, topicState, topicLwt,
//   topicOtaStatus, topicOtaNotify
// Ils sont construits par mqttBegin() (appelé dans mqttTransport_begin).

static MqttRxCallback userCallback = nullptr;

// ============================================================================
//  CALLBACK INTERMÉDIAIRE pour LTE
// ============================================================================
#if MQTT_TRANSPORT == MQTT_TRANSPORT_LTE
static void onLteMessage(const String& topic, const String& payload) {
    if (userCallback) userCallback(topic, payload);
}
#endif

// ============================================================================
//  INIT
// ============================================================================
bool mqttTransport_begin(const char* broker, uint16_t port, const char* clientId) {
   
    mqttBegin(broker, port, clientId);

    Serial.printf("[MQTT-TRANSPORT] Active transport: %s\n",
                  mqttTransport_typeName());

#if MQTT_TRANSPORT == MQTT_TRANSPORT_LTE
    bool ok = mqttLte_start(broker, port, clientId);
    if (ok) {
        mqttLte_onMessage(onLteMessage);
        mqttTransport_subscribeAll();
    }
    return ok;
#else
    return connectMqtt();
#endif
}

// ============================================================================
//  ÉTAT
// ============================================================================
bool mqttTransport_isConnected() {
#if MQTT_TRANSPORT == MQTT_TRANSPORT_LTE
    return mqttLte_isConnected();
#else
    return isMqttConnected();
#endif
}

// ============================================================================
//  RECONNEXION
// ============================================================================
bool mqttTransport_reconnect() {
#if MQTT_TRANSPORT == MQTT_TRANSPORT_LTE
    if (mqttLte_needsRestart()) {
        mqttLte_stop();
    }
    bool ok = mqttLte_start(MQTT_BROKER_HOST, MQTT_BROKER_PORT, DEVICE_ID);
    if (ok) {
        mqttLte_onMessage(onLteMessage);
        mqttTransport_subscribeAll();
    }
    return ok;
#else
    return connectMqtt();
#endif
}

// ============================================================================
//  LOOP
// ============================================================================
void mqttTransport_loop() {
#if MQTT_TRANSPORT == MQTT_TRANSPORT_LTE
    mqttLte_loop();
#else
    mqttLoopTick();
#endif
}

// ============================================================================
//  PUBLICATIONS
// ============================================================================
bool mqttTransport_publishAlert(const String& payload) {
#if MQTT_TRANSPORT == MQTT_TRANSPORT_LTE
    return mqttLte_publish(topicAlert.c_str(), payload.c_str(), 1, false);
#else
    return mqttPublishAlert(payload);
#endif
}

bool mqttTransport_publishTelemetry(const String& payload) {
#if MQTT_TRANSPORT == MQTT_TRANSPORT_LTE
    return mqttLte_publish(topicTelemetry.c_str(), payload.c_str(), 0, false);
#else
    return mqttPublishTelemetry(payload);
#endif
}

bool mqttTransport_publishState(const String& payload) {
#if MQTT_TRANSPORT == MQTT_TRANSPORT_LTE
    return mqttLte_publish(topicState.c_str(), payload.c_str(), 0, true);  // retain
#else
    return mqttPublishState(payload);
#endif
}

bool mqttTransport_publishOtaStatus(const String& payload) {
#if MQTT_TRANSPORT == MQTT_TRANSPORT_LTE
    return mqttLte_publish(topicOtaStatus.c_str(), payload.c_str(), 1, false);
#else
    return mqttPublishOtaStatus(payload.c_str());
#endif
}

// ============================================================================
//  ABONNEMENTS
// ============================================================================
bool mqttTransport_subscribeAll() {
#if MQTT_TRANSPORT == MQTT_TRANSPORT_LTE
    bool ok = true;
    ok &= mqttLte_subscribe(topicOtaNotify.c_str(), 1);
    return ok;
#else
    // WiFi : déjà géré par mqttBegin()/connectMqtt()
    return true;
#endif
}

// ============================================================================
//  CALLBACK
// ============================================================================
void mqttTransport_onMessage(MqttRxCallback cb) {
    userCallback = cb;
    // LTE : branché à onLteMessage dans begin/reconnect
    // WiFi : PubSubClient utilise déjà mqtt.setCallback() dans mqtt_client.cpp
}

// ============================================================================
//  TYPE
// ============================================================================
const char* mqttTransport_typeName() {
#if MQTT_TRANSPORT == MQTT_TRANSPORT_LTE
    return "LTE";
#else
    return "WIFI";
#endif
}