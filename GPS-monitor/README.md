# 🛰️ GPS Monitor (Architecture HTTP/REST)

Ce module implémente une solution de suivi GPS basée sur des échanges standards **HTTP POST**, idéale pour une intégration directe avec des serveurs web sans nécessiter de broker de messages intermédiaire.

## 🏗️ Structure du Module

### 📂 `Monitoring GPS` (Firmware ESP32)
*   **Description** : Firmware robuste pour l'acquisition et la transmission.
*   **Technique** : Utilise `HTTPClient` pour envoyer des payloads JSON structurés contenant : Latitude, Longitude, Altitude, Vitesse et nombre de Satellites.
*   **Hardware** : ESP32 + NEO-7M.

### 📂 `gps-backend` (Serveur Node.js)
*   **Description** : Backend de gestion et de stockage.
*   **Persistance** : Sauvegarde de l'historique complet dans `data/locations.json` et de la dernière position connue dans `data/last_position.json`.
*   **Real-time** : Intégration de **Socket.io** pour notifier les clients web de toute nouvelle position reçue.

---

## 🚀 Déploiement
1.  **Backend** :
    ```bash
    cd gps-backend
    npm install
    node server.js
    ```
2.  **Firmware** : Configurer l'IP du serveur dans `src/main.cpp` et téléverser via PlatformIO.

---

## 🤵 Auteur
**Helmi** - *Ingénieur IoT*
