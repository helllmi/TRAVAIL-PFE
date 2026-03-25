# Monitoring GPS (ESP32)

> Programme ESP32 pour l'acquisition de données GPS (NEO-7M) et l'envoi vers un serveur backend.

## Table des matières
- [Fonctionnalités](#fonctionnalités)
- [Installation](#installation)
- [Utilisation](#utilisation)
- [Configuration](#configuration)
- [Structure du projet](#structure-du-projet)
- [Développement](#développement)
- [Auteur](#auteur)
- [Licence](#licence)

## Fonctionnalités
- **Acquisition GPS** : Lecture des données NMEA à partir d'un module GPS via SoftwareSerial (Pins 16 RX / 17 TX).
- **Communication WiFi** : Connexion automatique au réseau WiFi spécifié.
- **Transmission HTTP** : Envoi des données de localisation (latitude, longitude, altitude, vitesse, satellites) vers un serveur via des requêtes HTTP POST en JSON.
- **Diagnostic** : Vérification de la connectivité GPS et affichage des statistiques sur le moniteur série.

## Installation
Ce projet utilise **PlatformIO**.

1. Installer l'extension PlatformIO dans VS Code.
2. Ouvrir le dossier `Monitoring GPS`.
3. Les dépendances sont gérées automatiquement via `platformio.ini`.

## Utilisation
1. Connecter votre ESP32 à votre ordinateur.
2. Compiler et téléverser le code : `PlatformIO: Upload`.
3. Ouvrir le moniteur série à `115200` baud pour suivre l'état du système.

## Configuration
Dans `src/main.cpp` :
- `ssid` et `password` : Identifiants de votre réseau WiFi.
- `http_server` : L'adresse IP et le port de votre serveur `gps-backend`.

## Structure du projet
- `src/main.cpp` : Logique principale du firmware.
- `platformio.ini` : Configuration de l'environnement, plateforme et bibliothèques.
- `lib/` : Bibliothèques personnalisées (le cas échéant).

## Développement
Le projet utilise les bibliothèques :
- **TinyGPS++** : Pour le décodage facile des trames NMEA.
- **EspSoftwareSerial** : Pour la communication série avec le module GPS.
- **ArduinoJson** : Pour la sérialisation des données envoyées au serveur.

## Auteur
[Helmi]

## Licence
MIT
