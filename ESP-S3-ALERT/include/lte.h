#ifndef LTE_H
#define LTE_H

#include <Arduino.h>

// Fonctions LTE
bool setupLTE();
bool httpPostLTE(const String& url, const String& jsonBody);
void sendAlertLTE();
String getNetworkTime();
#endif
