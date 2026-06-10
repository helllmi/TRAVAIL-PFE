# 📡 Documentation des topics MQTT — Firmware HEELPMEE

> **Device** : ESP32-S3-SIM7670G HEELPMEE wearable
> **Firmware version** : 1.2.0
> **Protocole** : MQTT 3.1.1 over TCP (port 1883) ou TLS (port 8883)
> **Transport** : LTE Cat-1 (primaire) ou WiFi (fallback)

---

## 🗺️ Vue d'ensemble

Tous les topics suivent un schéma hiérarchique commun :

```
devices/{device_id}/{canal}
```

- **`devices/`** : préfixe fixe pour tous les devices HEELPMEE
- **`{device_id}`** : identifiant unique du device (ex: `esp32-s3-test-01`)
- **`{canal}`** : type d'information (alert, telemetry, state, etc.)

Cette convention permet à un backend de **filtrer facilement** :
- `devices/+/alert` → toutes les alertes de tous les devices (`+` = wildcard 1 niveau)
- `devices/esp32-s3-test-01/#` → tout ce qui concerne ce device précis (`#` = wildcard multi-niveaux)

---

## 📋 Liste exhaustive des topics

| Topic | Direction | QoS | Retain | LWT | Usage |
|-------|-----------|-----|--------|-----|-------|
| `devices/{id}/alert` | Device → Backend | 1 | non | non | Alertes SOS en cours |
| `devices/{id}/telemetry` | Device → Backend | 0 | non | non | Télémétrie périodique |
| `devices/{id}/state` | Device → Backend | 0 | **oui** | non | État FSM du device |
| `devices/{id}/lwt` | Device → Backend | 1 | **oui** | **oui** | Last Will (détection offline) |
| `devices/{id}/ota/status` | Device → Backend | 1 | non | non | Statut de mise à jour OTA |
| `devices/{id}/ota/notify` | Backend → Device | 1 | non | non | Notification d'OTA disponible |

---

## 🚨 1. `devices/{id}/alert`

### Direction
Device → Backend

### QoS & Retain
- **QoS 1** (at-least-once) : garantit la livraison, même en cas de perte réseau
- **Retain : NON** : chaque alerte est unique, pas de "dernière valeur"

### Quand est-il publié ?
À chaque entrée en **STATE_ACTION** (déclenchement SOS via long press 3s) et **toutes les `ALERT_INTERVAL_MS` (10s)** tant que le device reste en ACTION.

### Mécanisme de bufferisation
Si la publication échoue (réseau down, broker injoignable, MQTT déconnecté), l'alerte est **automatiquement sauvegardée en flash** (LittleFS) et **rejouée** dès que la connexion est restaurée. Aucune alerte n'est jamais perdue.

### Format du payload (JSON, ~1158 octets)

```json
{
  "timestamp": "2026-06-01T22:15:27.000Z",
  "priority": "CRITICAL",
  "location": {
    "latitude": 36.806389,
    "longitude": 10.181667,
    "accuracy_meters": 5,
    "altitude_meters": 35,
    "speed_kmh": 0,
    "heading_degrees": null,
    "location_source": "GPS",
    "address_reverse_geocoded": "Tunisia"
  },
  "context": {
    "fall_detected": false,
    "heart_rate_bpm": 100,
    "ambient_noise_db": 70,
    "motion_state": "STATIONARY",
    "geofence_status": "UNKNOWN"
  },
  "metadata": {
    "schema_version": "1.0",
    "kafka_topic": "helpmee_alerts",
    "partition_key": "esp32-s3-test-01",
    "ttl_seconds": 86400,
    "retry_count": 0
  },
  "alert_id": "ALT-esp32-s3-test-01",
  "device_id": "esp32-s3-test-01",
  "user_id": "USR-001",
  "alert_type": "PANIC",
  "trigger_method": "BUTTON_PRESS",
  "press_count": 3,
  "press_duration_ms": 3245,
  "device_status": {
    "charging": false,
    "battery_level_pct": 100,
    "signal_strength_dbm": 20,
    "connectivity_type": "LTE",
    "firmware_version": "1.2.0",
    "is_charging": false,
    "last_health_check": 43
  },
  "user_profile": {
    "age": 78,
    "category": "ELDERLY",
    "full_name": "Fatma Ben Ali",
    "blood_type": "A+",
    "medical_conditions": ["hypertension", "diabetes_type_2"],
    "emergency_contacts": [
      {
        "name": "Ahmed Ben Ali",
        "relationship": "SON",
        "phone": "+216-55-123-456",
        "notify": true
      }
    ],
    "spoken_languages": ["ar", "fr"]
  }
}
```

