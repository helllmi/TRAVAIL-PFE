#include <Arduino.h>
#include <HardwareSerial.h>
#include "serial_comm.h"

HardwareSerial ss(1); // UART1 → SIM7670G : RX=17, TX=18

// ======== SERIAL COMMUNICATION ========

void SentSerial(const char *p_char) {
    ss.println(p_char);
}

bool SentMessage(const char *p_char, unsigned long timeout) {
    while (ss.available()) ss.read();

    ss.println(p_char);

    unsigned long start = millis();
    String resp = "";

    while (millis() - start < timeout) {
        while (ss.available()) resp += (char)ss.read();
        if (resp.indexOf("OK")    != -1) return true;
        if (resp.indexOf("ERROR") != -1) return false;
        delay(10);
    }
    return false;
}

String SentMessageResponse(const char *p_char, unsigned long timeout) {
    while (ss.available()) ss.read();

    ss.println(p_char);

    unsigned long start = millis();
    String resp = "";

    while (millis() - start < timeout) {
        while (ss.available()) resp += (char)ss.read();
        if (resp.indexOf("OK")    != -1 || resp.indexOf("ERROR") != -1) break;
        delay(10);
    }
    return resp;
}

String waitForResponse(const String& expected, unsigned long timeout) {
    unsigned long start = millis();
    String resp = "";

    while (millis() - start < timeout) {
        while (ss.available()) {
            resp += (char)ss.read();
        }
        if (resp.indexOf(expected) != -1) break;
        delay(100);
    }
    return resp;
}
