# 📡 TEST GPS (Architecture MQTT)

Ce module est dédié à l'expérimentation de la transmission de données géospatiales via le protocole **MQTT** (Message Queuing Telemetry Transport), optimisé pour les réseaux à faible bande passante et haute latence.

## 🏗️ Structure du Module

### 📂 `GPS-TRACKER` (Firmware ESP32)
*   **Fonction** : Acquisition des trames NMEA et publication JSON.
*   **Stack** : PlatformIO, TinyGPS++, PubSubClient, ArduinoJson.
*   **Cycle de vie** :
    1.  Connexion WiFi sécurisée.
    2.  Synchronisation avec le broker MQTT.
    3.  Lecture du module GPS via **SoftwareSerial** (Pins 16 RX / 17 TX).
    4.  Publication sur le topic `esp32/gps` à intervalle régulier.

### 📂 `GPS_TRACKER` (Backend Node.js)
*   **Fonction** : Broker/Subscriber et interface de visualisation.
*   **Stack** : Node.js, MQTT.js, Socket.io, Express.
*   **Action** : Récupère les messages du broker, les parse, et les "push" vers le frontend `GPS.html` en temps réel via WebSockets.

---

## 🛠️ Configuration
*   **MQTT Broker** : Configuré par défaut sur l'IP `10.145.2.184`.
*   **Topic** : `esp32/gps`.
*   **Port Serveur** : 3000.

---

## 🤵 Auteur
**Helmi** - *Ingénieur IoT*
