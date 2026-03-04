#include <WiFi.h>
#include <WiFiClientSecure.h> // <--- ADD THIS LINE TO FIX THE ERROR
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>  
#include <WebServer.h>
#include <ESPmDNS.h>

// ---------------- CONFIG ----------------
const char* ssid = "Shivaraj Dodamani";
const char* password = "10102003";

// THE LINK TO YOUR COMPILED .BIN FILE (Use the 'Raw' link from GitHub)
String firmwareURL = "https://raw.githubusercontent.com/username/repo/main/firmware.bin"; 

// ---------------- HARDWARE & VARIABLES ----------------
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);
WebServer server(80);
#define TEMP_PIN 34
#define PULSE_PIN 35
#define SWITCH_PIN 25

float smoothedTemp = 26.0;
int bpm = 0;
unsigned long lastBeatTime = 0;
bool beatDetected = false;
double lastLat = 0.0, lastLon = 0.0;
bool emergencyActive = false;
unsigned long buttonPressStart = 0;
bool isBeingPressed = false;

// ---------------- THE GLOBAL OTA FUNCTION ----------------
void performGlobalUpdate() {
  Serial.println("Starting Global OTA Update...");
  WiFiClientSecure client; // This now works because of the new #include
  client.setInsecure();    // Allows connection to GitHub without a certificate

  t_httpUpdate_return ret = httpUpdate.update(client, firmwareURL);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("Update Failed: (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("No Update Needed.");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("Update Successful!");
      break;
  }
}

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  
  MDNS.begin("nirbhaya");
  server.begin();
  Serial.println("\nSystem Live. Long-press SOS (6s) to trigger Global OTA.");
}

void loop() {
  unsigned long currentTime = millis();

  // 1. SOS & OTA TRIGGER
  if (digitalRead(SWITCH_PIN) == LOW) {
    if (!isBeingPressed) { buttonPressStart = millis(); isBeingPressed = true; }
    else {
      unsigned long holdTime = millis() - buttonPressStart;
      if (holdTime >= 6000) { 
        performGlobalUpdate(); 
        isBeingPressed = false; 
      }
      else if (holdTime >= 3000 && !emergencyActive) { 
        emergencyActive = true; 
      }
    }
  } else { isBeingPressed = false; }

  // 2. SENSORS
  int rawPulse = analogRead(PULSE_PIN);
  if (rawPulse > 2200 && !beatDetected && (currentTime - lastBeatTime > 450)) {
    bpm = 60000 / (currentTime - lastBeatTime);
    lastBeatTime = currentTime;
    beatDetected = true;
  }
  if (rawPulse < 2000) beatDetected = false;

  while (gpsSerial.available() > 0) { gps.encode(gpsSerial.read()); }
  if (gps.location.isValid()) { 
    lastLat = gps.location.lat(); 
    lastLon = gps.location.lng(); 
  }

  float cTemp = ((analogRead(TEMP_PIN) / 4095.0) * 330.0) - 273.15;
  smoothedTemp = (smoothedTemp * 0.98) + (cTemp * 0.02);

  server.handleClient();
}
