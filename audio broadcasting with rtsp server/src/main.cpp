/////////////////////////////////////////////////////////////////
/*
  Broadcasting Your Voice with ESP32-S3 & INMP441
  RTSP Server - Sans arduino-audio-tools
  Uniquement: Micro-RTSP-Audio
  https://github.com/pschatzmann/Micro-RTSP-Audio.git
*/
/////////////////////////////////////////////////////////////////

/*
  Bibliothèques requises :
  - Micro-RTSP-Audio : https://github.com/pschatzmann/Micro-RTSP-Audio.git
  
  Installation :
    cd ~/Documents/Arduino/libraries
    git clone https://github.com/pschatzmann/Micro-RTSP-Audio.git

  Écoute :
    VLC → Fichier → Ouvrir flux réseau → rtsp://<IP_ESP32>
    ffplay rtsp://<IP_ESP32>
*/
/////////////////////////////////////////////////////////////////

#include <WiFi.h>
#include <driver/i2s.h>
#include "IAudioSource.h"
#include "AudioStreamer.h"
#include "RTSPServer.h"

// ─── Pins INMP441 ─────────────────────────────────────────────
#define I2S_SD   32
#define I2S_WS   25
#define I2S_SCK  33
#define I2S_PORT I2S_NUM_0

// ─── Paramètres audio ─────────────────────────────────────────
// Micro-RTSP-Audio supporte uniquement 16000 Hz mono 16 bits
#define SAMPLE_RATE    16000
#define BITS           16
#define DMA_BUF_COUNT  8
#define DMA_BUF_LEN    512

// ─── WiFi ──────────────────────────────────────────────────────
const char* ssid     = "NA Stagiaires";
const char* password = "$tage@N*2023*A";

// =====================================================================
// Buffer circulaire thread-safe entre micTask et le serveur RTSP
// =====================================================================
#define RING_BUF_SIZE  (DMA_BUF_LEN * DMA_BUF_COUNT * 2)  // 8192 bytes

static uint8_t  ringBuffer[RING_BUF_SIZE];
static volatile size_t ringHead = 0;   // écrit par micTask
static volatile size_t ringTail = 0;   // lu    par RTSP
static SemaphoreHandle_t ringMutex;

// Écriture dans le ring buffer (appelée depuis micTask)
void ringWrite(const uint8_t* data, size_t len) {
  xSemaphoreTake(ringMutex, portMAX_DELAY);
  for (size_t i = 0; i < len; i++) {
    size_t nextHead = (ringHead + 1) % RING_BUF_SIZE;
    if (nextHead != ringTail) {          
      ringBuffer[ringHead] = data[i];
      ringHead = nextHead;
    }
    // si plein : on drop l'ancien (overwrite)
    else {
      ringBuffer[ringHead] = data[i];
      ringHead = nextHead;
      ringTail = (ringTail + 1) % RING_BUF_SIZE;
    }
  }
  xSemaphoreGive(ringMutex);
}

// Lecture depuis le ring buffer (appelée par IAudioSource::readBytes)
size_t ringRead(uint8_t* data, size_t maxLen) {
  xSemaphoreTake(ringMutex, portMAX_DELAY);
  size_t count = 0;
  while (count < maxLen && ringHead != ringTail) {
    data[count++] = ringBuffer[ringTail];
    ringTail = (ringTail + 1) % RING_BUF_SIZE;
  }
  xSemaphoreGive(ringMutex);
  return count;
}

size_t ringAvailable() {
  size_t h = ringHead;
  size_t t = ringTail;
  if (h >= t) return h - t;
  return RING_BUF_SIZE - t + h;
}

// =====================================================================
// Classe IAudioSource : interface entre le ring buffer et Micro-RTSP-Audio
// C'est elle que le AudioStreamer appelle pour récupérer les données PCM
// =====================================================================
class I2SMicSource : public IAudioSource {
public:

