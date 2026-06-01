# 🎙️ Enregistrement Audio Optimisé (ADPCM) sur ESP32

Ce module est une version avancée de l'enregistreur audio du projet, focalisée sur l'optimisation drastique de la consommation mémoire. Il permet de quadrupler la durée d'enregistrement sur le système de fichiers interne **SPIFFS** par rapport au format PCM standard.

---

## 🔬 Spécifications Techniques

### 1. Compression ADPCM (MS-ADPCM)
Contrairement au format WAV/PCM classique (16 bits par échantillon), ce module utilise l'algorithme **ADPCM** (Adaptive Differential Pulse Code Modulation).
*   **Facteur de compression** : ~4:1.
*   **Impact** : Un fichier qui pèserait 1 Mo en PCM ne pèse plus que 256 Ko en ADPCM.
*   **Qualité** : Maintient une fidélité vocale excellente, idéale pour les applications de sécurité ou de monitoring.

### 2. Chaîne de Capture (I2S)
*   **Interface** : Protocole **I2S** (Inter-IC Sound).
*   **Configuration** : 8000 Hz (Sample Rate), 16-bit (Quantification initiale), Mono.
*   **Microphone** : Testé avec **INMP441** (Micro MEMS numérique).
    *   Pins : WS (25), SD (32), SCK (33).

### 3. Architecture Logicielle
*   **Multitâche FreeRTOS** : L'acquisition audio et l'encodage ADPCM sont délégués à une tâche dédiée s'exécutant sur le Core 1 de l'ESP32, tandis que le serveur Web tourne sur le Core 0.
*   **Data Scaling** : Mise à l'échelle dynamique des données 12-bit/24-bit du micro vers le format 16-bit attendu par l'encodeur.

---

## 🌐 Interface de Gestion
Le module héberge un serveur web asynchrone qui permet de :
*   Visualiser l'espace libre sur la partition **SPIFFS**.
*   Démarrer/Arrêter l'enregistrement en temps réel.
*   Suivre la progression via une barre d'état.
*   Télécharger le fichier `.adpcm` résultant.

## 🛠️ Installation & Dépendances
Ce projet nécessite les bibliothèques suivantes via PlatformIO :
*   `Arduino-Audio-Tools` : Pour la manipulation des flux.
*   `arduino-libg7xx` : Pour l'implémentation des codecs ADPCM.
*   `ArduinoJson` : Pour les échanges d'état entre le frontend et l'ESP32.

---

## 🤵 Auteur
**Helmi** - *Expert IoT & Systèmes Embarqués*
