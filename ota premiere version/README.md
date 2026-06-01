# 🔄 OTA Infrastructure : Système de Mise à Jour à Distance

Ce module implémente une solution complète de gestion du cycle de vie des firmwares pour une flotte d'**ESP32**, permettant le déploiement de correctifs et de fonctionnalités sans intervention physique.

## 🏗️ Architecture du Système

Le système repose sur un triptyque :
1.  **Serveur OTA (Edge/Cloud)** : Un service Flask (Python) qui héberge les binaires et gère le versioning.
2.  **Broker MQTT** : Le canal de notification permettant de "pousser" les mises à jour vers les équipements.
3.  **Firmware Client** : Une implémentation robuste sur ESP32 capable de télécharger, vérifier et auto-réparer (rollback) le système.

---

## 📂 Contenu du Dossier

### 📂 `Serveur-ota`
Serveur de gestion des firmwares écrit en **Python/Flask**.
*   **Fonctions** :
    *   Dashboard web pour l'upload des fichiers `.bin`.
    *   Calcul automatique du hash **SHA256** pour garantir l'intégrité des transferts.
    *   API JSON exposant la dernière version disponible (`/firmware/version`).
*   **Lancement** :
    ```bash
    python Serveur-Ota.py
    ```

### 📂 `firmware-v1-mqtt` & `firmware-v2-mqtt`
Firmwares de référence (initial et mise à jour) intégrant la logique de mise à jour.
*   **v1.0.0** : Signature visuelle (LED clignote à 1Hz). Version stable de base.
*   **v2.0.0** : Signature visuelle (LED clignote à 5Hz). Utilisée pour valider le processus de migration.

---

## 🛡️ Mécanismes de Fiabilité (Critical Features)

Pour éviter de "bricker" (rendre inutilisable) les équipements en cas de mise à jour corrompue, les fonctionnalités suivantes sont implémentées :

1.  **Vérification d'Intégrité SHA256** : Avant tout flashage, le firmware calcule le hash du binaire téléchargé en streaming et le compare à celui fourni par le serveur via MQTT.
2.  **Double Partitionnement (A/B)** : L'ESP32 bascule entre deux partitions d'application. Si la nouvelle partition échoue, le système peut revenir à la précédente.
3.  **Watchdog Applicatif (Observation Window)** : Après une mise à jour, l'équipement entre dans une phase d'observation (ex: 5 minutes). S'il ne parvient pas à se reconnecter au WiFi et au MQTT après 3 tentatives consécutives, un **Rollback matériel** automatique est déclenché.
4.  **Hardware Watchdog (WDT)** : Protection contre le gel du processeur pendant le téléchargement.

---

## 🚀 Utilisation

1.  Lancer le **Serveur-ota**.
2.  Compiler et flasher le **firmware-v1-mqtt** sur l'ESP32.
3.  Uploader le binaire du **firmware-v2-mqtt** sur le serveur via l'interface web.
4.  Le serveur notifie l'équipement via MQTT, qui télécharge et installe automatiquement la nouvelle version.
