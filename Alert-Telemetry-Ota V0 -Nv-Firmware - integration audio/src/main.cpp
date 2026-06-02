/**
 * ===========================================================================
 *  Firmware ESP32-S3 — Alerte IoT  v1.0.0
 * ===========================================================================
 *  Fusion :
 *    - ESP-S3-ALERT  (GPS, batterie, NeoPixel, SIM7670G LTE, fallback Wi-Fi)
 *    - firmware-v1-mqtt (MQTT, OTA, watchdog, rollback)
 *
 *  Logique :
 *    - Toutes les 2s : lecture GPS via SIM7670G
 *    - Si fix valide : envoi d'une ALERTE par MQTT (toutes les 10s mini)
 *    - Toutes les 60s : envoi d'une TÉLÉMÉTRIE par MQTT
 *    - À tout moment : un OTA push peut arriver sur devices/{id}/ota/notify
 *      → téléchargement HTTP + SHA256 + Update + reboot + observation 1 min
 *
 *  Transport actuel : Wi-Fi (PubSubClient). Migration LTE Cat M1 ultérieure
 *  via AT+CMQTT* du SIM7670G — la couche métier reste identique.
 * ===========================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <SD_MMC.h>
#include <ArduinoJson.h>
#include "config.h"
#include "sos_button.h"
#include "state_machine.h"
#include "serial_comm.h"
#include "battery.h"
#include "gps.h"
#include "lte.h"
#include "fallbackwifi.h"
#include "led.h"
#include "mqtt_client.h"
#include "ota.h"
#include "watchdog.h"
#include "storage.h"
#include "power_manager.h"
#include "mqtt_transport.h"
#include "audio.h"

// ===========================================================================
//  ÉTAT GLOBAL
// ===========================================================================
unsigned long lastAlertMs = 0;
unsigned long lastTelemetryMs = 0;
unsigned long lastDeleteMs = 0;
unsigned long lastStateMs = 0;
// ===========================================================================
//  CONSTRUCTION DES PAYLOADS JSON
// ===========================================================================

// Reprend ton format complet (compatible backend / CRM existant)
static String buildAlertJson()
{
    float batteryLevel = readBattery();
    String networkTime = getNetworkTime();
    SignalInfo sig = getSignalInfo();

    JsonDocument doc;

    doc["timestamp"] = networkTime.length() > 0
                           ? networkTime
                           : String(millis() / 1000);
    doc["priority"] = "CRITICAL";

    // LOCATION
    JsonObject loc = doc["location"].to<JsonObject>();
    loc["latitude"] = currentGPS.latitude;
    loc["longitude"] = currentGPS.longitude;
    loc["accuracy_meters"] = 5.0;
    loc["altitude_meters"] = currentGPS.altitude;
    loc["speed_kmh"] = currentGPS.speed;
    loc["heading_degrees"] = nullptr;
    loc["location_source"] = "GPS";
    loc["address_reverse_geocoded"] = "Tunisia";

    // CONTEXT
    JsonObject ctx = doc["context"].to<JsonObject>();
    ctx["fall_detected"] = false;
    ctx["heart_rate_bpm"] = 100;
    ctx["ambient_noise_db"] = 70.0;
    ctx["motion_state"] = "STATIONARY";
    ctx["geofence_status"] = "UNKNOWN";

    // METADATA
    JsonObject meta = doc["metadata"].to<JsonObject>();
    meta["schema_version"] = "1.0";
    meta["kafka_topic"] = "helpmee_alerts";
    meta["partition_key"] = DEVICE_ID;
    meta["ttl_seconds"] = 86400;
    meta["retry_count"] = 0;

    // IDs
    doc["alert_id"] = "ALT-" + String(DEVICE_ID);
    doc["device_id"] = DEVICE_ID;
    doc["user_id"] = "USR-001";
    doc["alert_type"] = "PANIC";
    doc["trigger_method"] = "BUTTON_PRESS";
    doc["press_count"] = sosButton_getPressCount();
    doc["press_duration_ms"] = sosButton_getPressDurationMs();

    // DEVICE STATUS
    JsonObject dev = doc["device_status"].to<JsonObject>();
    dev["charging"] = false;
    dev["battery_level_pct"] = (int)batteryLevel;
    dev["signal_strength_dbm"] = sig.rssi;
    dev["connectivity_type"] = isWiFiConnected() ? "WIFI" : "LTE";
    dev["firmware_version"] = FIRMWARE_VERSION;
    dev["is_charging"] = false;
    dev["last_health_check"] = millis() / 1000;

    // USER PROFILE
    JsonObject usr = doc["user_profile"].to<JsonObject>();
    usr["age"] = 78;
    usr["category"] = "ELDERLY";
    usr["full_name"] = "Fatma Ben Ali";
    usr["blood_type"] = "A+";

    JsonArray conds = usr["medical_conditions"].to<JsonArray>();
    conds.add("hypertension");
    conds.add("diabetes_type_2");

    JsonArray contacts = usr["emergency_contacts"].to<JsonArray>();
    JsonObject c = contacts.add<JsonObject>();
    c["name"] = "Ahmed Ben Ali";
    c["relationship"] = "SON";
    c["phone"] = "+216-55-123-456";
    c["notify"] = true;

    JsonArray langs = usr["spoken_languages"].to<JsonArray>();
    langs.add("ar");
    langs.add("fr");

    String out;
    serializeJson(doc, out);
    return out;
}

// Télémétrie courte, envoyée régulièrement
static String buildTelemetryJson()
{
    SignalInfo sig = getSignalInfo();

    JsonDocument doc;
    doc["device_id"] = DEVICE_ID;
    doc["timestamp"] = getNetworkTime();
    doc["battery_pct"] = (int)readBattery();
    doc["signal_rssi"] = sig.rssi;
    doc["signal_ber"] = sig.ber;
    doc["connectivity"] = isWiFiConnected() ? "WIFI" : "LTE";
    doc["firmware_version"] = FIRMWARE_VERSION;
    doc["uptime_seconds"] = millis() / 1000;
    doc["gps_valid"] = currentGPS.valid;

    String out;
    serializeJson(doc, out);
    return out;
}
static String buildStateJson()
{
    JsonDocument doc;

    doc["device_id"] = DEVICE_ID;
    doc["state"] = stateMachine_stateName(stateMachine_getState());
    doc["timestamp"] = getNetworkTime();
    doc["battery_pct"] = (int)readBattery();
    doc["uptime_seconds"] = millis() / 1000;
    doc["state_uptime_sec"] = stateMachine_getStateUptime() / 1000;
    doc["firmware_version"] = FIRMWARE_VERSION;
    doc["connectivity"] = mqttTransport_typeName();
    doc["gps_valid"] = currentGPS.valid;

    String out;
    serializeJson(doc, out);
    return out;
}
static void onStateChanged(DeviceState newState)
{
    if (mqttTransport_isConnected())
    {
        String stateJson = buildStateJson();
        mqttTransport_publishState(stateJson);
    }
}
// callback pour republier une alerte stockée après reconnexion MQTT
static bool republishAlert(const String &payload)
{
    if (!mqttTransport_isConnected())
    {
        return false; // pas connecté → échec, le message reste en queue
    }
    return mqttTransport_publishAlert(payload);
}

// ===========================================================================
//  SETUP
// ===========================================================================
void setup()
{
    // Alimentation périphériques externes
    pinMode(PIN_POWER_PERIPH, OUTPUT);
    digitalWrite(PIN_POWER_PERIPH, HIGH);

    Serial.begin(115200);
    ss.begin(115200, SERIAL_8N1, PIN_SIM_RX, PIN_SIM_TX);
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    delay(3000);

    Serial.println("\n\n=========================================");
    Serial.printf("  ESP32-S3 ALERT FIRMWARE v%s\n", FIRMWARE_VERSION);
    Serial.printf("  Build:  %s %s\n", __DATE__, __TIME__);
    Serial.printf("  Device: %s\n", DEVICE_ID);
    Serial.println("=========================================\n");

    // LED
    ledBegin();
    setLED(LED_STARTUP);
    // Bouton SOS

    sosButton_init();

    // Watchdog hardware
    setupWatchdog();

    // initialisation du power manager

    power_init();

    // Initialiser le stockage avec la callback de republish

    // Initialisation du stockage
    storage_init();

    // Diagnostic partitions OTA
    debugAllPartitions();

    // Si on revient d'un OTA, on démarre la fenêtre d'observation
    runSelfTestAfterOTA();

    // Initialisation SIM7670G
    Serial.println("Waiting for SIM7670G...");
    while (!SentMessage("AT", 3000))
    {
        feedWatchdog();
        delay(1000);
    }
    Serial.println("SIM7670G OK");

    SentMessage("AT+CTZU=1", 3000); // sync auto temps réseau
    delay(1000);

    Serial.printf("Battery: %.1f%%\n", readBattery());

    // GNSS
    Serial.println("Powering GNSS...");
    SentMessage("AT+CGNSSPWR=1", 5000);
    delay(3000);

    // Carte SD (optionnelle - log local des fix GPS)
    SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
    if (!SD_MMC.begin("/sdcard", true))
    {
        Serial.println("SD Failed (non-critical)");
    }
    else
    {
        Serial.println("SD OK");
    }
    // intialisation d'audio
    audio_init();

    // LTE (PDP / APN) — utilisé pour CCLK et CSQ même si MQTT passe par Wi-Fi
    setupLTE();

#if MQTT_TRANSPORT == MQTT_TRANSPORT_WIFI
    Serial.println("[WIFI] Connecting to ...");
    if (!connectWiFi(WIFI_CONNECT_TIMEOUT_MS))
    {
        Serial.println("[WIFI] Could not connect at boot, will retry in loop");
    }
#endif

    // MQTT
    mqttTransport_begin(MQTT_BROKER_HOST, MQTT_BROKER_PORT, DEVICE_ID);
    if (isWiFiConnected())
    {
        mqttTransport_reconnect();
    }

    lastDeleteMs = millis();
    stateMachine_init();
    stateMachine_onStateChange(onStateChanged);
    stateMachine_dispatch(EVT_BOOT_OK);

    Serial.println("\n[INFO] Setup complete - entering loop()\n");
}

// ===========================================================================
//  LOOP
// ===========================================================================
void loop()
{
    feedWatchdog();
    // ── DIAGNOSTIC TEMPORAIRE — à retirer ensuite ──
    static uint32_t lastLoopTime = 0;
    uint32_t now = millis();
    uint32_t cycleTime = now - lastLoopTime;
    lastLoopTime = now;
    if (cycleTime > 200)
    { // log seulement si cycle > 200ms
        Serial.printf("[LOOP] cycle = %u ms\n", cycleTime);
    }
    sosButton_tick();
    if (sosButton_wasTripleClicked())
    {
        Serial.printf("[MAIN] Triple click detected! count=%d, duration=%dms\n",
                      sosButton_getPressCount(),
                      sosButton_getPressDurationMs());
        DeviceState state = stateMachine_getState();
        if (state == STATE_STANDBY)
        {
            stateMachine_dispatch(EVT_SOS_TRIGGERED);
        }
        else if (state == STATE_ACTION)
        {
            stateMachine_dispatch(EVT_USER_RESET);
        }
    }

    // ── 1. Maintenance Wi-Fi ────────────────────────────────────────────────

    // ── 1. Maintenance Wi-Fi (UNIQUEMENT si transport WiFi) ─────
#if MQTT_TRANSPORT == MQTT_TRANSPORT_WIFI
    static uint32_t lastWifiRetry = 0;
    if (!isWiFiConnected() && millis() - lastWifiRetry > WIFI_RETRY_INTERVAL_MS)
    {
        lastWifiRetry = millis();
        Serial.println("[WIFI] Disconnected - reconnecting");
        connectWiFi(WIFI_CONNECT_TIMEOUT_MS);
    }
#endif

    // ── 2. Maintenance MQTT ─────────────────────────────────────────────────
    static uint32_t lastMqttRetry = 0;
    if (isWiFiConnected() && !mqttTransport_isConnected() && millis() - lastMqttRetry > MQTT_RETRY_INTERVAL_MS)
    {
        lastMqttRetry = millis();
        mqttTransport_reconnect();
    }
    mqttTransport_loop();

    static bool wasMqttConnected = false;
    bool nowConnected = mqttTransport_isConnected();
    //
    if (nowConnected && !wasMqttConnected)
    {
        // MQTT vient de se reconnecter
        size_t pending = storage_count();
        if (pending > 0)
        {
            Serial.printf("[MAIN] MQTT reconnected — %u buffered alerts to flush\n",
                          (unsigned)pending);
        }
    }
    wasMqttConnected = nowConnected;

    // ── 3. Observation post-OTA ─────────────────────────────────────────────
    // Pendant cette fenêtre, la LED pulse en bleu (heartbeat)
    if (isOtaObserving())
    {
        ledHeartbeatTick(LED_OTA_OBSERVE, 250);
    }

    // ── 4. Rotation du log GPS sur SD (toutes les 5 min) ────────────────────
    if (millis() - lastDeleteMs >= DELETE_LOG_INTERVAL_MS)
    {
        if (SD_MMC.exists("/gps_log.csv"))
        {
            Serial.println("Deleting SD log...");
            SD_MMC.remove("/gps_log.csv");
        }
        lastDeleteMs = millis();
    }

    // ── 5. Lecture GPS ──────────────────────────────────────────────────────
    bool gpsOk = readGPS();
    if (gpsOk)
    {
        Serial.printf("GPS: %.6f, %.6f\n",
                      currentGPS.latitude, currentGPS.longitude);
        if (!isOtaObserving())
            setLED(LED_GPS_OK);
        logGPS();
    }
    else
    {
        Serial.println("GPS: waiting for fix...");
        if (!isOtaObserving())
            setLED(LED_GPS_SEARCH);
    }

    // ── 6. Envoi ALERTE MQTT (si fix GPS + intervalle écoulé) ──────────────
    if (stateMachine_getState() == STATE_ACTION && (millis() - lastAlertMs > ALERT_INTERVAL_MS))
    {
        lastAlertMs = millis();

        if (mqttTransport_isConnected())
        {
            String payload = buildAlertJson();
            Serial.println("================ ALERT JSON ================");
            Serial.println(payload);
            Serial.printf("Size: %u bytes\n", payload.length());
            Serial.println("============================================");
            mqttTransport_publishAlert(payload);
        }
        else
        {
            Serial.println("[ALERT] MQTT not connected, skipping publish");
            setLED(LED_ERROR);
        }
    }
    if (mqttTransport_isConnected() && storage_count() > 0)
    { // appelle du callback
        storage_flush(republishAlert);
    }
    // ── 7. Envoi TÉLÉMÉTRIE MQTT (toutes les 10 min) ──────────────────────────
    uint32_t telemetryInterval = (stateMachine_getState() == STATE_ACTION)
                                     ? TELEMETRY_INTERVAL_ACTION_MS
                                     : TELEMETRY_INTERVAL_STANDBY_MS;

    if (millis() - lastTelemetryMs > telemetryInterval)
    {
        lastTelemetryMs = millis();
        if (mqttTransport_isConnected())
        {
            String tele = buildTelemetryJson();
            Serial.println("[TELEMETRY] " + tele);
            mqttTransport_publishTelemetry(tele);
        }
    }
    // ── 8. Heartbeat STATE (toutes les 60s, retain=true) ─────────

    uint32_t stateInterval = (stateMachine_getState() == STATE_ACTION)
                                 ? STATE_HEARTBEAT_ACTION_MS
                                 : STATE_HEARTBEAT_STANDBY_MS;
    if (millis() - lastStateMs > stateInterval)
    {                           // ← NOUVEAU
        lastStateMs = millis(); // ← NOUVEAU
        if (mqttTransport_isConnected())
        {
            String stateJson = buildStateJson();
            Serial.println("[STATE] " + stateJson);
            mqttTransport_publishState(stateJson);
        } // ← NOUVEAU
    }

    uint32_t loopEnd = millis() + MAIN_LOOP_DELAY_MS;
    while (millis() < loopEnd)
    {
        sosButton_tick();
        if (sosButton_wasTripleClicked())
        {
            Serial.printf("[MAIN] Triple click detected! count=%d, duration=%dms\n",
                          sosButton_getPressCount(),
                          sosButton_getPressDurationMs());
            DeviceState s = stateMachine_getState();
            if (s == STATE_STANDBY)
            {
                stateMachine_dispatch(EVT_SOS_TRIGGERED);
            }
            else if (s == STATE_ACTION)
            {
                stateMachine_dispatch(EVT_USER_RESET);
            }
        }
        delay(10);
    }
}
