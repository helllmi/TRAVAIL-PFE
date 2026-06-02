# 🚀 Projet de Fin d'Études (PFE) : Écosystème IoT, Télémétrie GPS & Traitement Audio

Ce dépôt constitue une forge logicielle complète pour un système IoT hétérogène basé sur les microcontrôleurs **ESP32** et **ESP32-S3**. Il couvre l'ensemble de la chaîne de valeur : acquisition de données (capteurs, audio), transmission sans fil (WiFi, 4G LTE, MQTT, RTSP) et visualisation via des backends Node.js.

---

## 📂 Documentation Détaillée par Module

### 1. [audio broadcasting with rtsp server](./audio%20broadcasting%20with%20rtsp%20server)
Diffusion audio temps réel via le protocole RTSP.
*   **Hardware** : ESP32 + Microphone MEMS **INMP441**.
    *   Pinout I2S : SCK (33), WS (25), SD (32).
*   **Firmware** : Développé sous PlatformIO. Utilise `FreeRTOS` pour un buffer circulaire thread-safe.
*   **Protocoles** : RTSP (Real-Time Streaming Protocol) sur RTP.
*   **Utilisation** : Flux accessible via VLC (`rtsp://<IP_ESP32>`) ou `ffplay`.

### 2. [AUDIO-RECORD](./AUDIO-RECORD) & [AUDIO-RECORD-ADPCM](./AUDIO-RECORD-ADPCM)
Systèmes d'enregistrement audio sur mémoire interne.
*   **Standard (PCM)** : Capture en WAV 16-bit, 8/16 kHz. Stockage sur **SPIFFS**.
*   **Optimisé (ADPCM)** : Utilisation de la compression Adaptive Differential Pulse Code Modulation pour diviser l'espace de stockage par 4.
*   **Interface** : Serveur web intégré pour le déclenchement et le téléchargement des fichiers via navigateur.

### 3. [ESP-S3-ALERT](./ESP-S3-ALERT)
Dispositif d'alerte critique 4G/GNSS pour **ESP32-S3**.
*   **Modem** : SIM7670G (LTE Cat-1). UART : RX (17), TX (18).
*   **Capteurs** : Jauge de batterie **MAX17048** (I2C : SDA 15, SCL 16).
*   **Stockage** : Journalisation sur MicroSD en mode **SD_MMC** (Pins 4, 5, 6).
*   **Fonction** : Envoi de positions JSON par HTTP POST sur détection d'événement.

### 4. [GPS-monitor](./GPS-monitor)
Système complet de suivi via requêtes HTTP.
*   📂 **[gps-backend](./GPS-monitor/gps-backend)** : 
    *   Stack : Node.js, Express, Socket.io.
    *   Fonction : API REST de réception, persistance JSON (`data/locations.json`) et push temps réel vers l'UI.
*   📂 **[Monitoring GPS](./GPS-monitor/Monitoring%20GPS)** : 
    *   Hardware : ESP32 + NEO-7M (Pins 16 RX / 17 TX).
    *   Logique : Acquisition NMEA via `TinyGPS++` et transmission HTTP POST.

### 5. [TEST GPS](./TEST%20GPS)
Banc de test comparatif orienté **MQTT**.
*   📂 **[GPS_TRACKER](./TEST%20GPS/GPS_TRACKER)** : Backend Node.js configuré pour s'abonner à un broker MQTT (ex: Mosquitto) et redistribuer les données via WebSockets.
*   📂 **[GPS-TRACKER](./TEST%20GPS/GPS-TRACKER)** : Firmware ESP32 utilisant `PubSubClient` pour publier les coordonnées sur le topic `esp32/gps`.

### 6. [ota premiere version](./ota%20premiere%20version)
Maintenance à distance des équipements.
*   **Serveur** : Script Python gérant la distribution des binaires.
*   **Firmware** : Intègre la bibliothèque `Update.h` de l'ESP32 pour le flashage à chaud via flux HTTP déclenché par MQTT.

### 7. [Alert-Telemetry-Ota V0 -Nv-Firmware](./Alert-Telemetry-Ota%20V0%20-Nv-Firmware)
Le module le plus abouti du projet, intégrant une logique métier complète et une robustesse industrielle.
*   **FSM (Machine à États)** : Gestion intelligente des modes Standby (économie d'énergie) et Action (alerte active).
*   **Connectivité LTE/WiFi** : Basculement automatique et file d'attente LittleFS pour les messages hors-ligne.
*   **OTA Sécurisé** : Mise à jour à distance avec vérification SHA256 et mécanisme de rollback automatique.
*   **Hardware** : ESP32-S3 + SIM7670G + Fuel Gauge MAX17048.

### 8. [wifi-manager](./wifi-manager)
Gestionnaire de provisionnement réseau.
*   **Concept** : Portail captif autonome si le WiFi est absent.
*   **Stockage** : Sauvegarde des credentials en mémoire **NVS** via **LittleFS**.
*   **Action** : Téléverser l'image du système de fichiers via `Upload Filesystem Image` sur PlatformIO.

---

## 🛠️ Stack Technique Transverse

| Domaine | Technologies |
| :--- | :--- |
| **Microcontrôleurs** | ESP32, ESP32-S3 (Xtensa Dual-Core) |
| **Langages** | C++ (Embedded), JavaScript (Node.js), Python |
| **Gestion Audio** | I2S, PCM, ADPCM, RTSP, RTP |
| **Connectivité** | 4G LTE Cat-1, GNSS, WiFi, MQTT, WebSocket, HTTP/REST |
| **Sécurité** | Hash SHA256, OTA Rollback, Watchdog matériel |
| **Outils de Dev** | PlatformIO, Arduino IDE, NPM, Git |

## 📐 Schéma de Connexion Typique (GNSS)
| Composant | ESP32 Pin |
| :--- | :--- |
| GPS RX | GPIO 17 (TX ESP) |
| GPS TX | GPIO 16 (RX ESP) |
| Micro SD (DAT0) | GPIO 2 |
| Battery SDA | GPIO 15 |
| Battery SCL | GPIO 16 |

## 🤵 Auteur
**Helmi** - *Expert IoT & Systèmes Embarqués*
