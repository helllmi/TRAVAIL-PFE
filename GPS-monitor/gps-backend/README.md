# GPS Backend

> Serveur Node.js pour le stockage et la diffusion en temps réel des données de localisation GPS.

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
- **API REST** : Réception des coordonnées GPS via des requêtes HTTP POST.
- **Temps réel** : Diffusion instantanée des positions aux clients connectés via **Socket.IO**.
- **Stockage persistant** : Sauvegarde des historiques et de la dernière position dans des fichiers JSON (`data/`).
- **Serveur de fichiers** : Sert l'interface frontend pour la visualisation sur carte.

## Installation
Assurez-vous d'avoir Node.js installé sur votre machine.

```bash
cd gps-backend
npm install
```

## Utilisation
Pour lancer le serveur :
```bash
node server.js
```
Le serveur sera accessible sur `http://localhost:3000`.

## Configuration
Le serveur utilise le port `3000` par défaut. Les données sont stockées dans le dossier `./data/`.
L'interface frontend est attendue dans le dossier `../GPS-TRACKER` par défaut (modifiable dans `server.js`).

## Structure du projet
- `server.js` : Point d'entrée principal du serveur.
- `data/` : Contient les fichiers JSON de stockage.
  - `locations.json` : Historique complet.
  - `last_position.json` : Dernière position connue.
- `package.json` : Dépendances et scripts Node.js.

## Développement
Le projet utilise :
- **Express** : Framework web.
- **Socket.IO** : Communication bidirectionnelle en temps réel.
- **fs-extra** : Gestion simplifiée du système de fichiers.

## Auteur
[Helmi]

## Licence
ISC