### Spécificités
- **Plus gros payload** du firmware (~1158 octets) car il embarque le profil utilisateur complet
- **Self-contained** : le backend n'a pas besoin de requêter d'autres infos pour traiter l'alerte
- **`press_duration_ms`** : durée réelle de l'appui long (utile pour différencier les appuis intentionnels des accidentels)
- **`connectivity_type`** : indique le transport utilisé au moment de l'alerte (`"LTE"` ou `"WIFI"`)

---

## 📊 2. `devices/{id}/telemetry`

### Direction
Device → Backend

### QoS & Retain
- **QoS 0** (at-most-once) : envoyé "best effort", pas critique de le perdre
- **Retain : NON** : c'est un flux, pas un état

### Quand est-il publié ?
Périodiquement, à intervalle **adaptatif selon l'état FSM** :
- **STANDBY** : toutes les **10 minutes** (économie d'énergie)
- **ACTION** : toutes les **60 secondes** (suivi rapproché pendant l'alerte)

### Format du payload (JSON, ~207 octets)

```json
{
  "device_id": "esp32-s3-test-01",
  "timestamp": "2026-06-01T22:15:44.000Z",
  "battery_pct": 100,
  "signal_rssi": 20,
  "signal_ber": 0,
  "connectivity": "LTE",
  "firmware_version": "1.2.0",
  "uptime_seconds": 61,
  "gps_valid": false
}
```

### Spécificités
- **Léger** (~207 octets) pour minimiser le coût data en LTE
- **`signal_rssi`** : valeur CSQ brute (0-31). Conversion en dBm = `-113 + 2 × rssi`
- **`gps_valid`** : true si un fix GPS a été obtenu récemment
- Utile pour **dashboard de monitoring** : courbes batterie, signal, etc.

---

## 🏷️ 3. `devices/{id}/state`

### Direction
Device → Backend

### QoS & Retain
- **QoS 0**
- **Retain : OUI** ⭐

### À quoi sert le retain ?
Quand un client (ex: dashboard, autre device) **s'abonne** à ce topic, il reçoit **immédiatement** le dernier état publié, sans avoir à attendre la prochaine publication. C'est utile pour :
- Un dashboard qui démarre : il voit tout de suite l'état actuel du device
- Une reconnexion : pas besoin d'attendre 10 minutes pour savoir si le device est en ACTION ou STANDBY
- Découvrir l'état d'un device qu'on n'a jamais vu publier

### Quand est-il publié ?
- **À chaque transition d'état FSM** (STANDBY → ACTION ou vice versa) → publication immédiate
- **Heartbeat périodique** :
  - STANDBY : toutes les 10 min
  - ACTION : toutes les 60 s

### Format du payload (JSON, ~214 octets)

```json
{
  "device_id": "esp32-s3-test-01",
  "state": "STANDBY",
  "timestamp": "2026-06-01T22:15:45.000Z",
  "battery_pct": 100,
  "uptime_seconds": 61,
  "state_uptime_sec": 18,
  "firmware_version": "1.2.0",
  "connectivity": "LTE",
  "gps_valid": false
}
```

### Spécificités
- **`state`** : valeur possible `"STANDBY"`, `"ACTION"`, ou `"OFF"`
- **`state_uptime_sec`** : durée depuis la dernière transition (utile pour détecter un device "coincé" en ACTION trop longtemps)
- **Retain = oui** : permet au backend de reconstruire l'état du parc même au démarrage

---

## 💀 4. `devices/{id}/lwt` (Last Will and Testament)

### Direction
Device → Backend (publié automatiquement par le broker en cas de déconnexion brutale)

### QoS & Retain
- **QoS 1**
- **Retain : OUI**

### Qu'est-ce que le LWT ?
Le **Last Will and Testament** est une fonctionnalité native de MQTT. Le device dit au broker au moment de la connexion :
> "Si je me déconnecte sans dire au revoir (panne, plantage, perte réseau), publie ce message à ma place sur ce topic."

C'est le seul moyen de détecter qu'un device IoT est **vraiment hors ligne** (pas juste silencieux).

### Format du payload

**À la connexion (envoyé par le device)** :
```json
{
  "device_id": "esp32-s3-test-01",
  "status": "online",
  "timestamp": "2026-06-01T22:15:00.000Z"
}
```

**Au LWT (envoyé par le broker quand le device disparaît)** :
```json
{
  "device_id": "esp32-s3-test-01",
  "status": "offline",
  "reason": "connection_lost"
}
```

### Spécificités
- **Pré-configuré** dans `mqttBegin()` lors de la connexion au broker
- **Le broker gère tout** : le device n'a rien à faire en cas de plantage
- **Retain = oui** : le statut reste visible pour tout nouveau client qui s'abonne
- Permet au dashboard d'afficher un indicateur 🟢 online / 🔴 offline en temps réel

⚠️ **Note** : le LWT peut ne pas être déclenché si le device ferme la connexion proprement (ex: reboot via portail). Dans ce cas, le device publie lui-même `status:offline` avant de redémarrer.

---

## 📦 5. `devices/{id}/ota/status`

### Direction
Device → Backend

### QoS & Retain
- **QoS 1** (livraison garantie)
- **Retain : NON**

### Quand est-il publié ?
À chaque **étape clé** du processus OTA (Over-The-Air update) :
- Notification reçue
- Téléchargement démarré
- Téléchargement en cours (% de progression)
- Vérification SHA256
- Flash en cours
- Reboot
- Self-test post-OTA
- Validation finale (ou rollback)

### Format du payload (JSON)

```json
{
  "device_id": "esp32-s3-test-01",
  "ota_id": "deployment-12345",
  "stage": "downloading",
  "progress_pct": 47,
  "from_version": "1.1.0",
  "to_version": "1.2.0",
  "bytes_downloaded": 458752,
  "bytes_total": 945632,
  "timestamp": "2026-06-01T22:30:15.000Z"
}
```

### Valeurs possibles de `stage`
- `notified` : notification OTA reçue, traitement démarré
- `downloading` : téléchargement du `.bin` en cours
- `verifying` : vérification SHA256
- `flashing` : écriture sur la partition de secours
- `rebooting` : redémarrage imminent
- `validating` : self-test post-OTA (60s)
- `validated` : OTA validée, nouvelle version active
- `failed` : échec à une étape (avec `reason`)
- `rollback` : retour à l'ancienne version après échec

### Spécificités
- Permet au **backend de suivre en temps réel** une campagne d'OTA sur tout le parc
- Utile pour les **statistiques de déploiement** : combien de devices ont reçu, combien ont validé, combien ont rollback
- Le backend peut détecter un device **bloqué** dans une étape (ex: 5 min en `downloading` = problème)

---

## 📨 6. `devices/{id}/ota/notify`

### Direction
Backend → Device ⬅️ (seul topic descendant !)

### QoS & Retain
- **QoS 1**
- **Retain : NON**

### Quand est-il publié ?
Quand le backend veut **déclencher une mise à jour** sur un device précis.

### Format du payload (JSON)

```json
{
  "version": "1.2.1",
  "url": "https://ota.heelpmee.com/firmware/v1.2.1.bin",
  "sha256": "a3b2c1d4e5f6...",
  "size": 945632,
  "deployment_id": "deployment-12345",
  "force": false,
  "rollback_allowed": true
}
```

### Spécificités
- **Le device s'abonne** à ce topic automatiquement à la connexion
- **Vérification SHA256 obligatoire** côté device avant flash (sécurité)
- **`force = true`** : OTA même si batterie < 30% (déconseillé)
- **`rollback_allowed = true`** : si self-test échoue → retour à l'ancienne version automatiquement

### Sécurité
- L'URL doit être en **HTTPS** pour éviter l'interception
- Le SHA256 doit être **calculé et signé** côté backend (idéalement avec une clé privée)
- Le device peut vérifier une **signature** en plus du SHA256 pour s'assurer que le firmware vient bien du backend autorisé

---

## 🔄 Cycle de vie d'une session typique

Voici la séquence des publications/abonnements lors d'une session normale :

### 1. Connexion du device
```
device → broker : CONNECT
   ↓
   (avec LWT préconfiguré sur devices/{id}/lwt = "offline")
   ↓
device → broker : SUBSCRIBE devices/{id}/ota/notify
device → broker : PUBLISH devices/{id}/lwt = "online" (retain)
device → broker : PUBLISH devices/{id}/state = "STANDBY" (retain)
```

### 2. Vie normale en STANDBY
```
toutes les 10 min :
  device → broker : PUBLISH devices/{id}/telemetry
  device → broker : PUBLISH devices/{id}/state = "STANDBY"
```

### 3. Déclenchement d'une alerte SOS
```
[Long press 3s sur le bouton]

device → broker : PUBLISH devices/{id}/state = "ACTION" (retain, immédiat)
device → broker : PUBLISH devices/{id}/alert (toutes les 10s)
[en parallèle : audio enregistré sur SD]

toutes les 60s :
  device → broker : PUBLISH devices/{id}/telemetry
  device → broker : PUBLISH devices/{id}/state = "ACTION"
```

### 4. Reset (sortie d'ACTION)
```
[Long press 3s ou triple clic]

device → broker : PUBLISH devices/{id}/state = "STANDBY" (retain, immédiat)
[audio stoppé, header WAV finalisé]
```

### 5. Coupure réseau
```
[Perte de couverture LTE]

modem → device : +CMQTTCONNLOST
device → mémoire flash : enqueue alerts (LittleFS buffer)
broker → autres clients : LWT déclenché → devices/{id}/lwt = "offline"
```

### 6. Retour réseau
```
[Couverture revient]

device → broker : CONNECT (reconnexion)
device → broker : PUBLISH devices/{id}/lwt = "online"
device → broker : flush du buffer offline (1 alerte/seconde)
```

---

## 🧪 Tests rapides côté backend

### S'abonner à tout pour un device
```bash
mosquitto_sub -h broker.hivemq.com -t "devices/esp32-s3-test-01/#" -v
```

### S'abonner à toutes les alertes du parc
```bash
mosquitto_sub -h broker.hivemq.com -t "devices/+/alert" -v
```

### S'abonner aux statuts online/offline
```bash
mosquitto_sub -h broker.hivemq.com -t "devices/+/lwt" -v
```

### Déclencher une OTA test
```bash
mosquitto_pub -h broker.hivemq.com \
  -t "devices/esp32-s3-test-01/ota/notify" \
  -m '{"version":"1.2.1","url":"https://...","sha256":"...","size":945632}'
```

---

## 📐 Résumé visuel

```
                    ┌─────────────────────┐
                    │  Backend HEELPMEE   │
                    │  (Spring Boot)      │
                    └──────────┬──────────┘
                               │
                ┌──────────────┴──────────────┐
                │      MQTT Broker            │
                │  (broker.hivemq.com)        │
                └──────────────┬──────────────┘
                               │
       ┌───────────────────────┼───────────────────────┐
       │ UPSTREAM (device→backend)    │ DOWNSTREAM (backend→device)
       │                              │
       ├─ alert    (QoS1, no retain)  │
       ├─ telemetry (QoS0, no retain) │
       ├─ state    (QoS0, RETAIN)     │   ota/notify (QoS1, no retain)
       ├─ lwt      (QoS1, RETAIN+LWT) │
       └─ ota/status (QoS1, no retain)│
                               │
                    ┌──────────┴──────────┐
                    │  Device HEELPMEE    │
                    │  (ESP32-S3 + LTE)   │
                    └─────────────────────┘
```

---

##  Annexe — Conventions de codage

| Élément | Convention |
|---------|------------|
| Encodage des payloads | **UTF-8** |
| Format des timestamps | **ISO 8601 UTC** (`YYYY-MM-DDTHH:MM:SS.000Z`) |
| Séparateur de niveaux | **`/`** (standard MQTT) |
| Casse | **`snake_case`** dans les champs JSON, **`lowercase`** dans les topics |
| Taille max payload | ~16 Ko (limite MQTT 3.1.1, mais on reste largement en-dessous) |

---

## 🔧 Configuration côté firmware

Les topics sont construits dynamiquement dans `mqtt_client.cpp` à partir du `DEVICE_ID` :

```cpp
// Extrait de mqtt_client.cpp
String prefix = String("devices/") + DEVICE_ID;
topicAlert      = prefix + "/alert";
topicTelemetry  = prefix + "/telemetry";
topicState      = prefix + "/state";
topicLwt        = prefix + "/lwt";
topicOtaStatus  = prefix + "/ota/status";
topicOtaNotify  = prefix + "/ota/notify";
```

Le `DEVICE_ID` est défini dans `config.h` (par défaut `"esp32-s3-test-01"`).
Pour un déploiement en série, ce DEVICE_ID devrait être **unique par device** (ex: dérivé du MAC ESP32).

---

*Document généré dans le cadre du projet HEELPMEE — PFE 2026*