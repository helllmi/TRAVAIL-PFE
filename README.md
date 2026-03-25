# Projet de Fin d'Études (PFE) - GPS Tracking & Monitoring

> Ce dépôt regroupe l'ensemble des modules développés pour un système complet de suivi GPS en temps réel.

## Table des matières
- [Présentation](#présentation)
- [Structure du dépôt](#structure-du-dépôt)
- [Modules principaux](#modules-principaux)
- [Technologies utilisées](#technologies-utilisées)
- [Auteur](#auteur)

## Présentation
Ce projet vise à concevoir un système de tracking GPS utilisant des modules ESP32 (avec NEO-7M) pour l'acquisition de données, et des serveurs Node.js pour la visualisation en temps réel. Deux approches de communication sont explorées : HTTP direct et MQTT.

## Structure du dépôt
```text
PFE/
├── GPS-monitor/         # Système de monitoring standard
│   ├── gps-backend/     # Serveur Node.js (HTTP + Socket.io)
│   └── Monitoring GPS/  # Firmware ESP32 (HTTP)
├── TEST GPS/            # Versions de test basées sur MQTT
│   ├── GPS_TRACKER/     # Serveur Node.js (MQTT + Socket.io)
│   └── GPS-TRACKER/     # Firmware ESP32 (MQTT)
└── wifi-manager/        # Outil de configuration WiFi pour ESP32
```

## Modules principaux

### 1. GPS-monitor (HTTP)
Utilise des requêtes HTTP POST pour envoyer les données de l'ESP32 au serveur backend. Idéal pour des architectures simples sans broker intermédiaire.

### 2. TEST GPS (MQTT)
Exploite le protocole MQTT pour une communication légère et efficace. Requiert un broker MQTT (ex: Mosquitto). Plus performant pour de multiples objets connectés.

### 3. WiFi Manager
Un portail captif autonome permettant de configurer les identifiants WiFi d'un ESP32 sans avoir à re-compiler le code.

## Technologies utilisées
- **Hardware** : ESP32, Module GPS NEO-7M.
- **Embedded** : C++, Framework Arduino, PlatformIO, TinyGPS++, PubSubClient, ESPAsyncWebServer, LittleFS.
- **Backend** : Node.js, Express, Socket.IO, MQTT.js, fs-extra.
- **Frontend** : HTML/CSS/JavaScript.

## Auteur
[Helmi]
