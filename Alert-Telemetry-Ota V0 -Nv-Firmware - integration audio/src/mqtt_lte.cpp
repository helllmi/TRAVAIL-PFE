#include <Arduino.h>
#include "mqtt_lte.h"
#include "serial_comm.h"
#include "config.h"

// ============================================================================
//  ÉTAT INTERNE
// ============================================================================
static bool connected = false;
static bool needsRestart = false;
static MqttLteCallback messageCallback = nullptr;

#define MQTT_CLIENT_INDEX 0

// ============================================================================
//  NETTOYAGE MQTT (à appeler avant un (re)démarrage)
// ============================================================================
// Ordre critique : DISCONNECT → REL → STOP. Sans ce nettoyage, un état
// résiduel "client déjà acquis" peut empêcher un nouveau ACCQ.
static void mqttLte_cleanup()
{
    Serial.println("[MQTT-LTE] Cleanup...");

    char cmd[32];

    // 1) DISCONNECT (peut échouer si pas connecté — on tolère)
    snprintf(cmd, sizeof(cmd), "AT+CMQTTDISC=%d,120", MQTT_CLIENT_INDEX);
    SentMessage(cmd, 5000);
    delay(1000);

    // 2) RELEASE (libère le client_index)
    snprintf(cmd, sizeof(cmd), "AT+CMQTTREL=%d", MQTT_CLIENT_INDEX);
    SentMessage(cmd, 5000);
    delay(1000);

    // 3) STOP (arrête le service MQTT — réponse asynchrone)
    SentMessageAsync("AT+CMQTTSTOP", "+CMQTTSTOP:", 8000);
    delay(1500);
}

// ============================================================================
//  DÉMARRAGE + CONNEXION
// ============================================================================
bool mqttLte_start(const char *broker, uint16_t port, const char *clientId)
{
    Serial.println("[MQTT-LTE] Starting MQTT service over LTE...");
    connected = false;
    needsRestart = false;

    // 1) Nettoyage préalable
    mqttLte_cleanup();

    // 2) Démarrer le service MQTT (réponse asynchrone)
    String startResp = SentMessageAsync("AT+CMQTTSTART", "+CMQTTSTART:", 15000);
    Serial.printf("[MQTT-LTE] CMQTTSTART resp: %s\n", startResp.c_str());

    if (startResp.indexOf("+CMQTTSTART: 0") == -1 &&
        startResp.indexOf("+CMQTTSTART: 3") == -1)
    {
        // 0 = succès, 3 = déjà démarré (tolérable)
        Serial.println("[MQTT-LTE] CMQTTSTART failed");
        return false;
    }
    Serial.println("[MQTT-LTE] CMQTTSTART OK");
    delay(500);

    // 3) Acquérir un client avec le ClientID
    char accqCmd[128];
    snprintf(accqCmd, sizeof(accqCmd),
             "AT+CMQTTACCQ=%d,\"%s\",0",
             MQTT_CLIENT_INDEX, clientId);

    if (!SentMessage(accqCmd, 8000))
    {
        Serial.println("[MQTT-LTE] CMQTTACCQ failed");
        return false;
    }
    Serial.println("[MQTT-LTE] CMQTTACCQ OK");
    delay(500);

    // 4) Se connecter au broker (réponse asynchrone)
    char connectCmd[192];
    snprintf(connectCmd, sizeof(connectCmd),
             "AT+CMQTTCONNECT=%d,\"tcp://%s:%u\",%d,1",
             MQTT_CLIENT_INDEX, broker, port, MQTT_KEEPALIVE_SEC);

    String connResp = SentMessageAsync(connectCmd, "+CMQTTCONNECT:", 25000);
    Serial.printf("[MQTT-LTE] CMQTTCONNECT resp: %s\n", connResp.c_str());

    if (connResp.indexOf("+CMQTTCONNECT: 0,0") != -1)
    {
        connected = true;
        Serial.println("[MQTT-LTE] Connected to broker");
        return true;
    }

    Serial.println("[MQTT-LTE] Connect failed");
    return false;
}

// ============================================================================
//  GETTERS / STOP
// ============================================================================
bool mqttLte_isConnected()
{
    return connected;
}

bool mqttLte_needsRestart()
{
    return needsRestart;
}

void mqttLte_stop()
{
    mqttLte_cleanup();
    connected = false;
    needsRestart = false;
}

// ============================================================================
//  ABONNEMENT (variante SIM7670G : CMQTTSUB direct avec prompt)
// ============================================================================
bool mqttLte_subscribe(const char *topic, uint8_t qos)
{
    if (!connected)
    {
        Serial.println("[MQTT-LTE] Cannot subscribe: not connected");
        return false;
    }

    size_t topicLen = strlen(topic);

    char subCmd[48];
    snprintf(subCmd, sizeof(subCmd),
             "AT+CMQTTSUB=%d,%u,%u",
             MQTT_CLIENT_INDEX, (unsigned)topicLen, qos);

    if (!SentPrompt(subCmd, topic, 10000))
    {
        Serial.println("[MQTT-LTE] CMQTTSUB failed");
        return false;
    }

    Serial.printf("[MQTT-LTE] Subscribed to %s\n", topic);
    return true;
}

