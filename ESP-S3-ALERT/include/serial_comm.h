#ifndef SERIAL_COMM_H
#define SERIAL_COMM_H

#include <Arduino.h>
#include <HardwareSerial.h>

extern HardwareSerial ss;

// Fonctions de communication série
void SentSerial(const char *p_char);
bool SentMessage(const char *p_char, unsigned long timeout = 2000);
String SentMessageResponse(const char *p_char, unsigned long timeout = 3000);
String waitForResponse(const String& expected, unsigned long timeout);

#endif
