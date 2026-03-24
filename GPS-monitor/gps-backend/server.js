const express = require("express");
const cors    = require("cors");
const fs      = require("fs-extra");
const path    = require("path");
const http    = require("http");
const socketIO = require("socket.io");

const app  = express();
const server = http.createServer(app);
const io = socketIO(server, {
  cors: { origin: "*", methods: ["GET", "POST"] }
});
const PORT = 3000;

// ── Fichiers de stockage ──────────────────────────────────────
const DATA_DIR        = path.join(__dirname, "data");
const LOCATIONS_FILE  = path.join(DATA_DIR, "locations.json");
const LAST_POS_FILE   = path.join(DATA_DIR, "last_position.json");

app.use(cors());
app.use(express.json());

// ── Servir les fichiers statiques du frontend ────────────────
const FRONTEND_DIR = path.join(__dirname, "../GPS-TRACKER");
app.use(express.static(FRONTEND_DIR));

// Servir GPS.html en tant que page d'accueil
app.get("/", (req, res) => {
  res.sendFile(path.join(FRONTEND_DIR, "GPS.html"));
});

// ── Socket.IO : Connexion et broadcast ───────────────────────
io.on("connection", (socket) => {
  console.log(`[Socket.IO] ✅ Client connecté: ${socket.id}`);
  
  socket.on("disconnect", () => {
    console.log(`[Socket.IO] ❌ Client déconnecté: ${socket.id}`);
  });
});

// ── Init fichiers JSON au démarrage ──────────────────────────
async function initStorage() {
  await fs.ensureDir(DATA_DIR);
  if (!(await fs.pathExists(LOCATIONS_FILE)))
    await fs.writeJson(LOCATIONS_FILE, { locations: [] }, { spaces: 2 });
  if (!(await fs.pathExists(LAST_POS_FILE)))
    await fs.writeJson(LAST_POS_FILE, { last_position: null }, { spaces: 2 });
}

// ── Validation coordonnées ───────────────────────────────────
function validateCoords(lat, lng) {
  const la = parseFloat(lat);
  const ln = parseFloat(lng);
  if (isNaN(la) || isNaN(ln))       return null;
  if (la < -90  || la > 90)         return null;
  if (ln < -180 || ln > 180)        return null;
  return { latitude: la, longitude: ln };
}

// ── Sauvegarde dans les 2 fichiers ───────────────────────────
async function saveLocation(data) {
  const history = await fs.readJson(LOCATIONS_FILE);
  history.locations.push(data);
  await fs.writeJson(LOCATIONS_FILE, history, { spaces: 2 });
  await fs.writeJson(LAST_POS_FILE, { last_position: data }, { spaces: 2 });
}


app.post("/api/gps/location", async (req, res) => {
  try {
    const { device_id, latitude, longitude, speed, altitude, satellites, fallback } = req.body;

    if (!device_id)
      return res.status(400).json({ success: false, error: "device_id manquant" });

    const coords = validateCoords(latitude, longitude);
    if (!coords)
      return res.status(400).json({ success: false, error: "latitude ou longitude invalide" });

    const entry = {
      id:         Date.now().toString(),
      device_id:  String(device_id),
      latitude:   coords.latitude,
      longitude:  coords.longitude,
      speed:      speed      != null ? parseFloat(speed)      : null,
      altitude:   altitude   != null ? parseFloat(altitude)   : null,
      satellites: satellites != null ? parseInt(satellites)   : null,
      fallback:   fallback === true || fallback === "true",
      timestamp:  new Date().toISOString(),
    };

    await saveLocation(entry);

    // Émettre les données GPS à tous les clients connectés
    io.emit("gps-data", entry);

    console.log(`[${entry.timestamp}] ✅ ${entry.device_id} → lat:${entry.latitude} lng:${entry.longitude} ${entry.fallback ? "(FALLBACK)" : "(LIVE)"}`);

    return res.status(201).json({
      success: true,
      message: entry.fallback ? "Dernière position reçue (GPS sans fix)" : "Position GPS enregistrée",
      data: entry,
    });
  } catch (err) {
    console.error("Erreur POST:", err);
    return res.status(500).json({ success: false, error: "Erreur serveur" });
  }
});

// ═══════════════════════════════════════════════════════════════
//  GET /api/gps/location/last
//  → Retourne la DERNIÈRE position connue (fallback si GPS éteint)
//  Query optionnelle: ?device_id=ESP32_001
// ═══════════════════════════════════════════════════════════════
app.get("/api/gps/location/last", async (req, res) => {
  try {
    const { device_id } = req.query;

    if (device_id) {
      const history = await fs.readJson(LOCATIONS_FILE);
      const filtered = history.locations.filter(l => l.device_id === String(device_id));
      if (!filtered.length)
        return res.status(404).json({ success: false, message: `Aucune position pour device: ${device_id}` });
      const last = filtered[filtered.length - 1];
      return res.json({ success: true, source: "last_known", data: last });
    }

    const stored = await fs.readJson(LAST_POS_FILE);
    if (!stored.last_position)
      return res.status(404).json({ success: false, message: "Aucune position enregistrée" });

    return res.json({ success: true, source: "last_known", data: stored.last_position });
  } catch (err) {
    return res.status(500).json({ success: false, error: "Erreur serveur" });
  }
});