// ============================================================================
//  PUBLICATION
// ============================================================================
bool mqttLte_publish(const char *topic, const char *payload,
                     uint8_t qos, bool retain)
{
    if (!connected)
    {
        Serial.println("[MQTT-LTE] Cannot publish: not connected");
        return false;
    }

    size_t topicLen = strlen(topic);
    size_t payloadLen = strlen(payload);

    // 1) Topic — prompt '>' + données
    char topicCmd[48];
    snprintf(topicCmd, sizeof(topicCmd),
             "AT+CMQTTTOPIC=%d,%u",
             MQTT_CLIENT_INDEX, (unsigned)topicLen);
    if (!SentPrompt(topicCmd, topic, 8000))
    {
        Serial.println("[MQTT-LTE] CMQTTTOPIC failed");
        return false;
    }

    // 2) Payload — prompt '>' + données
    char payloadCmd[48];
    snprintf(payloadCmd, sizeof(payloadCmd),
             "AT+CMQTTPAYLOAD=%d,%u",
             MQTT_CLIENT_INDEX, (unsigned)payloadLen);
    if (!SentPrompt(payloadCmd, payload, 8000))
    {
        Serial.println("[MQTT-LTE] CMQTTPAYLOAD failed");
        return false;
    }

    // 3) Publier (QoS, timeout 60s, retain) — réponse asynchrone
    char pubCmd[48];
    snprintf(pubCmd, sizeof(pubCmd),
             "AT+CMQTTPUB=%d,%u,60,%d",
             MQTT_CLIENT_INDEX, qos, retain ? 1 : 0);

    String r = SentMessageAsync(pubCmd, "+CMQTTPUB:", 15000);
    if (r.indexOf("+CMQTTPUB: 0,0") != -1)
    {
        Serial.printf("[MQTT-LTE] Published %u bytes on %s\n",
                      (unsigned)payloadLen, topic);
        return true;
    }

    Serial.printf("[MQTT-LTE] Publish failed: %s\n", r.c_str());
    return false;
}

// ============================================================================
//  CALLBACK
// ============================================================================
void mqttLte_onMessage(MqttLteCallback cb)
{
    messageCallback = cb;
}

// ============================================================================
//  LOOP — surveille les URC entrants
// ============================================================================
// Pour la réception des URC, on a besoin d'accéder directement à 'ss' car
// readStringUntil() est une méthode du HardwareSerial. C'est la SEULE
// exception au principe "tout passer par serial_comm".
//
// On déclare 'ss' comme externe (défini dans serial_comm.cpp).
extern HardwareSerial ss;

void mqttLte_loop()
{
    if (!ss.available())
        return;

    String line = ss.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
        return;

    // ── URC : connexion MQTT perdue ────────────────────────────────────────
    if (line.startsWith("+CMQTTCONNLOST"))
    {
        Serial.printf("[MQTT-LTE] Connection lost: %s\n", line.c_str());
        connected = false;
        return;
    }

    // ── URC : plus de réseau → redémarrage requis ──────────────────────────
    if (line.startsWith("+CMQTTNONET"))
    {
        Serial.println("[MQTT-LTE] No network! Restart required");
        connected = false;
        needsRestart = true;
        return;
    }

    // ── URC : début d'un message reçu ──────────────────────────────────────
    if (line.startsWith("+CMQTTRXSTART"))
    {
        String topic = "";
        String payload = "";
        bool inTopic = false;
        bool inPayload = false;
        unsigned long start = millis();

        while (millis() - start < 6000)
        {
            if (!ss.available())
            {
                delay(2);
                continue;
            }
            String sub = ss.readStringUntil('\n');
            sub.trim();
            if (sub.length() == 0)
                continue;

            if (sub.startsWith("+CMQTTRXTOPIC"))
            {
                inTopic = true;
                inPayload = false;
                continue;
            }
            else if (sub.startsWith("+CMQTTRXPAYLOAD"))
            {
                inTopic = false;
                inPayload = true;
                continue;
            }
            else if (sub.startsWith("+CMQTTRXEND"))
            {
                break;
            }
            else
            {
                if (inTopic)
                    topic += sub;
                if (inPayload)
                    payload += sub;
            }
        }

        if (topic.length() > 0 && messageCallback != nullptr)
        {
            Serial.printf("[MQTT-LTE] RX on %s (%u bytes)\n",
                          topic.c_str(), payload.length());
            messageCallback(topic, payload);
        }
    }
}