# ESP32 Audio Recorder

> Un enregistreur audio haute performance basé sur l'ESP32, capable de capturer du son via un microphone I2S, de le stocker au format WAV sur la mémoire interne (SPIFFS) et de le servir via une interface web moderne.

## Table des matières
- [Fonctionnalités](#fonctionnalités)
- [Matériel requis](#matériel-requis)
- [Installation](#installation)
- [Configuration](#configuration)
- [Utilisation](#utilisation)
- [Structure du projet](#structure-du-projet)
- [Spécifications techniques](#spécifications-techniques)
- [Auteur](#auteur)
- [Licence](#licence)

## Fonctionnalités
- 🎙️ **Enregistrement I2S** : Capture audio numérique de haute qualité (testé avec INMP441).
- 💾 **Stockage Interne** : Sauvegarde directe sur le système de fichiers SPIFFS de l'ESP32.
- 🌐 **Interface Web "Cyberpunk"** : Interface responsive pour contrôler l'enregistrement et suivre la progression en temps réel.
- 📥 **Téléchargement Direct** : Récupération du fichier `.wav` directement depuis le navigateur.
- ⚙️ **Multitâche** : Utilisation de FreeRTOS pour gérer l'enregistrement en arrière-plan sans bloquer le serveur web.

## Matériel requis
- **Microcontrôleur** : ESP32 (DevKit V1 ou similaire).
- **Microphone I2S** : INMP441 ou équivalent.
- **Câblage** :
  | INMP441 | ESP32 | Description |
  | :--- | :--- | :--- |
  | **VDD** | 3.3V | Alimentation |
  | **GND** | GND | Masse |
  | **L/R** | GND | Canal Gauche (Mono) |
  | **WS** | GPIO 25 | Word Select |
  | **SCK** | GPIO 33 | Serial Clock |
  | **SD** | GPIO 32 | Serial Data |

## Installation
1. Installez l'**Arduino IDE** et le support des cartes **ESP32**.
2. Assurez-vous d'avoir les bibliothèques standards incluses (déjà présentes dans le core ESP32) :
   - `WiFi`
   - `WebServer`
   - `SPIFFS`
3. Ouvrez le fichier `AUDIO-RECORD.ino` dans l'Arduino IDE.
4. Sélectionnez votre carte ESP32 dans `Outils > Type de carte`.
5. Compilez et téléversez le code.

## Configuration
Avant de téléverser, modifiez les identifiants WiFi dans le code source :

```cpp
const char* ssid     = "VOTRE_SSID";
const char* password = "VOTRE_MOT_DE_PASSE";
```

Vous pouvez également ajuster les paramètres d'enregistrement :
- `RECORD_TIME` : Durée de l'enregistrement en secondes (par défaut 10s).
- `I2S_SAMPLE_RATE` : Fréquence d'échantillonnage (8000 Hz par défaut pour économiser l'espace SPIFFS).

## Utilisation
1. Ouvrez le **Moniteur Série** (115200 baud) pour obtenir l'adresse IP de l'ESP32.
2. Connectez votre ordinateur/smartphone au même réseau WiFi que l'ESP32.
3. Accédez à l'URL affichée (ex: `http://192.168.1.50`).
4. Cliquez sur **"DÉMARRER"** pour lancer l'enregistrement.
5. Une fois terminé, cliquez sur **"TÉLÉCHARGER WAV"** pour récupérer votre fichier audio.

## Structure du projet
```text
AUDIO-RECORD/
└── AUDIO-RECORD.ino    # Code source principal (Serveur Web + I2S + SPIFFS)
```

## Spécifications techniques
- **Format** : WAV (PCM 16 bits)
- **Fréquence** : 8 kHz
- **Canaux** : Mono
- **Stockage** : SPIFFS (partition interne de l'ESP32)

## Auteur
Projet réalisé dans le cadre d'un PFE (Projet de Fin d'Études).

## Licence
Ce projet est sous licence MIT.
