#include <Arduino.h>
#include "audio.h"
#include "config.h"
#include <driver/i2s.h>
#include <SD_MMC.h>

// ============================================================================
//  ÉTAT INTERNE
// ============================================================================
static volatile bool recording = false;
static volatile bool stopRequested = false;
static uint32_t lastSeq = 0;
static char lastFilePath[64] = "";
static TaskHandle_t recordTaskHandle = NULL;

// ============================================================================
//  I2S INIT (paramètres validés par ton test)
// ============================================================================
static void i2s_init()
{
    i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = AUDIO_SAMPLE_RATE,
        .bits_per_sample = (i2s_bits_per_sample_t)AUDIO_SAMPLE_BITS,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
        .intr_alloc_flags = 0,
        .dma_buf_count = 8,
        .dma_buf_len = 1024,
        .use_apll = 1};
    i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);

    const i2s_pin_config_t pins = {
        .bck_io_num = PIN_I2S_SCK,
        .ws_io_num = PIN_I2S_WS,
        .data_out_num = -1,
        .data_in_num = PIN_I2S_SD};
    i2s_set_pin(I2S_NUM_0, &pins);
}

// ============================================================================
//  MISE À L'ÉCHELLE 
// ============================================================================
static void i2s_adc_data_scale(uint8_t *d_buff, uint8_t *s_buff, uint32_t len)
{
    uint32_t j = 0;
    for (uint32_t i = 0; i < len; i += 2)
    {
        // Lire le sample 16-bit signé brut (little-endian)
        int16_t sample = (int16_t)((s_buff[i + 1] << 8) | s_buff[i]);

        // Gain x4 (à ajuster selon ton micro)
        int32_t amplified = (int32_t)sample * 4;

        // Clamp anti-saturation
        if (amplified > 32767)
            amplified = 32767;
        if (amplified < -32768)
            amplified = -32768;

        sample = (int16_t)amplified;

        // Écrire en little-endian dans le buffer destination
        d_buff[j++] = (uint8_t)(sample & 0xFF);
        d_buff[j++] = (uint8_t)((sample >> 8) & 0xFF);
    }
}

// ============================================================================
//  HEADER WAV
// ============================================================================
static void wavHeader(byte *h, int wavSize)
{
    h[0] = 'R';
    h[1] = 'I';
    h[2] = 'F';
    h[3] = 'F';
    unsigned int fs = wavSize + AUDIO_WAV_HEADER_SIZE - 8;
    h[4] = (byte)fs;
    h[5] = (byte)(fs >> 8);
    h[6] = (byte)(fs >> 16);
    h[7] = (byte)(fs >> 24);

    h[8] = 'W';
    h[9] = 'A';
    h[10] = 'V';
    h[11] = 'E';
    h[12] = 'f';
    h[13] = 'm';
    h[14] = 't';
    h[15] = ' ';
    h[16] = 0x10;
    h[17] = 0x00;
    h[18] = 0x00;
    h[19] = 0x00;
    h[20] = 0x01;
    h[21] = 0x00;
    h[22] = AUDIO_CHANNELS;
    h[23] = 0x00;

    uint32_t sr = AUDIO_SAMPLE_RATE;
    h[24] = (byte)sr;
    h[25] = (byte)(sr >> 8);
    h[26] = (byte)(sr >> 16);
    h[27] = (byte)(sr >> 24);

    uint32_t br = sr * AUDIO_CHANNELS * AUDIO_SAMPLE_BITS / 8;
    h[28] = (byte)br;
    h[29] = (byte)(br >> 8);
    h[30] = (byte)(br >> 16);
    h[31] = (byte)(br >> 24);

    uint16_t ba = AUDIO_CHANNELS * AUDIO_SAMPLE_BITS / 8;
    h[32] = (byte)ba;
    h[33] = (byte)(ba >> 8);

    h[34] = AUDIO_SAMPLE_BITS;
    h[35] = 0x00;

    h[36] = 'd';
    h[37] = 'a';
    h[38] = 't';
    h[39] = 'a';
    h[40] = (byte)wavSize;
    h[41] = (byte)(wavSize >> 8);
    h[42] = (byte)(wavSize >> 16);
    h[43] = (byte)(wavSize >> 24);
}

// ============================================================================
//  COMPTEUR SÉQUENTIEL PERSISTANT
// ============================================================================
static uint32_t loadSeq()
{
    if (!SD_MMC.exists(AUDIO_SEQ_FILE))
        return 0;
    File f = SD_MMC.open(AUDIO_SEQ_FILE, FILE_READ);
    if (!f)
        return 0;
    uint32_t n = f.parseInt();
    f.close();
    return n;
}

static void saveSeq(uint32_t n)
{
    File f = SD_MMC.open(AUDIO_SEQ_FILE, FILE_WRITE);
    if (!f)
        return;
    f.print(n);
    f.close();
}

