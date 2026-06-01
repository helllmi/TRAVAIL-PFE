# 🔊 Traitement Audio Avancé : Data Scaling & Amplification Logicielle

Ce module de recherche se concentre sur l'optimisation de la qualité du signal capturé par les microphones MEMS numériques (**I2S**) sur **ESP32-S3**. Il traite spécifiquement la problématique de la dynamique du signal et du rapport signal/bruit.

---

## 🎯 Objectifs Techniques

### 1. Full 16-bit Data Scaling
La plupart des microphones I2S (comme l'INMP441) transmettent les données sur 24 bits, mais l'information utile se trouve souvent sur les 12 ou 18 bits de poids fort. 
Ce module implémente une routine de **Mise à l'échelle (Scaling)** précise pour :
*   Convertir les trames I2S brutes vers un format **PCM 16-bit** pur.
*   Assurer que l'intégralité de la plage dynamique du convertisseur ADC est utilisée (**Full Scale**).

### 2. Amplification Numérique (Digital Gain)
Afin de compenser la faible sensibilité de certains micros ou l'éloignement de la source sonore, un étage d'amplification logicielle a été ajouté :
*   **Multiplication de gain** : Application d'un facteur multiplicateur (x4, x8) directement sur les échantillons PCM.
*   **Saturation Protection (Clamping)** : Algorithme de protection pour éviter l'écrêtage (clipping) numérique, garantissant que les valeurs ne dépassent pas les limites du format 16-bit (-32768 à 32767).

---

## 🔌 Configuration Matérielle (ESP32-S3)

| Signal | Pin ESP32-S3 | Description |
| :--- | :--- | :--- |
| **WS** | GPIO 11 | Word Select (LRCK) |
| **SD** | GPIO 12 | Serial Data (DIN) |
| **SCK** | GPIO 10 | Serial Clock (BCLK) |

---

## 📂 Fonctionnalités Implémentées
*   **Enregistrement WAV** : Stockage direct sur partition **SPIFFS**.
*   **Visualisation Peak** : Affichage dans le moniteur série du niveau de crête (**Peak**) en temps réel pour calibrer le gain.
*   **Web Interface "Cyberpunk"** : Une interface moderne permettant de contrôler l'enregistrement et de télécharger le fichier audio final.

---

## 🚀 Utilisation
1. Ouvrir le projet sous **PlatformIO**.
2. Ajuster le facteur de gain dans `src/main.cpp` (fonction `i2s_adc_data_scale`).
3. Compiler et téléverser.
4. Surveiller le moniteur série pour valider que le `PEAK` ne sature pas (valeur proche de 32767).
