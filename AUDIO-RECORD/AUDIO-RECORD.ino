#include <driver/i2s.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WebServer.h>

// ─── WiFi ────────────────────────────────────────────────
const char* ssid     = "NA Stagiaires";
const char* password = "$tage@N*2023*A";

// ─── I2S ─────────────────────────────────────────────────
#define I2S_WS   25
#define I2S_SD   32
#define I2S_SCK  33
#define I2S_PORT  I2S_NUM_0

#define I2S_SAMPLE_RATE   (8000)   // réduit : 16000 → 8000 Hz
#define I2S_SAMPLE_BITS   (16)
#define I2S_READ_LEN      (4 * 1024)  // réduit : 16K → 4K
#define RECORD_TIME       (10)         // réduit : 20s → 10s
#define I2S_CHANNEL_NUM   (1)
#define FLASH_RECORD_SIZE (I2S_CHANNEL_NUM * I2S_SAMPLE_RATE * I2S_SAMPLE_BITS / 8 * RECORD_TIME)

// ─── Fichier ──────────────────────────────────────────────
const char filename[] = "/rec.wav";
const int  headerSize = 44;
File       file;

// ─── État partagé ─────────────────────────────────────────
volatile bool recording     = false;
volatile bool recordingDone = false;
volatile int  currentProgress = 0;

// ─── Serveur ──────────────────────────────────────────────
WebServer server(80);

