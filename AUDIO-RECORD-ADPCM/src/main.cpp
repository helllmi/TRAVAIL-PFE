#include <driver/i2s.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include "AudioTools.h"
#include "ADPCM.h"
#include "ADPCMEncoder.h"
#include "AudioTools/AudioCodecs/CodecADPCM.h"
#include "AudioTools/AudioCodecs/AudioCodecsBase.h"

// ─── WiFi ────────────────────────────────────────────────
const char* ssid     = "NA Stagiaires";
const char* password = "$tage@N*2023*A";

// ─── I2S ─────────────────────────────────────────────────
#define I2S_WS   25
#define I2S_SD   32
#define I2S_SCK  33
#define I2S_PORT  I2S_NUM_0

#define I2S_SAMPLE_RATE   (8000)
#define I2S_SAMPLE_BITS   (16)
#define I2S_READ_LEN      (4 * 1024)
#define RECORD_TIME       (10)
#define I2S_CHANNEL_NUM   (1)
#define FLASH_RECORD_SIZE (I2S_CHANNEL_NUM * I2S_SAMPLE_RATE * I2S_SAMPLE_BITS / 8 * RECORD_TIME)

// Taille estimée après compression ADPCM ≈ PCM / 4
#define ADPCM_ESTIMATED_SIZE (FLASH_RECORD_SIZE / 4)

// ─── Fichier ──────────────────────────────────────────────
const char filename[] = "/rec.adpcm";  // ← changé : .wav → .adpcm

// ─── État partagé ─────────────────────────────────────────
volatile bool recording       = false;
volatile bool recordingDone   = false;
volatile int  currentProgress = 0;
void i2s_adc(void* arg);
// ─── Serveur ──────────────────────────────────────────────
WebServer server(80);

// ════════════════════════════════════════════════════════
//  État d'erreur global
// ════════════════════════════════════════════════════════
String lastError = "";
size_t finalSize = 0;

// ════════════════════════════════════════════════════════
//  Classe FilePrint : adapte File → Print pour l'encodeur
// ════════════════════════════════════════════════════════
class FilePrint : public Print {
public:
  File* f;
  FilePrint(File* file) : f(file) {}
  size_t write(uint8_t b) override               { return f->write(b); }
  size_t write(const uint8_t* buf, size_t size)  { return f->write(buf, size); }
};

// ════════════════════════════════════════════════════════
//  Handlers HTTP  (identiques à ton code — seul /download change)
// ════════════════════════════════════════════════════════
void handleRoot() {
  File f = SPIFFS.open("/index.html", FILE_READ);
  if (!f) { server.send(404, "text/plain", "index.html non trouvé"); return; }
  server.streamFile(f, "text/html");
  f.close();
}

void handleCSS() {
  File f = SPIFFS.open("/style.css", FILE_READ);
  if (!f) { server.send(404, "text/plain", "style.css non trouvé"); return; }
  server.streamFile(f, "text/css");
  f.close();
}

void handleJS() {
  File f = SPIFFS.open("/script.js", FILE_READ);
  if (!f) { server.send(404, "text/plain", "script.js non trouvé"); return; }
  server.streamFile(f, "application/javascript");
  f.close();
}

void handleRecord() {
  if (recording) { server.send(409, "text/plain", "Déjà en cours"); return; }
  lastError     = "";
  finalSize     = 0;
  recordingDone = false;
  server.send(200, "text/plain", "OK");
  xTaskCreate(i2s_adc, "i2s_adc", 1024 * 8, NULL, 1, NULL); // ← 8K au lieu de 4K pour ADPCM
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
  if (recording)      { server.send(503, "text/plain", "Enregistrement en cours."); return; }
  if (!recordingDone) { server.send(404, "text/plain", "Aucun enregistrement."); return; }

  File f = SPIFFS.open(filename, FILE_READ);
  if (!f || f.size() == 0) {
    server.send(500, "text/plain", "Fichier vide ou introuvable.");
    if (f) f.close();
    return;
  }
  Serial.printf("[HTTP] Envoi fichier ADPCM : %u bytes\n", f.size());
  server.sendHeader("Content-Disposition", "attachment; filename=\"recording.adpcm\""); // ← .adpcm
  server.sendHeader("Content-Length", String(f.size()));
  server.streamFile(f, "audio/x-adpcm"); // ← MIME type ADPCM
  f.close();
}

