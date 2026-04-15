#include <Arduino.h>
#include <ArduinoJson.h>
#include "lte.h"
#include "serial_comm.h"
#include "gps.h"
#include "battery.h"

// ======== LTE FUNCTIONS ========
String getNetworkTime() {
    String resp = SentMessageResponse("AT+CCLK?", 3000);
    int idx = resp.indexOf("+CCLK: \"");
    if (idx == -1) return "";
    String time = resp.substring(idx + 8);
    time = time.substring(0, time.indexOf("\""));
    return time; 
}
bool setupLTE() {
    Serial.println("Setting up LTE...");

    String cereg = SentMessageResponse("AT+CEREG?", 3000);
    Serial.println("CEREG: " + cereg);

    SentMessage("AT+CGDCONT=1,\"IP\",\"internet.ooredoo.tn\"", 3000);
    delay(500);

    if (!SentMessage("AT+CGACT=1,1", 10000)) {
        Serial.println("PDP activation failed, retrying...");
        delay(2000);
        SentMessage("AT+CGACT=1,1", 10000);
    }

    String ip = SentMessageResponse("AT+CGPADDR=1", 3000);
    Serial.println("IP: " + ip);

    Serial.println("LTE Ready");
    return true;
}

bool httpPostLTE(const String& url, const String& jsonBody) {
    Serial.println("HTTP POST via LTE...");

    while (ss.available()) ss.read();

    SentMessage("AT+HTTPTERM", 2000);
    delay(500);

    if (!SentMessage("AT+HTTPINIT", 3000)) {
        Serial.println("HTTPINIT failed");
        return false;
    }

    String urlCmd = "AT+HTTPPARA=\"URL\",\"" + url + "\"";
    if (!SentMessage(urlCmd.c_str(), 3000)) {
        Serial.println("URL set failed");
        SentMessage("AT+HTTPTERM", 2000);
        return false;
    }

    if (!SentMessage("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 3000)) {
        Serial.println("CONTENT set failed");
        SentMessage("AT+HTTPTERM", 2000);
        return false;
    }

    SentMessage("AT+HTTPPARA=\"USERDATA\",\"Accept: application/json\"", 3000);
    SentMessage("AT+HTTPPARA=\"CID\",1", 3000);

    int bodyLen = jsonBody.length();
    String dataCmd = "AT+HTTPDATA=" + String(bodyLen) + ",10000";

    while (ss.available()) ss.read();
    ss.println(dataCmd);

    String downloadResp = waitForResponse("DOWNLOAD", 5000);
    if (downloadResp.indexOf("DOWNLOAD") == -1) {
        Serial.println("DOWNLOAD prompt not received: " + downloadResp);
        SentMessage("AT+HTTPTERM", 2000);
        return false;
    }

    ss.print(jsonBody);
    String bodyResp = waitForResponse("OK", 5000);
    Serial.println("Body sent: " + bodyResp);

    while (ss.available()) ss.read();

    ss.println("AT+HTTPACTION=1");
    String okResp = waitForResponse("OK", 3000);
    Serial.println("HTTPACTION OK: " + okResp);

    String postResp = waitForResponse("+HTTPACTION", 30000);
    Serial.println("POST response: " + postResp);

    if (postResp.indexOf("+HTTPACTION") == -1) {
        Serial.println("No HTTPACTION response received");
        SentMessage("AT+HTTPTERM", 2000);
        return false;
    }

    // Extraire le code HTTP
    int httpCode = 0;
    int actionIdx = postResp.indexOf("+HTTPACTION: 1,");
    if (actionIdx != -1) {
        String codeStr = postResp.substring(actionIdx + 15, actionIdx + 18);
        httpCode = codeStr.toInt();
        Serial.println("HTTP Code: " + String(httpCode));
    }

    // Lire réponse serveur
    if (httpCode > 0) {
        while (ss.available()) ss.read();
        int sizeIdx = postResp.lastIndexOf(",");
        int dataSize = postResp.substring(sizeIdx + 1).toInt();
        if (dataSize > 0) {
            String readCmd = "AT+HTTPREAD=0," + String(dataSize);
            ss.println(readCmd);
            String result = waitForResponse("OK", 5000);
            Serial.println("Server response: " + result);
        }
    }

    SentMessage("AT+HTTPTERM", 2000);

    return (httpCode == 200 || httpCode == 201);
}

void sendAlertLTE() {
    float batteryLevel = readBattery();
    Serial.printf("Battery before send: %.1f%%\n", batteryLevel);
    String networkTime = getNetworkTime();

    StaticJsonDocument<1024> doc;
    if(networkTime != "") {
        doc["timestamp"] = networkTime;
    } else {
        doc["timestamp"] = millis() / 1000; 
    }

    
    doc["priority"]  = "CRITICAL";

    // LOCATION
    doc["location"]["latitude"]                 = currentGPS.latitude;
    doc["location"]["longitude"]                = currentGPS.longitude;
    doc["location"]["accuracy_meters"]          = 5.0;
    doc["location"]["altitude_meters"]          = currentGPS.altitude;
    doc["location"]["speed_kmh"]                = currentGPS.speed;
    doc["location"]["heading_degrees"]          = nullptr;
    doc["location"]["location_source"]          = "GPS";
    doc["location"]["address_reverse_geocoded"] = "Tunisia";

    // CONTEXT
    doc["context"]["fall_detected"]    = false;
    doc["context"]["heart_rate_bpm"]   = 100;
    doc["context"]["ambient_noise_db"] = 70.0;
    doc["context"]["motion_state"]     = "STATIONARY";
    doc["context"]["geofence_status"]  = "UNKNOWN";

    // METADATA
    doc["metadata"]["schema_version"] = "1.0";
    doc["metadata"]["kafka_topic"]    = "helpmee_alerts";
    doc["metadata"]["partition_key"]  = "ESP32S3";
    doc["metadata"]["ttl_seconds"]    = 86400;
    doc["metadata"]["retry_count"]    = 0;

    // IDs
    doc["alert_id"]          = "ALT-ESP32";
    doc["device_id"]         = "DEV-ESP32";
    doc["user_id"]           = "USR-001";
    doc["alert_type"]        = "PANIC";
    doc["trigger_method"]    = "AUTO";
    doc["press_count"]       = 1;
    doc["press_duration_ms"] = 1000;

    // DEVICE — batterie réelle depuis MAX17048
    doc["device_status"]["battery_level_pct"]   = (int)batteryLevel;
    doc["device_status"]["signal_strength_dbm"] = -70;
    doc["device_status"]["connectivity_type"]   = "LTE";

    String json;
    serializeJson(doc, json);

    Serial.println("Sending HTTP via LTE...");
    bool ok = httpPostLTE("http://helpmee.nacloud.tn/alerts", json);

    if (ok) {
        Serial.println("✅ Alert sent OK via LTE");
    } else {
        Serial.println("❌ Alert failed, retrying LTE setup...");
        setupLTE();
    }
}