  // Appelée par AudioStreamer pour obtenir les samples PCM
  // Equivalent de : client.sendBinary((const char*)sBuffer, bytesIn)
  // dans le code original — sauf qu'ici c'est le RTSP qui tire les données
  int readBytes(void* data, int length) override {
    // Attendre qu'il y ait assez de données dans le ring buffer
    uint32_t timeout = 200;  // ms max d'attente
    while (ringAvailable() < (size_t)length && timeout > 0) {
      vTaskDelay(1 / portTICK_PERIOD_MS);
      timeout--;
    }
    if (ringAvailable() == 0) return 0;

    return (int)ringRead((uint8_t*)data, (size_t)length);
  }

  // Fréquence d'échantillonnage en Hz
 
};

// =====================================================================
// Objets globaux RTSP
// =====================================================================
I2SMicSource   micSource;
AudioStreamer*  streamer = nullptr;
RTSPServer*     rtsp     = nullptr;

// =====================================================================
// Configuration du driver I2S (identique au code original)
// =====================================================================
void i2s_install() {
  const i2s_config_t i2s_config = {
    .mode               = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate        = SAMPLE_RATE,
    .bits_per_sample    = i2s_bits_per_sample_t(BITS),
    .channel_format     = I2S_CHANNEL_FMT_ONLY_LEFT,   // L/R → GND sur INMP441
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags   = 0,
    .dma_buf_count      = DMA_BUF_COUNT,
    .dma_buf_len        = DMA_BUF_LEN,
    .use_apll           = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk         = 0
  };
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}

void i2s_setpin() {
  const i2s_pin_config_t pin_config = {
    .bck_io_num   = I2S_SCK,
    .ws_io_num    = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = I2S_SD
  };
  i2s_set_pin(I2S_PORT, &pin_config);
}

// =====================================================================
// micTask : lit le microphone I2S et pousse dans le ring buffer
// Logique identique au code original de Eric N. (ThatProject)
// =====================================================================
void micTask(void* parameter) {
  i2s_install();
  i2s_setpin();
  i2s_start(I2S_PORT);

  uint8_t i2sBuffer[DMA_BUF_LEN * 2];  // buffer local en bytes
  size_t bytesIn = 0;

  Serial.println("[micTask] démarré");

  while (1) {
    // Lecture exactement comme dans le code original :
    // esp_err_t result = i2s_read(I2S_PORT, &sBuffer, bufferLen, &bytesIn, portMAX_DELAY)
    esp_err_t result = i2s_read(
        I2S_PORT,
        (void*)i2sBuffer,
        sizeof(i2sBuffer),
        &bytesIn,
        portMAX_DELAY     // bloquant, identique à l'original
    );

    if (result == ESP_OK && bytesIn > 0) {
      // Au lieu de : client.sendBinary((const char*)sBuffer, bytesIn)
      // on pousse dans le ring buffer pour que le serveur RTSP tire les données
      ringWrite(i2sBuffer, bytesIn);
    }
  }
}

// ─── Connexion WiFi ────────────────────────────────────────────
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connexion WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connecté !");
  Serial.print("Adresse IP : ");
  Serial.println(WiFi.localIP());
}

// =====================================================================
void setup() {
  Serial.begin(115200);

  // 1. Mutex pour le ring buffer
  ringMutex = xSemaphoreCreateMutex();

  // 2. WiFi
  connectWiFi();

  // 3. Démarrage du serveur RTSP
  streamer = new AudioStreamer(&micSource);
  rtsp     = new RTSPServer(streamer,554,1);  // port 8554, core 1
  rtsp->runAsync();   // lance le serveur dans sa propre tâche FreeRTOS

  Serial.println("╔══════════════════════════════════════════╗");
  Serial.println("║         Serveur RTSP démarré             ║");
  Serial.printf( "║  → rtsp://%s\n",
                 WiFi.localIP().toString().c_str());
  Serial.println("║  VLC : Fichier → Ouvrir flux réseau      ║");
  Serial.println("╚══════════════════════════════════════════╝");

  // 4. Démarrage de micTask sur le core 1 (comme l'original)
  xTaskCreatePinnedToCore(
    micTask,      // fonction
    "micTask",    // nom
    10000,        // stack
    NULL,         // paramètre
    1,            // priorité
    NULL,         // handle
    1             // core 1
  );
}

void loop() {
  // Tout tourne dans les tâches FreeRTOS
  delay(1000);
}