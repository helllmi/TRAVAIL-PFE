# ESP32 Audio Broadcasting RTSP Server

> Diffusion audio en temps réel depuis un microphone INMP441 via un serveur RTSP sur ESP32.

Ce projet permet de transformer un ESP32 en un serveur de streaming audio sans fil. Il capture le son via un microphone I2S (INMP441) et le diffuse sur le réseau local en utilisant le protocole RTSP, permettant une écoute facile via des lecteurs standards comme VLC.

## Table des matières
- [Fonctionnalités](#fonctionnalités)
- [Matériel requis](#matériel-requis)
- [Installation](#installation)
- [Configuration](#configuration)
- [Utilisation](#utilisation)
- [Structure du projet](#structure-du-projet)
- [Bibliothèques utilisées](#bibliothèques-utilisées)
- [Auteur](#auteur)
- [Licence](#licence)

## Fonctionnalités
- **Streaming Temps Réel** : Capture et diffusion audio avec une latence minimale.
- **Protocole RTSP** : Compatible avec la majorité des lecteurs multimédias réseau.
- **Buffer Circulaire Thread-safe** : Utilisation de FreeRTOS pour assurer une synchronisation parfaite entre la capture I2S (Core 1) et le service réseau.
- **Haute Qualité** : Audio PCM 16 bits, 16000 Hz, Mono.

## Matériel requis
- **Microcontrôleur** : ESP32 (DevKit V1, S3, etc.)
- **Microphone** : INMP441 (Microphone MEMS I2S)
- **Câblage** :
    - VDD -> 3.3V
    - GND -> GND
    - L/R -> GND (pour canal Gauche)
    - SD -> GPIO 32
    - WS -> GPIO 25
    - SCK -> GPIO 33

## Installation
1.  **Prérequis** : Avoir [PlatformIO](https://platformio.org/) installé (VS Code plugin recommandé).
2.  **Clonage** :
    ```bash
    git clone <url-du-depot>
    cd "audio broadcasting with rtsp server"
    ```
3.  **Dépendances** : Les bibliothèques sont gérées automatiquement par PlatformIO via le fichier `platformio.ini`.

## Configuration
Avant de téléverser, modifiez les identifiants WiFi dans `src/main.cpp` :

```cpp
const char* ssid     = "VOTRE_SSID";
const char* password = "VOTRE_MOT_DE_PASSE";
```

Vous pouvez également modifier les pins I2S si nécessaire :
```cpp
#define I2S_SD   32
#define I2S_WS   25
#define I2S_SCK  33
```

## Utilisation
1.  Compilez et téléversez le code sur votre ESP32.
2.  Ouvrez le moniteur série (baud rate 115200) pour obtenir l'adresse IP de l'appareil.
3.  **Écoute avec VLC** :
    - Ouvrez VLC.
    - Allez dans `Média` -> `Ouvrir un flux réseau`.
    - Entrez l'URL : `rtsp://<IP_DE_L_ESP32>`
4.  **Écoute avec ffplay** :
    ```bash
    ffplay rtsp://<IP_DE_L_ESP32>
    ```

## Structure du projet
```text
.
├── lib/               # Bibliothèques locales
├── src/
│   └── main.cpp       # Code source principal (logique I2S et RTSP)
├── platformio.ini     # Configuration PlatformIO et dépendances
└── README.md          # Documentation
```

## Bibliothèques utilisées
- [Micro-RTSP-Audio](https://github.com/pschatzmann/Micro-RTSP-Audio) : Moteur du serveur RTSP audio.
- [arduino-audio-tools](https://github.com/pschatzmann/arduino-audio-tools) : Utilitaires de traitement audio.
- [arduino-libg7xx](https://github.com/pschatzmann/arduino-libg7xx) : Codecs audio.

## Auteur
Projet développé dans le cadre d'un PFE (Projet de Fin d'Études).

## Licence
Ce projet est sous licence MIT.
