#include <Arduino.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi Configuration
const char* ssid = "helmi";
const char* password = "11160648bh";

const char* http_server = "http://10.196.128.184:3000/api/gps/location";

// GPS Configuration
static const int RXPin = 16;
static const int TXPin = 17;
static const uint32_t GPSBaud = 9600;

// Objects
SoftwareSerial mygps(RXPin, TXPin);
TinyGPSPlus gps;
HTTPClient http;

// GPS Variables
double latitude = 0.0;
double longitude = 0.0;
double altitude = 0.0;
double velocity = 0.0;
uint8_t satellites = 0;
String bearing = "";

// Timing variables
unsigned long lastPublish = 0;
const unsigned long publishInterval = 10000;  // Publish every 10 seconds

// Fonction de délai intelligent pour lectureGPS
static void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    while (mygps.available())
      gps.encode(mygps.read());
  } while (millis() - start < ms);
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connexion WiFi à: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" WiFi connecté!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" Échec connexion WiFi - Redémarrage...");
    delay(5000);
    ESP.restart();
  }
}

void reconnect() {
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠ WiFi déconnecté - Reconnexion...");
    setup_wifi();
  }
}

void checkGPS() {
  if (gps.charsProcessed() < 10) {
    Serial.println(" Attention: Aucun caractère GPS détecté - Vérifiez le câblage!");
  }
}

void displayInfo() {
  if (gps.location.isValid()) {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
    satellites = gps.satellites.value();
    velocity = gps.speed.kmph();
    bearing = TinyGPSPlus::cardinal(gps.course.value());
    
    if (gps.altitude.isValid()) {
      altitude = gps.altitude.meters();
    }

    Serial.println("===== DONNÉES GPS =====");
    Serial.print("Satellites: ");
    Serial.println(satellites);
    Serial.print("Latitude:  ");
    Serial.println(latitude, 6);
    Serial.print("Longitude: ");
    Serial.println(longitude, 6);
    Serial.print("Altitude:  ");
    Serial.print(altitude);
    Serial.println(" m");
    Serial.print("Vitesse:   ");
    Serial.print(velocity);
    Serial.println(" km/h");
    Serial.print("Direction: ");
    Serial.println(bearing);
    Serial.print("Heure:     ");
    Serial.print(gps.time.hour());
    Serial.print(":");
    Serial.print(gps.time.minute());
    Serial.print(":");
    Serial.println(gps.time.second());
    Serial.println("========================");
  } else {
    Serial.println(" En attente de signal GPS...");
  }
}

void sendGPSData(double lat, double lon, double alt, double vel, uint8_t sats, String dir, String time, String date) {
  // Vérifier la connexion WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("✗ WiFi non connecté - Impossible d'envoyer");
    return;
  }

  StaticJsonDocument<512> doc;
  doc["device_id"] = "ESP32-UNIT-001";
  doc["latitude"] = lat;
  doc["longitude"] = lon;
  doc["altitude"] = alt;
  doc["velocity"] = vel;
  doc["satellites"] = sats;
  doc["bearing"] = dir;
  doc["time"] = time;
  doc["date"] = date;

  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.print("JSON envoyé: ");
  Serial.println(jsonString);
  
  http.end(); // Fermer toute connexion précédente
  delay(100);
  
  if (!http.begin(http_server)) {
    Serial.println("✗ Erreur: Impossible de se connecter au serveur");
    return;
  }
  
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "ESP32-GPS-Tracker");
  
  int httpCode = http.POST(jsonString);
  
  Serial.print("Code HTTP: ");
  Serial.println(httpCode);
  
  if (httpCode == 200 || httpCode == 201) {
    Serial.println("✓ Données GPS envoyées avec succès");
  } else if (httpCode > 0) {
    Serial.print("✗ Erreur HTTP - Code: ");
    Serial.println(httpCode);
  } else {
    Serial.print("✗ Erreur de connexion: ");
    Serial.println(http.errorToString(httpCode));
  }
  
  http.end();
}

void testSendGPS() {
  Serial.println("\n========== TEST ENVOI GPS - 2 POSITIONS ===========");
  
  // Position 1
  Serial.println("\n--- Position 1 ---");
  double lat1 = 36.83989106901585;
  double lon1 = 10.247345665285435;
  double alt1 = 50.5;
  double vel1 = 45.2;
  uint8_t sats1 = 12;
  String dir1 = "NE";
  String time1 = "14:30:45";
  String date1 = "24/03/2026";
  
  Serial.print("Latitude:  ");
  Serial.println(lat1, 8);
  Serial.print("Longitude: ");
  Serial.println(lon1, 8);
  Serial.print("Altitude:  ");
  Serial.print(alt1);
  Serial.println(" m");
  
  sendGPSData(lat1, lon1, alt1, vel1, sats1, dir1, time1, date1);
  
 
  delay(10000);
  
  // Position 2
  Serial.println("\n--- Position 2 ---");
  double lat2 = 36.839679496273575;
  double lon2 = 10.247916251425654;
  double alt2 = 52.1;
  double vel2 = 48.5;
  uint8_t sats2 = 13;
  String dir2 = "NE";
  String time2 = "14:30:55";
  String date2 = "24/03/2026";
  
  Serial.print("Latitude:  ");
  Serial.println(lat2, 8);
  Serial.print("Longitude: ");
  Serial.println(lon2, 8);
  Serial.print("Altitude:  ");
  Serial.print(alt2);
  Serial.println(" m");
  
  sendGPSData(lat2, lon2, alt2, vel2, sats2, dir2, time2, date2);
  
  Serial.println("\n====================================\n");
}

void publishGPS() {
  reconnect();  // Vérifier la connexion WiFi

  if (gps.location.isValid()) {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
    satellites = gps.satellites.value();
    velocity = gps.speed.kmph();
    bearing = TinyGPSPlus::cardinal(gps.course.value());
    
    if (gps.altitude.isValid()) {
      altitude = gps.altitude.meters();
    }

    char timeStr[10];
    char dateStr[11];
    
    if (gps.time.isValid()) {
      sprintf(timeStr, "%02d:%02d:%02d", gps.time.hour(), gps.time.minute(), gps.time.second());
    } else {
      strcpy(timeStr, "00:00:00");
    }
    
    if (gps.date.isValid()) {
      sprintf(dateStr, "%02d/%02d/%04d", gps.date.day(), gps.date.month(), gps.date.year());
    } else {
      strcpy(dateStr, "00/00/0000");
    }

    sendGPSData(latitude, longitude, altitude, velocity, satellites, bearing, String(timeStr), String(dateStr));
  } else {
    Serial.println("⏳ Position GPS non valide - Publication ignorée");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n====== ESP32 GPS HTTP TRACKER =====");
  Serial.println("Initialisation en cours...");
  
  // Initialize GPS serial with SoftwareSerial
  mygps.begin(GPSBaud);
  Serial.println(" Interface GPS initialisée (SoftwareSerial)");
  
  // Connect to WiFi
  setup_wifi();
  
  // Attendre un peu pour que WiFi soit stable
  delay(2000);
  
  // Test d'envoi avec données fictives
  testSendGPS();
  
  Serial.println("\nServeur HTTP configuré: ");
  Serial.println(http_server);
  
  Serial.println("\n✓ Configuration complète - En attente de signal GPS...\n");
}

void loop() {
  // Handle WiFi disconnections
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠ WiFi déconnecté - Reconnexion...");
    setup_wifi();
  }

  // Envoyer les données de test à intervalle régulier
  if (millis() - lastPublish >= publishInterval) {
    testSendGPS();
    lastPublish = millis();
  }
  
  delay(100);
} 