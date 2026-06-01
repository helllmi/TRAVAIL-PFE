/**
 * ===========================================================================
 *  Firmware OTA Test - v1.0.0  (version stable initiale)
 * ===========================================================================
 *  Mécanismes implémentés:
 *    1. Watchdog hardware (Task Watchdog Timer)
 *    2. Connexion Wi-Fi + MQTT (PubSubClient)
 *    3. Réception OTA via MQTT push (topic devices/{id}/ota/notify)
 *    4. Téléchargement HTTP streaming + vérif SHA256
 *    5. Marquage NVS "OTA pending validation" avant reboot
 *    6. Watchdog applicatif post-OTA (5min d'observation Wi-Fi+MQTT)
 *    7. Rollback manuel en cas d'échec persistant
 *
 *  LED: clignote LENTEMENT (1 Hz) - signature visuelle de la v1
 * ===========================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <mbedtls/sha256.h>

// ===========================================================================
//  CONFIGURATION - À ADAPTER
// ===========================================================================
const char* WIFI_SSID     = "NA Stagiaires";
const char* WIFI_PASSWORD = "tage@N*2023*A";

const char* MQTT_BROKER = "172.29.26.29";  // IP de ton PC (Mosquitto + Flask)
const uint16_t MQTT_PORT = 1883;

const char* DEVICE_ID = "esp32-test-01";

const uint32_t WDT_TIMEOUT_S = 60;
const int LED_PIN = 2;

// Configuration du watchdog applicatif post-OTA
// (réduit à 1 min pour les tests, à passer à 5 min en prod)
const uint32_t OTA_OBSERVATION_PERIOD_MS = 60 * 1000;     // 1 minute
const uint32_t OTA_HEALTH_CHECK_INTERVAL_MS = 5 * 1000;   // check toutes les 5s
const uint8_t  OTA_MAX_FAILURES = 3;                      // 3 échecs consécutifs

// ===========================================================================
//  ÉTAT GLOBAL
// ===========================================================================
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
Preferences preferences;

String topicOtaNotify;
String topicOtaStatus;
String topicLwt;

uint32_t lastBlinkMs = 0;
bool ledState = false;
bool otaInProgress = false;

// État du watchdog OTA
bool     otaObservationActive = false;
uint32_t otaObservationStartMs = 0;
uint32_t otaLastCheckMs = 0;
uint8_t  otaConsecutiveFailures = 0;

// Constantes NVS
const char* NVS_NAMESPACE = "ota";
const char* NVS_KEY_PENDING = "pending";

// ===========================================================================
//  WATCHDOG HARDWARE
// ===========================================================================
void setupWatchdog() {
    Serial.printf("[WDT] Init watchdog (timeout=%us)\n", WDT_TIMEOUT_S);
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WDT_TIMEOUT_S * 1000,
        .idle_core_mask = 0,
        .trigger_panic = true,
    };
    esp_task_wdt_reconfigure(&wdt_config);
#else
    esp_task_wdt_init(WDT_TIMEOUT_S, true);
#endif
    esp_task_wdt_add(NULL);
}

void feedWatchdog() {
    esp_task_wdt_reset();
}

// ===========================================================================
//  NVS - GESTION DU DRAPEAU "OTA PENDING VALIDATION"
// ===========================================================================
void markOtaPendingValidation() {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.putBool(NVS_KEY_PENDING, true);
    preferences.end();
    Serial.println("[NVS] Marked OTA as pending validation");
}

bool isOtaPendingValidation() {
    preferences.begin(NVS_NAMESPACE, true);
    bool pending = preferences.getBool(NVS_KEY_PENDING, false);
    preferences.end();
    return pending;
}

void confirmOtaSuccess() {
    preferences.begin(NVS_NAMESPACE, false);
    preferences.remove(NVS_KEY_PENDING);
    preferences.end();
    Serial.println("[NVS] OTA validation flag cleared");
}

// ===========================================================================
//  DEBUG PARTITIONS
// ===========================================================================
void debugAllPartitions() {
    Serial.println("\n========== PARTITIONS DEBUG ==========");
    const esp_partition_t* running = esp_ota_get_running_partition();
    Serial.printf("Running on: %s @ 0x%x\n", running->label, running->address);

    esp_partition_iterator_t it = esp_partition_find(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);

    while (it != NULL) {
        const esp_partition_t* p = esp_partition_get(it);
        esp_ota_img_states_t state;
        esp_err_t err = esp_ota_get_state_partition(p, &state);

        const char* stateStr = "EMPTY";
        if (err == ESP_OK) {
            switch (state) {
                case ESP_OTA_IMG_NEW:            stateStr = "NEW"; break;
                case ESP_OTA_IMG_PENDING_VERIFY: stateStr = "PENDING_VERIFY"; break;
                case ESP_OTA_IMG_VALID:          stateStr = "VALID"; break;
                case ESP_OTA_IMG_INVALID:        stateStr = "INVALID"; break;
                case ESP_OTA_IMG_ABORTED:        stateStr = "ABORTED"; break;
                case ESP_OTA_IMG_UNDEFINED:      stateStr = "UNDEFINED"; break;
            }
        }

        Serial.printf("  %s @ 0x%x: state=%s%s\n",
                      p->label, p->address, stateStr,
                      (p == running) ? "  <-- RUNNING" : "");

        it = esp_partition_next(it);
    }
    esp_partition_iterator_release(it);
    Serial.println("======================================\n");
}

// ===========================================================================
//  WIFI - retourne bool, ne reboote JAMAIS
// ===========================================================================
bool connectWiFi() {
    Serial.printf("[WIFI] Connecting to %s...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
        delay(500);
        Serial.print(".");
        feedWatchdog();
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WIFI] Connected. IP: %s\n",
                      WiFi.localIP().toString().c_str());
        return true;
    }
    Serial.println("\n[WIFI] Connection failed");
    return false;
}

// ===========================================================================
//  PROCESSUS OTA
// ===========================================================================
void shaToHex(uint8_t* hash, String& out) {
    out = "";
    for (int i = 0; i < 32; i++) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", hash[i]);
        out += buf;
    }
}

bool performOTA(const String& url, const String& expectedSha256, size_t expectedSize) {
    Serial.println("\n========== OTA UPDATE START ==========");
    Serial.printf("[OTA] URL: %s\n", url.c_str());
    Serial.printf("[OTA] Expected SHA256: %s\n", expectedSha256.c_str());

    mqtt.publish(topicOtaStatus.c_str(), "{\"state\":\"downloading\"}");

    HTTPClient http;
    http.begin(url);
    http.setTimeout(30000);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[OTA] HTTP error: %d\n", httpCode);
        mqtt.publish(topicOtaStatus.c_str(), "{\"state\":\"failed\",\"reason\":\"http\"}");
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
        Serial.println("[OTA] Invalid content length");
        http.end();
        return false;
    }

    if (!Update.begin(contentLength)) {
        Serial.printf("[OTA] Update.begin failed: %s\n", Update.errorString());
        mqtt.publish(topicOtaStatus.c_str(), "{\"state\":\"failed\",\"reason\":\"begin\"}");
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    mbedtls_sha256_context shaCtx;
    mbedtls_sha256_init(&shaCtx);
    mbedtls_sha256_starts(&shaCtx, 0);

    uint8_t buf[1024];
    size_t totalWritten = 0;
    uint32_t lastProgress = 0;

    while (http.connected() && (totalWritten < (size_t)contentLength)) {
        size_t available = stream->available();
        if (available > 0) {
            int read = stream->readBytes(buf, min(available, sizeof(buf)));
            if (read > 0) {
                if (Update.write(buf, read) != (size_t)read) {
                    Update.abort();
                    http.end();
                    mqtt.publish(topicOtaStatus.c_str(),
                                 "{\"state\":\"failed\",\"reason\":\"write\"}");
                    return false;
                }
                mbedtls_sha256_update(&shaCtx, buf, read);
                totalWritten += read;

                uint32_t pct = (totalWritten * 100) / contentLength;
                if (pct >= lastProgress + 10) {
                    Serial.printf("[OTA] Progress: %u%%\n", pct);
                    lastProgress = pct;
                }
            }
        }
        feedWatchdog();
        delay(1);
    }
    http.end();

    uint8_t computedHash[32];
    mbedtls_sha256_finish(&shaCtx, computedHash);
    mbedtls_sha256_free(&shaCtx);

    String computed;
    shaToHex(computedHash, computed);

    if (computed != expectedSha256) {
        Serial.println("[OTA] SHA256 MISMATCH - aborting");
        Serial.printf("[OTA] Computed: %s\n", computed.c_str());
        Update.abort();
        mqtt.publish(topicOtaStatus.c_str(),
                     "{\"state\":\"failed\",\"reason\":\"sha256\"}");
        return false;
    }
    Serial.println("[OTA] SHA256 verified OK");

    if (!Update.end(true) || !Update.isFinished()) {
        Serial.printf("[OTA] Update.end failed: %s\n", Update.errorString());
        return false;
    }

    Serial.println("[OTA] Success - rebooting in 3s");
    mqtt.publish(topicOtaStatus.c_str(), "{\"state\":\"installing\"}", true);

    // CRITIQUE : marque que la prochaine boot devra être validée
    markOtaPendingValidation();

    delay(3000);
    ESP.restart();
    return true;
}

// ===========================================================================
//  CALLBACK MQTT
// ===========================================================================
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
    Serial.printf("\n[MQTT] Message received on '%s' (%u bytes)\n", topic, length);

    if (otaInProgress) {
        Serial.println("[MQTT] OTA already in progress, ignoring");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
        Serial.printf("[MQTT] JSON parse error: %s\n", err.c_str());
        return;
    }

    String version = doc["version"].as<String>();
    String url     = doc["url"].as<String>();
    String sha256  = doc["sha256"].as<String>();
    size_t size    = doc["size"] | 0;

    Serial.printf("[MQTT] OTA notify: v%s, size=%u\n", version.c_str(), size);

    if (version == FIRMWARE_VERSION) {
        Serial.println("[MQTT] Already on this version, ignoring");
        return;
    }

    if (url.length() == 0 || sha256.length() != 64) {
        Serial.println("[MQTT] Invalid payload");
        return;
    }

    otaInProgress = true;
    performOTA(url, sha256, size);
    otaInProgress = false;
}

// ===========================================================================
//  CONNEXION MQTT avec LWT
// ===========================================================================
bool connectMqtt() {
    Serial.printf("[MQTT] Connecting to %s:%u...\n", MQTT_BROKER, MQTT_PORT);
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(onMqttMessage);
    mqtt.setBufferSize(1024);

    bool connected = mqtt.connect(
        DEVICE_ID,
        nullptr, nullptr,
        topicLwt.c_str(), 1, true,
        "{\"state\":\"offline\"}"
    );

    if (!connected) {
        Serial.printf("[MQTT] Failed, rc=%d\n", mqtt.state());
        return false;
    }

    Serial.println("[MQTT] Connected");
    mqtt.publish(topicLwt.c_str(), "{\"state\":\"online\"}", true);
    mqtt.subscribe(topicOtaNotify.c_str(), 1);
    Serial.printf("[MQTT] Subscribed to %s\n", topicOtaNotify.c_str());
    return true;
}

// ===========================================================================
//  ROLLBACK MANUEL (compatible Arduino-ESP32)
// ===========================================================================
void manualRollback() {
    Serial.println("\n[ROLLBACK] ========== MANUAL ROLLBACK START ==========");

    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* previous = esp_ota_get_next_update_partition(NULL);

    Serial.printf("[ROLLBACK] Currently running on: %s @ 0x%x\n",
                  running->label, running->address);

    if (previous == NULL) {
        Serial.println("[ROLLBACK] No previous partition - reboot only");
        delay(2000);
        ESP.restart();
        return;
    }

    Serial.printf("[ROLLBACK] Switching to: %s @ 0x%x\n",
                  previous->label, previous->address);

    esp_err_t err = esp_ota_set_boot_partition(previous);
    if (err != ESP_OK) {
        Serial.printf("[ROLLBACK] set_boot_partition FAILED: %s\n",
                      esp_err_to_name(err));
        delay(2000);
        ESP.restart();
        return;
    }

    // On garde le drapeau pending=true intentionnellement
    // → après le reboot sur l'ancienne partition, si Wi-Fi OK, le drapeau sera nettoyé
    Serial.println("[ROLLBACK] Rebooting in 2 seconds...");
    Serial.println("[ROLLBACK] ========== END ==========\n");
    delay(2000);
    ESP.restart();
}

// ===========================================================================
//  SELF-TEST APRÈS OTA - démarre la fenêtre d'observation
// ===========================================================================
void runSelfTestAfterOTA() {
    Serial.println("\n[SELFTEST] ========== START ==========");

    bool pending = isOtaPendingValidation();
    Serial.printf("[SELFTEST] OTA pending validation: %s\n",
                  pending ? "YES" : "NO");

    if (!pending) {
        Serial.println("[SELFTEST] Regular boot - no observation needed");
        Serial.println("[SELFTEST] ========== END ==========\n");
        return;
    }

    Serial.println("[SELFTEST] >>> POST-OTA BOOT DETECTED <<<");
    Serial.printf("[SELFTEST] Starting %u-second observation window\n",
                  OTA_OBSERVATION_PERIOD_MS / 1000);
    Serial.printf("[SELFTEST] Health checks every %u seconds\n",
                  OTA_HEALTH_CHECK_INTERVAL_MS / 1000);
    Serial.printf("[SELFTEST] Will rollback after %u consecutive failures\n",
                  OTA_MAX_FAILURES);

    otaObservationActive    = true;
    otaObservationStartMs   = millis();
    otaLastCheckMs          = millis();
    otaConsecutiveFailures  = 0;

    Serial.println("[SELFTEST] ========== END ==========\n");
}

// ===========================================================================
//  HEALTH CHECK : retourne true si Wi-Fi + MQTT sont OK
// ===========================================================================
bool runHealthCheck() {
    bool wifiOK = (WiFi.status() == WL_CONNECTED);
    bool mqttOK = mqtt.connected();
    Serial.printf("[HEALTH] WiFi=%s, MQTT=%s\n",
                  wifiOK ? "OK" : "FAIL",
                  mqttOK ? "OK" : "FAIL");
    return wifiOK && mqttOK;
}

// ===========================================================================
//  WATCHDOG OTA - tick à appeler dans loop()
// ===========================================================================
void otaObservationTick() {
    if (!otaObservationActive) return;

    uint32_t now = millis();
    uint32_t elapsed = now - otaObservationStartMs;

    // ★ FIX 1 : on déplace la vérification "période terminée" À LA FIN
    // pour donner la priorité à la détection d'échec

    // Check périodique
    if (now - otaLastCheckMs >= OTA_HEALTH_CHECK_INTERVAL_MS) {
        otaLastCheckMs = now;

        uint32_t remainingS = (OTA_OBSERVATION_PERIOD_MS - elapsed) / 1000;
        Serial.printf("\n[WATCHDOG] Health check (%u sec remaining)\n", remainingS);

        bool healthy = runHealthCheck();

        if (healthy) {
            if (otaConsecutiveFailures > 0) {
                Serial.printf("[WATCHDOG] Recovered (was %u failures)\n",
                              otaConsecutiveFailures);
            }
            otaConsecutiveFailures = 0;
        } else {
            otaConsecutiveFailures++;
            Serial.printf("[WATCHDOG] Failure %u/%u\n",
                          otaConsecutiveFailures, OTA_MAX_FAILURES);

            if (otaConsecutiveFailures >= OTA_MAX_FAILURES) {
                Serial.println("\n[WATCHDOG] ========== ROLLBACK TRIGGERED ==========");
                Serial.printf("[WATCHDOG] %u consecutive failures - rolling back\n",
                              OTA_MAX_FAILURES);
                otaObservationActive = false;
                manualRollback();
                return;  // ne devrait jamais s'exécuter (manualRollback reboote)
            }
        }
    }

    // ★ FIX 1 (suite) : MAINTENANT on vérifie si la période est terminée
    // Si on est arrivé ici, c'est qu'on n'a pas atteint le seuil d'échecs
    // MAIS on doit aussi rejeter si on a accumulé des échecs sans atteindre 3
    if (elapsed >= OTA_OBSERVATION_PERIOD_MS) {
        if (otaConsecutiveFailures > 0) {
            // ★ NOUVEAU : on a fini la période MAIS avec des échecs en cours
            // → on rollback par sécurité (le firmware n'est pas stable)
            Serial.println("\n[WATCHDOG] ========== PERIOD ENDED WITH FAILURES ==========");
            Serial.printf("[WATCHDOG] %u failures at end of period - rolling back\n",
                          otaConsecutiveFailures);
            otaObservationActive = false;
            manualRollback();
            return;
        }

        // Période finie sans échec → c'est un succès
        Serial.println("\n[WATCHDOG] ========== OBSERVATION COMPLETE ==========");
        Serial.printf("[WATCHDOG] Stable for %u seconds - confirming OTA\n",
                      OTA_OBSERVATION_PERIOD_MS / 1000);
        confirmOtaSuccess();
        otaObservationActive = false;
    }
}
// ===========================================================================
//  SETUP
// ===========================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n=====================================");
    Serial.printf("  FIRMWARE v%s (MQTT)\n", FIRMWARE_VERSION);
    Serial.printf("  Build:   %s %s\n", __DATE__, __TIME__);
    Serial.printf("  Device:  %s\n", DEVICE_ID);
    Serial.println("=====================================\n");

    topicOtaNotify = String("devices/") + DEVICE_ID + "/ota/notify";
    topicOtaStatus = String("devices/") + DEVICE_ID + "/ota/status";
    topicLwt       = String("devices/") + DEVICE_ID + "/lwt";

    pinMode(LED_PIN, OUTPUT);
    setupWatchdog();
    debugAllPartitions();

    runSelfTestAfterOTA();  // démarre la fenêtre d'observation si pending

    // Connexion (sans bloquer ni rebooter en cas d'échec)
    if (connectWiFi()) {
        connectMqtt();
    }

    Serial.println("[INFO] Setup complete - entering loop()");
}

// ===========================================================================
//  LOOP
// ===========================================================================
void loop() {
    feedWatchdog();

    // LED clignote LENTEMENT (5 Hz) - signature visuelle de la v1
    if (millis() - lastBlinkMs > 100) {
        lastBlinkMs = millis();
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState);
    }

    // Reconnexion auto Wi-Fi
    static uint32_t lastWifiRetry = 0;
    if (WiFi.status() != WL_CONNECTED && millis() - lastWifiRetry > 10000) {
        lastWifiRetry = millis();
        Serial.println("[WIFI] Disconnected - reconnecting");
        connectWiFi();
    }

    // Reconnexion auto MQTT
    static uint32_t lastMqttRetry = 0;
    if (WiFi.status() == WL_CONNECTED && !mqtt.connected()
        && millis() - lastMqttRetry > 5000) {
        lastMqttRetry = millis();
        connectMqtt();
    }

    mqtt.loop();

    // Surveillance post-OTA
    otaObservationTick();

    delay(10);
}
