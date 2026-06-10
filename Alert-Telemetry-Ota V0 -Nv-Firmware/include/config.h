
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
//  IDENTIFICATION DU DEVICE
// ============================================================================
#define DEVICE_ID "esp32-s3-test-01"


// ============================================================================
//  MQTT
// ============================================================================
#define MQTT_BROKER_HOST "broker.hivemq.com" // IP de ton PC qui héberge Mosquitto
#define MQTT_BROKER_PORT 1883
#define MQTT_KEEPALIVE_SEC 30
#define MQTT_BUFFER_SIZE 2048 // taille du buffer interne PubSubClient

// ============================================================================
//  LTE / APN
// ============================================================================
#define LTE_APN "internet.ooredoo.tn"

// ============================================================================
//  TIMINGS DU LOOP PRINCIPAL
// ============================================================================
#define ALERT_INTERVAL_MS 10000            // alerte toutes les 10s minimum
#define TELEMETRY_INTERVAL_MS 60000        // télémétrie toutes les 60s
#define DELETE_LOG_INTERVAL_MS (5 * 60000) // rotation log GPS SD toutes les 5min

#define WIFI_RETRY_INTERVAL_MS 10000  // retry connexion WiFi si down
#define MQTT_RETRY_INTERVAL_MS 5000   // retry connexion MQTT si down
#define WIFI_CONNECT_TIMEOUT_MS 15000 // timeout connexion WiFi initiale

#define MAIN_LOOP_DELAY_MS 2000 // delay() à la fin de loop()

// ============================================================================
//  PINS GPIO
// ============================================================================
// SIM7670G (UART1)
#define PIN_SIM_RX 17
#define PIN_SIM_TX 18

// I2C (battery sensor MAX17048)
#define PIN_I2C_SDA 15
#define PIN_I2C_SCL 16

// Alimentation périphériques externes
#define PIN_POWER_PERIPH 21

// Carte SD (SDIO)
#define PIN_SD_CLK 5
#define PIN_SD_CMD 4
#define PIN_SD_D0 6

// Bouton sos
#define PIN_SOS_BUTTON 0
#define SOS_TRIPLE_WINDOW_MS 800 // fenêtre totale pour les 3 clics
#define SOS_DEBOUNCE_MS 50       // anti-rebond logiciel
#define SOS_CLICK_MAX_MS 300     // durée max d'un clic court (sinon = long press)
// ============================================================================
//  STORAGE - Buffer offline LittleFS
// ============================================================================
#define STORAGE_MAX_MESSAGES 100
#define STORAGE_DIR_QUEUE "/queue"

// ============================================================================
//  POWER MANAGEMENT
// ============================================================================
#define CPU_FREQ_ACTION_MHZ 240 // pleine puissance en ACTION
#define CPU_FREQ_STANDBY_MHZ 80 // économie en STANDBY
#define CPU_FREQ_BOOT_MHZ 240   // pleine puissance au boot
// NeoPixel : défini dans led.h (PIN 38)

// ============================================================================
//  WATCHDOG
// ============================================================================
// La config watchdog est dans watchdog.h (WDT_TIMEOUT_S = 60s)
// On ne la duplique pas ici pour éviter les conflits.
// ============================================================================
//  TRANSPORT MQTT — choix WiFi / LTE              ← NOUVEAU BLOC
// ============================================================================
#define MQTT_TRANSPORT_WIFI   0
#define MQTT_TRANSPORT_LTE    1

// Transport actif. Pour l'instant WiFi (testé). Passer à _LTE quand le
// device est dispo pour tester le MQTT 4G.
#define MQTT_TRANSPORT  MQTT_TRANSPORT_LTE

#endif