// ════════════════════════════════════════════════════════
//  Page HTML
// ════════════════════════════════════════════════════════
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Recorder</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      background: #0d0d0d;
      color: #e8e8e8;
      font-family: 'Courier New', monospace;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      min-height: 100vh;
      gap: 2rem;
    }
    h1 { font-size: 1.4rem; letter-spacing: 0.3em; text-transform: uppercase; color: #00ff99; text-shadow: 0 0 12px #00ff9966; }
    .status { font-size: 0.85rem; letter-spacing: 0.15em; color: #888; min-height: 1.2em; }
    .status.recording { color: #ff4455; animation: blink 1s infinite; }
    .status.done      { color: #00ff99; }
    .status.error     { color: #ffaa00; }
    @keyframes blink { 50% { opacity: 0.3; } }
    .btn {
      padding: 0.8rem 2.4rem;
      border: 2px solid #00ff99;
      background: transparent;
      color: #00ff99;
      font-family: inherit;
      font-size: 0.9rem;
      letter-spacing: 0.2em;
      text-transform: uppercase;
      cursor: pointer;
      transition: all 0.2s;
    }
    .btn:hover  { background: #00ff9922; box-shadow: 0 0 18px #00ff9944; }
    .btn:active { transform: scale(0.97); }
    .btn:disabled { opacity: 0.3; cursor: default; }
    .btn.download { border-color: #4488ff; color: #4488ff; text-decoration: none; display: inline-block; }
    .btn.download:hover { background: #4488ff22; box-shadow: 0 0 18px #4488ff44; }
    .progress-bar { width: 260px; height: 4px; background: #222; border-radius: 2px; overflow: hidden; }
    .progress-fill { height: 100%; width: 0%; background: linear-gradient(90deg, #00ff99, #4488ff); transition: width 0.5s; }
    .info { font-size: 0.75rem; color: #555; letter-spacing: 0.1em; }
  </style>
</head>
<body>
  <h1>&#9632; ESP32 Recorder</h1>
  <div class="status" id="status">En attente...</div>
  <div class="progress-bar"><div class="progress-fill" id="bar"></div></div>
  <button class="btn" id="recBtn" onclick="startRecording()">Démarrer</button>
  <a id="dlLink" class="btn download" href="/download" style="display:none">&#8659; Télécharger WAV</a>
  <div class="info">8 kHz · 16 bit · Mono · 10 s</div>
  <script>
    let polling = null;
    async function startRecording() {
      document.getElementById('recBtn').disabled = true;
      document.getElementById('dlLink').style.display = 'none';
      setStatus('Enregistrement en cours...', 'recording');
      await fetch('/record');
      polling = setInterval(checkStatus, 1000);
    }
    async function checkStatus() {
      try {
        const r = await fetch('/status');
        const d = await r.json();
        document.getElementById('bar').style.width = d.progress + '%';
        if (d.error) {
          clearInterval(polling);
          setStatus('Erreur : ' + d.error, 'error');
          document.getElementById('recBtn').disabled = false;
        } else if (d.done) {
          clearInterval(polling);
          setStatus('Terminé ✓  (' + d.size + ' bytes)', 'done');
          document.getElementById('dlLink').style.display = 'inline-block';
          document.getElementById('recBtn').disabled = false;
        } else {
          setStatus('Enregistrement... ' + d.progress + '%', 'recording');
        }
      } catch(e) {}
    }
    function setStatus(msg, cls) {
      const el = document.getElementById('status');
      el.textContent = msg;
      el.className = 'status ' + (cls || '');
    }
  </script>
</body>
</html>
)rawliteral";

// ════════════════════════════════════════════════════════
//  État d'erreur global
// ════════════════════════════════════════════════════════
String lastError   = "";
size_t finalSize   = 0;

// ════════════════════════════════════════════════════════
//  Handlers HTTP
// ════════════════════════════════════════════════════════
void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

void handleRecord() {
  if (recording) { server.send(409, "text/plain", "Déjà en cours"); return; }
  lastError = "";
  finalSize = 0;
  server.send(200, "text/plain", "OK");
  xTaskCreate(i2s_adc, "i2s_adc", 1024 * 4, NULL, 1, NULL);
}

void handleStatus() {
  String json = "{";
  json += "\"done\":"      + String(recordingDone ? "true" : "false") + ",";
  json += "\"recording\":" + String(recording     ? "true" : "false") + ",";
  json += "\"progress\":"  + String(currentProgress) + ",";
  json += "\"size\":"      + String(finalSize);
  if (lastError.length() > 0) json += ",\"error\":\"" + lastError + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleDownload() {
  if (recording) { server.send(503, "text/plain", "Enregistrement en cours."); return; }
  if (!recordingDone) { server.send(404, "text/plain", "Aucun enregistrement."); return; }

  File f = SPIFFS.open(filename, FILE_READ);
  if (!f || f.size() == 0) {
    server.send(500, "text/plain", "Fichier vide ou introuvable.");
    if (f) f.close();
    return;
  }
  Serial.printf("[HTTP] Envoi fichier : %u bytes\n", f.size());
  server.sendHeader("Content-Disposition", "attachment; filename=\"recording.wav\"");
  server.sendHeader("Content-Length", String(f.size()));
  server.streamFile(f, "audio/wav");
  f.close();
}

// ════════════════════════════════════════════════════════
//  I2S init
// ════════════════════════════════════════════════════════
void i2sInit() {
  i2s_config_t cfg = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate          = I2S_SAMPLE_RATE,
    .bits_per_sample      = i2s_bits_per_sample_t(I2S_SAMPLE_BITS),
    .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags     = 0,
    .dma_buf_count        = 8,     // réduit : 64 → 8
    .dma_buf_len          = 256,   // réduit : 1024 → 256
    .use_apll             = 1
  };
  i2s_driver_install(I2S_PORT, &cfg, 0, NULL);

  const i2s_pin_config_t pins = {
    .bck_io_num   = I2S_SCK,
    .ws_io_num    = I2S_WS,
    .data_out_num = -1,
    .data_in_num  = I2S_SD
  };
  i2s_set_pin(I2S_PORT, &pins);
}

// ════════════════════════════════════════════════════════
//  Mise à l'échelle des données I2S
// ════════════════════════════════════════════════════════
void i2s_adc_data_scale(uint8_t* d_buff, uint8_t* s_buff, uint32_t len) {
  uint32_t j = 0;
  for (int i = 0; i < len; i += 2) {
    uint32_t val = (((uint16_t)(s_buff[i + 1] & 0xf) << 8) | s_buff[i]);
    d_buff[j++] = 0;
    d_buff[j++] = val * 256 / 2048;
  }
}

// ════════════════════════════════════════════════════════
//  Tâche d'enregistrement
// ════════════════════════════════════════════════════════
void i2s_adc(void* arg) {
  recording       = true;
  recordingDone   = false;
  currentProgress = 0;

  // ── Vérification espace SPIFFS ───────────────────────
  size_t freeBytes = SPIFFS.totalBytes() - SPIFFS.usedBytes();
  size_t needed    = (size_t)FLASH_RECORD_SIZE + headerSize;
  Serial.printf("[REC] Espace libre : %u B  |  Nécessaire : %u B\n", freeBytes, needed);

  if (freeBytes < needed) {
    lastError = "SPIFFS plein";
    Serial.println("[REC] ERREUR : espace insuffisant !");
    recording = false;
    vTaskDelete(NULL);
    return;
  }

  // ── Ouvrir fichier ───────────────────────────────────
  SPIFFS.remove(filename);
  file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    lastError = "Ouverture fichier impossible";
    recording = false;
    vTaskDelete(NULL);
    return;
  }

  // ── Header WAV ───────────────────────────────────────
  byte header[headerSize];
  wavHeader(header, FLASH_RECORD_SIZE);
  if (file.write(header, headerSize) != headerSize) {
    lastError = "Ecriture header échouée";
    file.close();
    recording = false;
    vTaskDelete(NULL);
    return;
  }
  Serial.printf("[REC] Header écrit. Taille cible audio : %u B\n", FLASH_RECORD_SIZE);

  // ── Allocation buffers ───────────────────────────────
  int i2s_read_len = I2S_READ_LEN;

  char*    i2s_read_buff    = (char*)    calloc(i2s_read_len, 1);
  uint8_t* flash_write_buff = (uint8_t*) calloc(i2s_read_len, 1);

  if (!i2s_read_buff || !flash_write_buff) {
    lastError = "Mémoire insuffisante";
    file.close();
    free(i2s_read_buff);
    free(flash_write_buff);
    recording = false;
    vTaskDelete(NULL);
    return;
  }

  // ── Lectures de chauffe ──────────────────────────────
  size_t bytes_read;
  i2s_read(I2S_PORT, i2s_read_buff, i2s_read_len, &bytes_read, portMAX_DELAY);
  i2s_read(I2S_PORT, i2s_read_buff, i2s_read_len, &bytes_read, portMAX_DELAY);
  Serial.println("[REC] Démarré...");

  // ── Boucle principale ────────────────────────────────
  int    flash_wr_size = 0;
  int    writeErrors   = 0;

  while (flash_wr_size < FLASH_RECORD_SIZE) {
    i2s_read(I2S_PORT, i2s_read_buff, i2s_read_len, &bytes_read, portMAX_DELAY);
    i2s_adc_data_scale(flash_write_buff, (uint8_t*)i2s_read_buff, bytes_read);

    size_t w = file.write((const byte*)flash_write_buff, bytes_read);
    if (w != bytes_read) {
      writeErrors++;
      Serial.printf("[REC] Erreur écriture %u/%u (x%d)\n", w, bytes_read, writeErrors);
      if (writeErrors >= 3) {
        lastError = "Echec ecriture flash";
        break;
      }
    }

    flash_wr_size  += (int)bytes_read;
    currentProgress = flash_wr_size * 100 / FLASH_RECORD_SIZE;
    ets_printf("[REC] %d%%\n", currentProgress);
  }

  // ── Fermeture propre ─────────────────────────────────
  file.flush();
  file.close();

  // ── Vérification finale ──────────────────────────────
  File chk = SPIFFS.open(filename, FILE_READ);
  if (chk) {
    finalSize = chk.size();
    chk.close();
    Serial.printf("[REC] Fichier final : %u bytes\n", finalSize);
    if (finalSize <= (size_t)headerSize) {
      lastError = "Fichier final vide";
    }
  } else {
    lastError = "Fichier introuvable apres fermeture";
  }

  free(i2s_read_buff);
  free(flash_write_buff);

  recording     = false;
  recordingDone = (lastError.length() == 0);

  if (lastError.length())
    Serial.printf("[REC] ERREUR : %s\n", lastError.c_str());
  else
    Serial.println("[REC] Terminé — disponible sur /download");

  vTaskDelete(NULL);
}

// ════════════════════════════════════════════════════════
//  WAV Header
// ════════════════════════════════════════════════════════
void wavHeader(byte* h, int wavSize) {
  // RIFF
  h[0]='R'; h[1]='I'; h[2]='F'; h[3]='F';
  unsigned int fs = wavSize + headerSize - 8;
  h[4]=(byte)fs; h[5]=(byte)(fs>>8); h[6]=(byte)(fs>>16); h[7]=(byte)(fs>>24);
  // WAVE fmt
  h[8]='W'; h[9]='A'; h[10]='V'; h[11]='E';
  h[12]='f'; h[13]='m'; h[14]='t'; h[15]=' ';
  h[16]=0x10; h[17]=0x00; h[18]=0x00; h[19]=0x00; // chunk size
  h[20]=0x01; h[21]=0x00;                           // PCM
  h[22]=0x01; h[23]=0x00;                           // Mono
  // Sample rate (8000 = 0x1F40)
  h[24]=0x40; h[25]=0x1F; h[26]=0x00; h[27]=0x00;
  // Byte rate = sampleRate * channels * bitsPerSample/8 = 8000*1*2 = 16000 = 0x3E80
  h[28]=0x80; h[29]=0x3E; h[30]=0x00; h[31]=0x00;
  h[32]=0x02; h[33]=0x00;                           // block align
  h[34]=0x10; h[35]=0x00;                           // 16 bits
  // data
  h[36]='d'; h[37]='a'; h[38]='t'; h[39]='a';
  h[40]=(byte)wavSize; h[41]=(byte)(wavSize>>8);
  h[42]=(byte)(wavSize>>16); h[43]=(byte)(wavSize>>24);
}

// ════════════════════════════════════════════════════════
//  Setup & Loop
// ════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);

  if (!SPIFFS.begin(true)) {
    Serial.println("ERREUR SPIFFS !");
    while (1) yield();
  }
  Serial.printf("[SPIFFS] Total: %u B  Utilisé: %u B\n",
                SPIFFS.totalBytes(), SPIFFS.usedBytes());

  Serial.printf("[WiFi] Connexion à %s", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.printf("\n[WiFi] IP : http://%s\n", WiFi.localIP().toString().c_str());

  i2sInit();

  server.on("/",         HTTP_GET, handleRoot);
  server.on("/record",   HTTP_GET, handleRecord);
  server.on("/status",   HTTP_GET, handleStatus);
  server.on("/download", HTTP_GET, handleDownload);
  server.begin();
  Serial.println("[HTTP] Serveur démarré.");
}

void loop() {
  server.handleClient();
}