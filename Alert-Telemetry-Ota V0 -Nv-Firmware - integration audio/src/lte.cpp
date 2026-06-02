#include <Arduino.h>
#include "lte.h"
#include "serial_comm.h"
#include "config.h"

// ============================================================================
//  CONFIGURATION LTE
// ============================================================================
// httpPostLTE() et sendAlertLTE() ont été retirés. Les alertes partent
// désormais par MQTT (cf. mqtt_client.cpp). La couche LTE est conservée
// pour :
//   - l'horodatage réseau (CCLK) utilisé dans le JSON d'alerte
//   - la mesure de signal (CSQ) intégrée à la télémétrie
//   - la migration future où MQTT passera par AT+CMQTT* du SIM7670G
//   - l'OTA en LTE via AT+HTTPDATA chunked (à venir)
// ============================================================================

String getNetworkTime()
{
    String resp = SentMessageResponse("AT+CCLK?", 3000);
    // Réponse type : +CCLK: "26/04/16,13:32:45+04"
    int idx = resp.indexOf("+CCLK: \"");
    if (idx == -1)
        return "";

    String raw = resp.substring(idx + 8);
    raw = raw.substring(0, raw.indexOf("\""));

    String yy = raw.substring(0, 2);
    String mm = raw.substring(3, 5);
    String dd = raw.substring(6, 8);
    String hh = raw.substring(9, 11);
    String min = raw.substring(12, 14);
    String sec = raw.substring(15, 17);

    return "20" + yy + "-" + mm + "-" + dd + "T" + hh + ":" + min + ":" + sec + ".000Z";
}

bool setupLTE()
{
    Serial.println("Setting up LTE...");

    String cereg = SentMessageResponse("AT+CEREG?", 3000);
    Serial.println("CEREG: " + cereg);

    String apnCmd = String("AT+CGDCONT=1,\"IP\",\"") + LTE_APN + "\"";
    SentMessage(apnCmd.c_str(), 3000);
    delay(500);

    if (!SentMessage("AT+CGACT=1,1", 10000))
    {
        Serial.println("PDP activation failed, retrying...");
        delay(2000);
        SentMessage("AT+CGACT=1,1", 10000);
    }

    String ip = SentMessageResponse("AT+CGPADDR=1", 3000);
    Serial.println("IP: " + ip);
    Serial.println("[LTE] Opening network stack (NETOPEN)...");

    // Vérifier si déjà ouvert
    String netCheck = SentMessageResponse("AT+NETOPEN?", 3000);
    if (netCheck.indexOf("+NETOPEN: 1") != -1)
    {
        Serial.println("[LTE] Network stack already open");
    }
    else
    {
        
        SentMessage("AT+NETOPEN", 15000);
        delay(2000);

        
        netCheck = SentMessageResponse("AT+NETOPEN?", 3000);
        if (netCheck.indexOf("+NETOPEN: 1") == -1)
        {
            Serial.println("[LTE] WARNING: NETOPEN unconfirmed, MQTT-LTE may fail");
        }
        else
        {
            Serial.println("[LTE] Network stack opened");
        }
    }
    delay(1000);
    Serial.println("[LTE] Enabling modem sleep (CSCLK=1)...");
    SentMessage("AT+CSCLK=1", 3000);
    delay(500);

    Serial.println("LTE Ready");
    return true;
}

SignalInfo getSignalInfo()
{
    SignalInfo info;
    String resp = SentMessageResponse("AT+CSQ", 3000);
    int idx = resp.indexOf("+CSQ: ");
    if (idx == -1)
        return info;

    String data = resp.substring(idx + 6);
    info.rssi = data.substring(0, data.indexOf(",")).toInt();
    info.ber = data.substring(data.indexOf(",") + 1).toInt();

    Serial.printf("Signal RSSI: %d, BER: %d\n", info.rssi, info.ber);
    return info;
}
