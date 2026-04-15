#include <Arduino.h>
#include <Wire.h>
#include <SD_MMC.h>
#include "serial_comm.h"
#include "battery.h"
#include "gps.h"
#include "lte.h"

// ======== TIMING ========
unsigned long lastDeleteTime = 0;
const unsigned long deleteInterval = 5 * 60 * 1000;

unsigned long lastAlertTime = 0;
const unsigned long alertInterval = 10000; // 10 sec

void setup() {

    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);

    Serial.begin(115200);
    ss.begin(115200, SERIAL_8N1, 17, 18);

    // I2C pour MAX17048 : SDA=15, SCL=16
    Wire.begin(15, 16);

    delay(3000);

    // Test AT
    Serial.println("Waiting for SIM7670G...");
    while (!SentMessage("AT", 3000)) {
        delay(1000);
    }
    Serial.println("SIM7670G OK");

    // Test batterie
    float bat = readBattery();
    Serial.printf("Battery: %.1f%%\n", bat);

    // GNSS
    Serial.println("Powering GNSS...");
    SentMessage("AT+CGNSSPWR=1", 5000);
    delay(3000);

    // SD
    SD_MMC.setPins(5, 4, 6);
    if (!SD_MMC.begin("/sdcard", true)) {
        Serial.println("SD Failed");
    } else {
        Serial.println("SD OK");
    }

    // LTE
    setupLTE();

    lastDeleteTime = millis();
}

void loop() {

    // ===== DELETE FILE EVERY 5 MIN =====
    if (millis() - lastDeleteTime >= deleteInterval) {
        Serial.println("Deleting SD file...");
        if (SD_MMC.exists("/gps_log.csv")) {
            SD_MMC.remove("/gps_log.csv");
        }
        lastDeleteTime = millis();
    }

    // ===== READ GPS =====
    if (readGPS()) {

        Serial.printf("GPS: %.6f, %.6f\n", currentGPS.latitude, currentGPS.longitude);

        // Afficher batterie dans le terminal
        float bat = readBattery();
        Serial.printf("Battery: %.1f%%\n", bat);

        logGPS();

        // ===== SEND ALERT VIA LTE =====
        if (millis() - lastAlertTime > alertInterval) {
            sendAlertLTE();
            lastAlertTime = millis();
        }

    } else {
        Serial.println("GPS: waiting for fix...");
    }

    delay(2000);
}