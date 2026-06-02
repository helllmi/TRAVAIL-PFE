#include <Arduino.h>
#include "power_manager.h"
#include "config.h"
#include <WiFi.h>

// ============================================================================
//  ÉTAT INTERNE
// ============================================================================
static uint32_t currentFreq = CPU_FREQ_BOOT_MHZ;

// ============================================================================
//  INIT
// ============================================================================
void power_init() {
    Serial.println("[PWR] Init power manager");

    // Au boot, on tourne à pleine puissance pour une init rapide
    power_setcpufreq(CPU_FREQ_BOOT_MHZ);

    Serial.printf("[PWR] CPU freq = %u MHz\n", currentFreq);
}

// ============================================================================
//  CPU FREQUENCY SCALING
// ============================================================================
void power_setcpufreq(uint32_t mhz) {
    // setCpuFrequencyMhz() est fournie par le core Arduino ESP32
    bool ok = setCpuFrequencyMhz(mhz);
    if (ok) {
        currentFreq = mhz;
        Serial.printf("[PWR] CPU freq set to %u MHz\n", mhz);
    } else {
        Serial.printf("[PWR] FAILED to set CPU freq to %u MHz\n", mhz);
    }
}

uint32_t power_getcpufreq() {
    return currentFreq;
}

// ============================================================================
//  MODE STANDBY — économie d'énergie
// ============================================================================
void power_enterstandby() {
    Serial.println("[PWR] Entering STANDBY power mode");

    // 1) Réduire la fréquence CPU
    power_setcpufreq(CPU_FREQ_STANDBY_MHZ);

    // 2) Activer le WiFi modem-sleep (si WiFi connecté)
    //    Le modem WiFi dort entre les beacons de l'AP, se réveille pour
    //    les recevoir. Économie sans perdre la connexion.
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.setSleep(WIFI_PS_MIN_MODEM);
        Serial.println("[PWR] WiFi modem-sleep enabled");
    }
}

// ============================================================================
//  MODE ACTION — pleine puissance
// ============================================================================
void power_enteraction() {
    Serial.println("[PWR] Entering ACTION power mode (full power)");

    // 1) Pleine fréquence CPU pour réactivité maximale
    power_setcpufreq(CPU_FREQ_ACTION_MHZ);

    // 2) Désactiver le WiFi sleep : on veut le débit maximal pour
    //    transmettre les alertes le plus vite possible
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.setSleep(WIFI_PS_NONE);
        Serial.println("[PWR] WiFi sleep disabled (full throughput)");
    }
}