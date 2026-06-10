# 📑 Spécification Technique Firmware : Alert-Telemetry-Ota (V0-Nv)

Ce document détaille l'ingénierie logicielle et matérielle du firmware de référence pour le dispositif **HelpMee**, basé sur l'architecture **ESP32-S3**.

---

## 🏗️ Architecture Logicielle (Software Stack)

Le firmware est structuré en couches pour assurer l'abstraction du matériel et la portabilité de la logique métier :

1.  **Application Layer (`main.cpp`)** : Orchestration des tâches, construction des payloads JSON (ArduinoJson), et gestion des timers globaux.
2.  **Service Layer (`state_machine.cpp`, `ota.cpp`, `storage.cpp`)** : Services système transverses (Machine à états, mise à jour sécurisée, file d'attente persistante).
3.  **Transport Layer (`mqtt_transport.cpp`, `mqtt_client.cpp`, `mqtt_lte.cpp`)** : Abstraction de la connectivité. Supporte nativement le basculement WiFi/LTE Cat-1.
4.  **Hardware Abstraction Layer (HAL) (`lte.cpp`, `gps.cpp`, `battery.cpp`, `led.cpp`)** : Drivers spécifiques aux composants (SIM7670G, MAX17048, NeoPixel).

---

## 🚦 Logique de Commande : Machine à États Finis (FSM)

Le cycle de vie du produit est régi par une FSM déterministe (`DeviceState`) :

*   **`STATE_OFF`** : État initial. Autotests au boot. Passage vers `STANDBY` après `EVT_BOOT_OK`.
*   **`STATE_STANDBY`** : 
    *   **Horloge CPU** : 80 MHz (économie).
    *   **Activité** : Monitoring GPS passif, heartbeat MQTT toutes les 60s.
    *   **Transition** : Bascule vers `ACTION` sur `EVT_SOS_TRIGGERED` (Triple-clic ou chute).
*   **`STATE_ACTION`** :
    *   **Horloge CPU** : 240 MHz (performance).
    *   **Activité** : Publication d'alertes haute priorité toutes les 10s. LED rouge pulsante.
    *   **Transition** : Retour vers `STANDBY` sur `EVT_USER_RESET` (via bouton ou commande MQTT).

---

## 📡 Gestion de la Connectivité & Résilience

### Stratégie Hybride (Dual-Transport)
Le firmware utilise un "Transport Provider" abstrait. La sélection se fait via `#define MQTT_TRANSPORT` dans `config.h` :
- **Mode WiFi** : Utilise la pile TCP/IP native de l'ESP32 et `PubSubClient`.
- **Mode LTE** : Pilotage direct du modem **SIM7670G** via commandes AT (UART1) pour la pile MQTT interne du module.

### File d'Attente Offline (Store-and-Forward)
En cas de déconnexion (`mqttTransport_isConnected() == false`) :
- Les payloads critiques sont sérialisés et stockés sur **LittleFS** (`/queue/msg_XXXXXXXXXX.json`).
- Capacité : 100 messages (FIFO - suppression du plus ancien si plein).
- **Auto-Flush** : Dès le rétablissement de la connexion, un service de rejeu (`storage_flush`) vide la file d'attente à un débit régulier pour ne pas saturer le broker.

---

## 🛡️ Mécanismes de Sûreté (Safety)

### Processus OTA Transactionnel
1.  **Notification** : Réception d'un JSON via MQTT (URL, SHA256, Taille).
2.  **Streaming** : Téléchargement par blocs de 1 Ko.
3.  **Vérification** : Calcul du hash **SHA256** (`mbedtls/sha256`) à la volée. Si le hash calculé ≠ hash attendu, l'image est rejetée.
4.  **Partitionnement** : Écriture dans la partition `app1` (OTA_0) ou `app2` (OTA_1).
5.  **Validation Post-Reboot** : Si le firmware crash au boot, l'ESP32-S3 effectue un **rollback automatique** vers la partition précédente stable.

### Watchdog (WDT)
- **Timeout** : 60 secondes.
- **Principe** : Toutes les boucles bloquantes (GPS wait, OTA download, LTE init) "nourrissent" le chien de garde (`feedWatchdog`).

---

## ⚡ Spécifications Électriques & Pinout

| Signal | GPIO | Protocole | Description |
| :--- | :--- | :--- | :--- |
| **UART1 RX/TX** | 17 / 18 | AT Commands | Interface Modem SIM7670G |
| **I2C SDA/SCL** | 15 / 16 | I2C | Capteur Batterie MAX17048 |
| **SDIO CLK/CMD/D0** | 5 / 4 / 6 | SD_MMC | Stockage SD interne (Logs/Audio) |
| **SOS_BTN** | 0 | Digital Input | Bouton utilisateur (Pull-up externe) |
| **RGB_LED** | 38 | NeoPixel | Feedback visuel d'état |
| **PERIPH_PWR** | 21 | Output | Pilotage alimentation périphériques |

---

## 📦 Structure des Payloads (JSON)

### Alertes (`CRITICAL`)
```json
{
  "device_id": "esp32-s3-test-01",
  "priority": "CRITICAL",
  "location": { "latitude": 36.8, "longitude": 10.1, "speed_kmh": 0.0 },
  "device_status": { "battery_level_pct": 85, "connectivity_type": "LTE" },
  "user_profile": { "full_name": "...", "blood_type": "A+" }
}
```

---

## 🛠️ Maintenance & Evolution
- **Build System** : PlatformIO (Framework Arduino v3.0+).
- **Partitions** : `default_16MB.csv` (16MB Flash support).
- **Versions** : Suivi sémantique (v1.x.x) défini dans `platformio.ini`.

**Auteur :** Helmi - *Engineering Department*