// ════════════════════════════════════════════════════════
//  I2S init  (identique)
// ════════════════════════════════════════════════════════
void i2sInit() {
  i2s_config_t cfg = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate          = I2S_SAMPLE_RATE,
    .bits_per_sample      = i2s_bits_per_sample_t(I2S_SAMPLE_BITS),
    .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags     = 0,
    .dma_buf_count        = 8,
    .dma_buf_len          = 256,
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
//  Mise à l'échelle des données I2S  (identique)
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
//  Tâche d'enregistrement  ← MODIFIÉE : encode en ADPCM
// ════════════════════════════════════════════════════════
void i2s_adc(void* arg) {
  recording       = true;
  recordingDone   = false;
  currentProgress = 0;

  // ── Vérification espace SPIFFS ───────────────────────
  // On a besoin de ~PCM/4 car ADPCM compresse ×4
  size_t freeBytes = SPIFFS.totalBytes() - SPIFFS.usedBytes();
  size_t needed    = ADPCM_ESTIMATED_SIZE + 512;
  Serial.printf("[REC] Espace libre : %u B  |  Nécessaire (ADPCM ~×4) : %u B\n", freeBytes, needed);

  if (freeBytes < needed) {
    lastError = "SPIFFS plein";
    Serial.println("[REC] ERREUR : espace insuffisant !");
    recording = false;
    vTaskDelete(NULL);
    return;
  }

  // ── Ouvrir fichier ───────────────────────────────────
  SPIFFS.remove(filename);
  File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    lastError = "Ouverture fichier impossible";
    recording = false;
    vTaskDelete(NULL);
    return;
  }

  // ── Initialiser l'encodeur ADPCM ─────────────────────
  FilePrint filePrint(&file);

  ADPCMEncoder encoder(AV_CODEC_ID_ADPCM_MS);
  AudioInfo    audioInfo;
  audioInfo.sample_rate     = I2S_SAMPLE_RATE;  // 8000 Hz
  audioInfo.channels        = I2S_CHANNEL_NUM;  // 1 = Mono
  audioInfo.bits_per_sample = I2S_SAMPLE_BITS;  // 16 bits

  encoder.setAudioInfo(audioInfo);
  encoder.setOutput(filePrint);

  if (!encoder.begin()) {
    lastError = "Encodeur ADPCM init échoué";
    file.close();
    recording = false;
    vTaskDelete(NULL);
    return;
  }
  Serial.println("[REC] Encodeur ADPCM initialisé.");

  // ── Allocation buffers ───────────────────────────────
  char*    i2s_read_buff    = (char*)    calloc(I2S_READ_LEN, 1);
  uint8_t* flash_write_buff = (uint8_t*) calloc(I2S_READ_LEN, 1);

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
  i2s_read(I2S_PORT, i2s_read_buff, I2S_READ_LEN, &bytes_read, portMAX_DELAY);
  i2s_read(I2S_PORT, i2s_read_buff, I2S_READ_LEN, &bytes_read, portMAX_DELAY);
  Serial.println("[REC] Démarré...");

  // ── Boucle principale ────────────────────────────────
  int flash_wr_size = 0;
  int writeErrors   = 0;

  while (flash_wr_size < FLASH_RECORD_SIZE) {
    // 1. Lire les données brutes I2S
    i2s_read(I2S_PORT, i2s_read_buff, I2S_READ_LEN, &bytes_read, portMAX_DELAY);

    // 2. Mettre à l'échelle I2S 12 bits → PCM 16 bits
    i2s_adc_data_scale(flash_write_buff, (uint8_t*)i2s_read_buff, bytes_read);

    // 3. Encoder en ADPCM → écrit compressé dans SPIFFS via FilePrint
    size_t written = encoder.write(flash_write_buff, bytes_read);
    if (written != bytes_read) {
      writeErrors++;
      Serial.printf("[REC] Erreur encodage (x%d)\n", writeErrors);
      if (writeErrors >= 3) {
        lastError = "Echec encodage ADPCM";
        break;
      }
    }

    flash_wr_size  += (int)bytes_read;
    currentProgress = flash_wr_size * 100 / FLASH_RECORD_SIZE;
    ets_printf("[REC] %d%%\n", currentProgress);
  }

  // ── Fermeture propre ─────────────────────────────────
  encoder.end();
  file.flush();
  file.close();

  // ── Vérification finale ──────────────────────────────
  File chk = SPIFFS.open(filename, FILE_READ);
  if (chk) {
    finalSize = chk.size();
    chk.close();
    Serial.printf("[REC] Fichier ADPCM : %u bytes  (PCM aurait été %u bytes → ×%.1f)\n",
                  finalSize, FLASH_RECORD_SIZE,
                  finalSize > 0 ? (float)FLASH_RECORD_SIZE / finalSize : 0.0f);
    if (finalSize == 0) lastError = "Fichier final vide";
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
//  Setup & Loop  (identiques)
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

  server.on("/",          HTTP_GET, handleRoot);
  server.on("/style.css", HTTP_GET, handleCSS);
  server.on("/script.js", HTTP_GET, handleJS);
  server.on("/record",    HTTP_GET, handleRecord);
  server.on("/status",    HTTP_GET, handleStatus);
  server.on("/download",  HTTP_GET, handleDownload);
  server.begin();
  Serial.println("[HTTP] Serveur démarré.");
}

void loop() {
  server.handleClient();
}
