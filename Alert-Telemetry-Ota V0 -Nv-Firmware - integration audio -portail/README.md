# 🚨 HelpMee v1.0.0 — Système d'Alerte IoT (Alert, Telemetry, OTA, Audio, Portal)

Ce module est la version la plus complète et la plus robuste du projet **HelpMee**. Il intègre une gestion avancée des alertes critiques, une télémétrie complète, une mise à jour à distance (OTA), un enregistrement audio de sécurité et un portail captif de configuration.

---

## 🌟 Fonctionnalités Clés

### 1. Système d'Alerte SOS "Critical"
*   **Déclenchement** : Par appui long (≥ 3s) sur le bouton SOS.
*   **Payload Enrichi** : Envoi de données JSON complètes incluant position GPS, niveau de batterie, profil médical de l'utilisateur (âge, groupe sanguin, antécédents) et contacts d'urgence.
*   **File d'Attente Offline** : Si le réseau est absent, l'alerte est stockée dans la mémoire Flash (**LittleFS**) et renvoyée automatiquement dès la reconnexion.

### 2. Connectivité Hybride & Robuste
*   **Dual-Transport** : Support du **WiFi** et de la **4G LTE (SIM7670G)**.
*   **Intelligence Réseau** : Basculement intelligent entre les modes de transport.
*   **MQTT** : Utilisation du protocole MQTT pour une transmission légère et bidirectionnelle.

### 3. Enregistrement Audio de Sécurité
*   **Capture Automatique** : Dès le déclenchement d'un SOS, le système enregistre l'audio environnant sur la **carte SD** (I2S via microphone MEMS).
*   **Format** : WAV 16-bit / 16kHz haute fidélité.
*   **Historique** : Les enregistrements sont horodatés et consultables via le portail captif.

### 4. Over-The-Air (OTA) Management
*   **Mise à jour Distante** : Déclenchée par un message MQTT sur un topic dédié.
*   **Sécurité** : Vérification de l'intégrité par hash **SHA256**.
*   **Robustesse** : Mécanisme de **Rollback automatique** en cas d'échec du boot après mise à jour (fenêtre d'observation de 60s).

### 5. Portail Captif de Configuration
*   **Accès** : Triple-clic sur le bouton SOS pour activer le point d'accès "HEELPMEE-Config".
*   **Interface Web** : Permet de configurer le WiFi (SSID/Pass), le broker MQTT, et de visualiser/télécharger les fichiers audio de la carte SD.

---

## 🛠️ Spécifications Hardware (ESP32-S3)

| Composant | Pins / Protocole |
| :--- | :--- |
| **Modem SIM7670G** | UART : RX (17), TX (18) |
| **Microphone I2S** | SCK (10), WS (11), SD (12) |
| **Carte SD** | SD_MMC : CLK (5), CMD (4), D0 (6) |
| **NeoPixel LED** | GPIO 38 |
| **Bouton SOS** | GPIO 7 |
| **Batterie (MAX17048)** | I2C : SDA (15), SCL (16) |

---

## 🏗️ Architecture Logicielle

Le firmware repose sur une **Machine à États Finis (FSM)** pour garantir une réactivité optimale :

1.  **STANDBY** : Mode veille active, monitoring batterie et signal, attente de déclenchement.
2.  **ACTION** : Mode SOS actif. Publication périodique des alertes, enregistrement audio en cours, LED rouge.

### Modules principaux :
*   `src/state_machine.cpp` : Cœur de la logique métier.
*   `src/ota.cpp` : Gestion du cycle de vie des mises à jour.
*   `src/audio.cpp` : Drivers I2S et gestion de l'écriture WAV sur SD.
*   `src/captive_portal.cpp` : Serveur web asynchrone et gestion DNS.
*   `src/mqtt_transport.cpp` : Couche d'abstraction pour le transport MQTT (WiFi/LTE).

---

## 🚀 Installation et Utilisation

### Compilation
Ce projet utilise **PlatformIO**. Pour compiler et téléverser :
1.  Ouvrez le dossier dans VS Code avec l'extension PlatformIO.
2.  Configurez vos identifiants par défaut dans `include/config.h` ou via le portail captif.
3.  Cliquez sur **Build** puis **Upload**.

### Actions Bouton
*   **Appui Long (3s)** : Déclencher / Annuler une alerte SOS.
*   **Triple Clic** : Démarrer le portail captif de configuration.

### États LED (NeoPixel)
*   🟢 **Vert Fixe** : Système prêt, connecté au WiFi/MQTT.
*   🔵 **Bleu Pulsant** : Recherche GPS en cours.
*   🔴 **Rouge Fixe** : Alerte SOS en cours (Mode ACTION).
*   🟣 **Violet Pulsant** : Mise à jour OTA ou Observation post-OTA.

---

## 📊 Format des Données (JSON)

### Exemple d'Alerte SOS
```json
{
  "alert_id": "ALT-ESP32-S3-001",
  "priority": "CRITICAL",
  "location": {
    "latitude": 36.8065,
    "longitude": 10.1815,
    "source": "GPS"
  },
  "user_profile": {
    "full_name": "Fatma Ben Ali",
    "medical_conditions": ["hypertension", "diabetes_type_2"]
  },
  "device_status": {
    "battery_level_pct": 85,
    "connectivity_type": "WIFI"
  }
}
```

---

## 🤵 Auteur
**Helmi** - *Expert IoT & Systèmes Embarqués*