// ═══════════════════════════════════════════════════════════════
//  GET /api/gps/locations
//  → Historique de toutes les positions
//  Query: ?device_id=xxx  &limit=50  &from=2024-01-01  &to=2024-12-31
// ═══════════════════════════════════════════════════════════════
app.get("/api/gps/locations", async (req, res) => {
  try {
    const { device_id, limit, from, to } = req.query;
    const history = await fs.readJson(LOCATIONS_FILE);
    let list = history.locations;

    if (device_id) list = list.filter(l => l.device_id === String(device_id));
    if (from)      list = list.filter(l => new Date(l.timestamp) >= new Date(from));
    if (to)        list = list.filter(l => new Date(l.timestamp) <= new Date(to));

    const max    = parseInt(limit) || 100;
    const result = list.slice(-max);

    return res.json({ success: true, total: list.length, returned: result.length, data: result });
  } catch (err) {
    return res.status(500).json({ success: false, error: "Erreur serveur" });
  }
});

// ═══════════════════════════════════════════════════════════════
//  GET /api/gps/status
//  → Statistiques : devices, activité, dernière position
// ═══════════════════════════════════════════════════════════════
app.get("/api/gps/status", async (req, res) => {
  try {
    const history = await fs.readJson(LOCATIONS_FILE);
    const lastPos = await fs.readJson(LAST_POS_FILE);
    const all     = history.locations;
    const devices = [...new Set(all.map(l => l.device_id))];

    const stats = devices.map(id => {
      const locs    = all.filter(l => l.device_id === id);
      const last    = locs[locs.length - 1];
      const diffMin = Math.floor((Date.now() - new Date(last.timestamp)) / 60000);
      return {
        device_id:          id,
        total_points:       locs.length,
        last_seen:          last.timestamp,
        minutes_since_last: diffMin,
        gps_active:         diffMin < 1,
        last_position:      { lat: last.latitude, lng: last.longitude },
      };
    });

    return res.json({
      success:          true,
      server_time:      new Date().toISOString(),
      total_locations:  all.length,
      total_devices:    devices.length,
      devices:          stats,
      last_position:    lastPos.last_position,
    });
  } catch (err) {
    return res.status(500).json({ success: false, error: "Erreur serveur" });
  }
});

// ═══════════════════════════════════════════════════════════════
//  DELETE /api/gps/locations
//  → Vider l'historique (tout ou par device)
// ═══════════════════════════════════════════════════════════════
app.delete("/api/gps/locations", async (req, res) => {
  try {
    const { device_id } = req.query;

    if (device_id) {
      const history = await fs.readJson(LOCATIONS_FILE);
      history.locations = history.locations.filter(l => l.device_id !== String(device_id));
      await fs.writeJson(LOCATIONS_FILE, history, { spaces: 2 });
      const remaining = history.locations;
      await fs.writeJson(LAST_POS_FILE,
        { last_position: remaining.length ? remaining[remaining.length - 1] : null },
        { spaces: 2 });
      return res.json({ success: true, message: `Historique effacé pour ${device_id}` });
    }

    await fs.writeJson(LOCATIONS_FILE, { locations: [] },   { spaces: 2 });
    await fs.writeJson(LAST_POS_FILE,  { last_position: null }, { spaces: 2 });
    return res.json({ success: true, message: "Tout l'historique effacé" });
  } catch (err) {
    return res.status(500).json({ success: false, error: "Erreur serveur" });
  }
});

// ── 404 ──────────────────────────────────────────────────────
app.use((req, res) => {
  res.status(404).json({
    success: false,
    error: "Route introuvable",
    routes: [
      "POST   /api/gps/location        ← ESP32 envoie position",
      "GET    /api/gps/location/last   ← Dernière position connue",
      "GET    /api/gps/locations       ← Historique complet",
      "GET    /api/gps/status          ← Statistiques",
      "DELETE /api/gps/locations       ← Vider l'historique",
    ],
  });
});

// ── Démarrage ────────────────────────────────────────────────
initStorage().then(() => {
  server.listen(PORT, () => {
    console.log("╔══════════════════════════════════╗");
    console.log("║   🛰️  GPS Tracker Backend         ║");
    console.log(`║   http://localhost:${PORT}          ║`);
    console.log("╚══════════════════════════════════╝");
    console.log(`📁 Données → ${DATA_DIR}`);
    console.log(`📄 Frontend → ${FRONTEND_DIR}`);
    console.log("Routes:");
    console.log("  POST   /api/gps/location");
    console.log("  GET    /api/gps/location/last");
    console.log("  GET    /api/gps/locations");
    console.log("  GET    /api/gps/status");
    console.log("  DELETE /api/gps/locations");
  });
});
