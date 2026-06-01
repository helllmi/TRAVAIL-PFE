"""
Mini-serveur OTA pour tester les mises à jour ESP32.
Expose:
  GET  /firmware/version  -> renvoie la dernière version disponible (JSON)
  GET  /firmware/latest.bin -> télécharge le binaire de la dernière version
  GET  /                  -> page web simple pour piloter le tout
  POST /firmware/upload   -> upload d'un nouveau firmware (admin)
  GET  /devices           -> liste des devices qui ont check-in
"""

from flask import Flask, jsonify, send_file, request, render_template_string
from pathlib import Path
import hashlib
import json
import time

app = Flask(__name__)

# Dossier où on stocke les firmwares uploadés
FIRMWARE_DIR = Path(__file__).parent / "firmwares"
FIRMWARE_DIR.mkdir(exist_ok=True)

# Fichier de métadonnées (version courante, hash, etc.)
META_FILE = FIRMWARE_DIR / "metadata.json"

# Tracking en mémoire des devices qui font des check-in
devices_seen = {}


def load_metadata():
    """Charge les métadonnées du firmware courant."""
    if META_FILE.exists():
        return json.loads(META_FILE.read_text())
    return {"version": "0.0.0", "filename": None, "sha256": None, "size": 0}


def save_metadata(meta):
    META_FILE.write_text(json.dumps(meta, indent=2))


def compute_sha256(filepath):
    """Calcule le SHA256 d'un fichier — utilisé pour la vérification d'intégrité."""
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        for chunk in iter(lambda: f.read(8192), b""):
            h.update(chunk)
    return h.hexdigest()


# ============================================================
# Endpoints API
# ============================================================

@app.route("/firmware/version")
def get_version():
    """
    Le device interroge cet endpoint pour savoir s'il doit se mettre à jour.
    Il envoie sa version actuelle en query param: /firmware/version?current=1.0.0
    """
    current = request.args.get("current", "0.0.0")
    device_id = request.args.get("device_id", "unknown")

    # On enregistre le check-in du device
    devices_seen[device_id] = {
        "last_seen": time.strftime("%Y-%m-%d %H:%M:%S"),
        "current_version": current,
    }

    meta = load_metadata()
    update_available = (current != meta["version"]) and meta["filename"] is not None

    print(f"[CHECK-IN] device={device_id} current={current} -> "
          f"latest={meta['version']} update={'YES' if update_available else 'NO'}")

    return jsonify({
        "latest_version": meta["version"],
        "update_available": update_available,
        "url": f"/firmware/latest.bin" if update_available else None,
        "sha256": meta.get("sha256"),
        "size": meta.get("size", 0),
    })


@app.route("/firmware/latest.bin")
def download_firmware():
    """Le device télécharge le binaire ici."""
    meta = load_metadata()
    if not meta["filename"]:
        return "No firmware available", 404

    filepath = FIRMWARE_DIR / meta["filename"]
    if not filepath.exists():
        return "Firmware file missing", 404

    print(f"[DOWNLOAD] Serving {meta['filename']} ({meta['size']} bytes)")
    return send_file(filepath, mimetype="application/octet-stream")


@app.route("/firmware/upload", methods=["POST"])
def upload_firmware():
    """
    Upload d'un nouveau firmware via curl ou la page web.
    POST avec form-data: file=<binaire>, version=<x.y.z>
    """
    if "file" not in request.files:
        return jsonify({"error": "No file provided"}), 400

    version = request.form.get("version", "").strip()
    if not version:
        return jsonify({"error": "Version is required"}), 400

    file = request.files["file"]
    filename = f"firmware-v{version}.bin"
    filepath = FIRMWARE_DIR / filename
    file.save(filepath)

    sha256 = compute_sha256(filepath)
    size = filepath.stat().st_size

    meta = {
        "version": version,
        "filename": filename,
        "sha256": sha256,
        "size": size,
        "uploaded_at": time.strftime("%Y-%m-%d %H:%M:%S"),
    }
    save_metadata(meta)

    print(f"[UPLOAD] New firmware v{version} ({size} bytes) sha256={sha256[:16]}...")
    return jsonify(meta)


@app.route("/devices")
def list_devices():
    return jsonify(devices_seen)


# ============================================================
# Page web admin minimaliste
# ============================================================

DASHBOARD_HTML = """
<!DOCTYPE html>
<html>
<head>
    <title>OTA Test Server</title>
    <style>
        body { font-family: monospace; max-width: 800px; margin: 40px auto; padding: 20px; }
        h1 { border-bottom: 2px solid #333; padding-bottom: 10px; }
        .box { border: 1px solid #ccc; padding: 15px; margin: 15px 0; border-radius: 4px; }
        .ok { color: green; } .warn { color: orange; }
        input, button { padding: 8px; margin: 4px 0; font-family: inherit; }
        button { background: #333; color: white; border: none; cursor: pointer; }
        button:hover { background: #555; }
        pre { background: #f4f4f4; padding: 10px; overflow-x: auto; }
    </style>
</head>
<body>
    <h1>OTA Test Server</h1>

    <div class="box">
        <h3>Firmware actuel</h3>
        <p><strong>Version:</strong> {{ meta.version }}</p>
        <p><strong>Fichier:</strong> {{ meta.filename or 'aucun' }}</p>
        <p><strong>SHA256:</strong> <code>{{ (meta.sha256 or 'N/A')[:32] }}...</code></p>
        <p><strong>Taille:</strong> {{ meta.size }} bytes</p>
    </div>

    <div class="box">
        <h3>Uploader un nouveau firmware</h3>
        <form action="/firmware/upload" method="POST" enctype="multipart/form-data">
            <label>Version (ex: 1.0.1):</label><br>
            <input type="text" name="version" required pattern="\\d+\\.\\d+\\.\\d+"><br>
            <label>Fichier .bin:</label><br>
            <input type="file" name="file" accept=".bin" required><br><br>
            <button type="submit">Publier</button>
        </form>
    </div>

    <div class="box">
        <h3>Devices vus récemment</h3>
        {% if devices %}
            <pre>{{ devices_json }}</pre>
        {% else %}
            <p class="warn">Aucun device n'a encore fait de check-in.</p>
        {% endif %}
    </div>

    <div class="box">
        <h3>Endpoints disponibles</h3>
        <ul>
            <li><code>GET /firmware/version?current=X.Y.Z&device_id=esp32-01</code></li>
            <li><code>GET /firmware/latest.bin</code></li>
            <li><code>POST /firmware/upload</code> (form: file, version)</li>
            <li><code>GET /devices</code></li>
        </ul>
    </div>
</body>
</html>
"""


@app.route("/")
def dashboard():
    meta = load_metadata()
    return render_template_string(
        DASHBOARD_HTML,
        meta=meta,
        devices=devices_seen,
        devices_json=json.dumps(devices_seen, indent=2),
    )


if __name__ == "__main__":
    print("=" * 60)
    print("OTA Test Server")
    print("=" * 60)
    print(f"Firmware dir: {FIRMWARE_DIR}")
    print(f"Dashboard:    http://0.0.0.0:5000")
    print(f"API:          http://<your-ip>:5000/firmware/version")
    print("=" * 60)
    # IMPORTANT: 0.0.0.0 pour que l'ESP32 puisse accéder depuis le réseau
    app.run(host="0.0.0.0", port=5000, debug=True)
