# ESP-S3-ALERT

> Système d'alerte et de géolocalisation basé sur l'ESP32-S3 et le module SIM7670G (LTE/GNSS).

Ce projet fait partie d'un **Projet de Fin d'Études (PFE)** visant à fournir une solution robuste pour le suivi GPS et l'envoi d'alertes via le réseau 4G LTE. Il intègre également la surveillance de l'état de la batterie et le stockage local des données sur carte SD.

## Table des matières
- [Fonctionnalités](#fonctionnalités)
- [Matériel requis](#matériel-requis)
- [Installation](#installation)
- [Configuration](#configuration)
- [Structure du projet](#structure-du-projet)
- [Auteur](#auteur)
- [Licence](#licence)

---

## Fonctionnalités

- 📡 **Géolocalisation GNSS** : Acquisition des coordonnées GPS via le module SIM7670G.
- 📶 **Connectivité LTE** : Envoi d'alertes au format JSON vers un serveur distant via HTTP POST.
- 🔋 **Gestion de Batterie** : Lecture précise du pourcentage de batterie via le capteur I2C MAX17048.
- 💾 **Stockage SD** : Journalisation des positions GPS dans un fichier CSV (`gps_log.csv`) sur carte SD (SD_MMC).
- 🧹 **Maintenance automatique** : Suppression périodique (toutes les 5 minutes) des anciens logs pour optimiser l'espace.

---

## Matériel requis

- **MCU** : ESP32-S3 (DevKitC-1)
- **Module GSM/GNSS** : Waveshare SIM7670G
- **Batterie** : LiPo avec capteur de jauge MAX17048
- **Stockage** : Carte MicroSD

### Brochage (Pinout)

| Composant | ESP32-S3 Pin | Fonction |
|---|---|---|
| **SIM7670G** | GPIO 17 (RX) | UART RX |
| **SIM7670G** | GPIO 18 (TX) | UART TX |
| **SIM7670G** | GPIO 21 | PWRKEY (Power On) |
| **MAX17048** | GPIO 15 (SDA) | I2C Data |
| **MAX17048** | GPIO 16 (SCL) | I2C Clock |
| **MicroSD** | GPIO 4, 5, 6 | SD_MMC (1-bit mode) |

---

## Installation

Ce projet utilise **PlatformIO** sous VS Code.

1. Clonez ce dépôt.
2. Ouvrez le dossier du projet dans VS Code avec l'extension PlatformIO installée.
3. Les bibliothèques suivantes seront automatiquement installées via le fichier `platformio.ini` :
   - `EspSoftwareSerial`
   - `TinyGPSPlus`
   - `PubSubClient`
   - `ArduinoJson`

---

## Configuration

Le fichier `platformio.ini` est configuré pour une ESP32-S3 avec 8MB de PSRAM OPI et 16MB de Flash.

```ini
board_build.flash_mode = qio
board_build.psram_type = opi
board_build.arduino.memory_type = qio_opi
board_upload.flash_size = 16MB
```

Dans `src/lte.cpp`, assurez-vous de configurer correctement l'APN de votre opérateur (par défaut : `internet.ooredoo.tn`).

---

## Structure du projet

```
ESP-S3-ALERT/
├── include/            # En-têtes (.h)
├── src/                # Code source (.cpp)
│   ├── main.cpp        # Logique principale (setup/loop)
│   ├── lte.cpp         # Gestion 4G et HTTP
│   ├── gps.cpp         # Parsing et logging GNSS
│   ├── battery.cpp     # Lecture I2C de la jauge
│   └── serial_comm.cpp # Communication série AT
├── lib/                # Bibliothèques spécifiques
├── platformio.ini      # Configuration du projet
└── README.md           # Documentation
```

---

## Auteur

- **Helmi** - *Projet PFE*

---

## Licence

Ce projet est sous licence MIT - voir le fichier [LICENSE](LICENSE) pour plus de détails.
