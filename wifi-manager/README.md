# WiFi Manager (ESP32)

> Gestionnaire de configuration WiFi avec portail captif pour ESP32.

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
- **Mode Point d'Accès (AP)** : Crée son propre réseau WiFi si aucune configuration n'est trouvée ou si la connexion échoue.
- **Portail Captif** : Serveur web asynchrone permettant de configurer le SSID, le mot de passe et l'IP statique (optionnel).
- **Stockage Persistant** : Utilise **LittleFS** pour sauvegarder les identifiants WiFi sur la mémoire flash de l'ESP32.
- **Interface Web** : Formulaire HTML simple pour la saisie des données.

## Installation
Projet basé sur **PlatformIO**.

1. Installer l'extension PlatformIO dans VS Code.
2. Ouvrir le dossier `wifi-manager`.
3. Assurez-vous d'avoir installé les outils de téléchargement de système de fichiers LittleFS pour PlatformIO.

## Utilisation
1. Téléversez le firmware : `PlatformIO: Upload`.
2. **Important** : Téléversez également l'image du système de fichiers (dossier `Data`) : `PlatformIO: Upload Filesystem Image`.
3. Si l'ESP32 ne trouve pas de WiFi, il crée un point d'accès. Connectez-vous à ce réseau pour accéder à l'interface de configuration.

## Configuration
Le point d'accès par défaut est généralement nommé "ESP-WIFI-MANAGER". Vous pouvez modifier ce nom dans `src/wifi_manager.h` ou `src/main.cpp`.

## Structure du projet
- `src/main.cpp` : Initialisation du serveur web et gestion du WiFi.
- `src/wifi_manager.cpp/h` : Logique de gestion de la connexion WiFi.
- `src/littlefs_manager.cpp/h` : Gestion de la lecture/écriture sur LittleFS.
- `Data/` : Contient les fichiers HTML/CSS du portail captif.
- `platformio.ini` : Configuration PlatformIO (ESP32, ESPAsyncWebServer).

## Développement
Bibliothèques utilisées :
- **ESPAsyncWebServer** : Serveur web performant.
- **AsyncTCP** : Dépendance pour le serveur web asynchrone.
- **LittleFS** : Système de fichiers moderne pour ESP32.

## Auteur
[Helmi]

## Licence
MIT