// ============================================================================
//  TÂCHE D'ENREGISTREMENT (non-bloquante pour la loop principale)
// ============================================================================
static void recordTask(void *arg)
{
    recording = true;
    stopRequested = false;

    // 1) Préparer le chemin du fichier
    uint32_t seq = ++lastSeq;
    saveSeq(seq);
    snprintf(lastFilePath, sizeof(lastFilePath),
             "%s/alert_%03u.wav", AUDIO_DIR_SD, seq);
    Serial.printf("[AUDIO] Recording to %s\n", lastFilePath);

    // 2) Ouvrir le fichier
    File file = SD_MMC.open(lastFilePath, FILE_WRITE);
    if (!file)
    {
        Serial.println("[AUDIO] ERROR: cannot open file on SD");
        recording = false;
        recordTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    // 3) Écrire le header WAV (provisoire avec la taille théorique)
    byte header[AUDIO_WAV_HEADER_SIZE];
    wavHeader(header, AUDIO_FILE_SIZE);
    file.write(header, AUDIO_WAV_HEADER_SIZE);

    // 4) Allouer les buffers
    char *read_buff = (char *)calloc(AUDIO_I2S_READ_LEN, 1);
    uint8_t *write_buff = (uint8_t *)calloc(AUDIO_I2S_READ_LEN, 1);
    if (!read_buff || !write_buff)
    {
        Serial.println("[AUDIO] ERROR: out of memory");
        file.close();
        free(read_buff);
        free(write_buff);
        recording = false;
        recordTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    // 5) Lectures de chauffe (2 reads pour stabiliser)
    size_t bytes_read;
    i2s_read(I2S_NUM_0, read_buff, AUDIO_I2S_READ_LEN, &bytes_read, portMAX_DELAY);
    i2s_read(I2S_NUM_0, read_buff, AUDIO_I2S_READ_LEN, &bytes_read, portMAX_DELAY);

    // Diagnostic peak — utile pour vérifier que le micro capte
    int16_t peak = 0;
    for (uint32_t k = 0; k < bytes_read; k += 2)
    {
        int16_t s = (int16_t)((read_buff[k + 1] << 8) | read_buff[k]);
        if (s < 0)
            s = -s;
        if (s > peak)
            peak = s;
    }
    Serial.printf("[AUDIO] PEAK after warmup: %d / 32767\n", peak);

    // 6) Boucle d'enregistrement (stop sur taille atteinte OU stopRequested)
    int total_written = 0;
    int lastPct = -1;
    while (total_written < AUDIO_FILE_SIZE && !stopRequested)
    {
        i2s_read(I2S_NUM_0, read_buff, AUDIO_I2S_READ_LEN, &bytes_read, portMAX_DELAY);
        i2s_adc_data_scale(write_buff, (uint8_t *)read_buff, bytes_read);
        file.write((const byte *)write_buff, bytes_read);
        total_written += (int)bytes_read;

        int pct = total_written * 100 / AUDIO_FILE_SIZE;
        if (pct / 10 != lastPct / 10)
        {
            Serial.printf("[AUDIO] %d%%\n", pct);
            lastPct = pct;
        }
    }

    // 7) Si stop anticipé, mettre à jour le header avec la VRAIE taille
    if (stopRequested && total_written < AUDIO_FILE_SIZE)
    {
        file.seek(0);
        byte realHeader[AUDIO_WAV_HEADER_SIZE];
        wavHeader(realHeader, total_written);
        file.write(realHeader, AUDIO_WAV_HEADER_SIZE);
        Serial.printf("[AUDIO] Stopped early, header updated (%d bytes audio)\n",
                      total_written);
    }

    // 8) Fermeture
    file.flush();
    file.close();
    free(read_buff);
    free(write_buff);

    Serial.printf("[AUDIO] Recording done: %s (%d bytes audio)\n",
                  lastFilePath, total_written);

    recording = false;
    stopRequested = false;
    recordTaskHandle = NULL;
    vTaskDelete(NULL);
}

// ============================================================================
//  API PUBLIQUE
// ============================================================================
bool audio_init()
{
    Serial.println("[AUDIO] Init...");

    // La SD est déjà initialisée dans main.cpp
    if (!SD_MMC.cardSize())
    {
        Serial.println("[AUDIO] ERROR: SD not initialized");
        return false;
    }
    Serial.printf("[AUDIO] Using existing SD, size=%lluMB\n",
                  SD_MMC.cardSize() / (1024 * 1024));

    // Créer le dossier des alertes
    if (!SD_MMC.exists(AUDIO_DIR_SD))
    {
        SD_MMC.mkdir(AUDIO_DIR_SD);
    }

    // Charger le dernier numéro séquentiel
    lastSeq = loadSeq();
    Serial.printf("[AUDIO] Last seq number: %u\n", lastSeq);

    // Init I2S
    i2s_init();
    Serial.println("[AUDIO] I2S initialized (WS=%d, SCK=%d, SD=%d)");

    return true;
}

bool audio_startRecording()
{
    if (recording)
    {
        Serial.println("[AUDIO] Already recording");
        return false;
    }

    BaseType_t ok = xTaskCreate(
        recordTask, "audio_rec", 8 * 1024, NULL, 1, &recordTaskHandle);

    if (ok != pdPASS)
    {
        Serial.println("[AUDIO] ERROR: cannot create record task");
        return false;
    }
    return true;
}

void audio_stopRecording()
{
    if (!recording)
        return;
    Serial.println("[AUDIO] Stop requested");
    stopRequested = true;
}

bool audio_isRecording()
{
    return recording;
}

uint32_t audio_getLastSeq()
{
    return lastSeq;
}

const char *audio_getLastFilePath()
{
    return lastFilePath;
}