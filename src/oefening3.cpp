#include <Arduino.h>
#include <painlessMesh.h>
#include <ArduinoJson.h>

#define MESH_PREFIX   "KdG_Mesh_Michael"
#define MESH_PASSWORD "meshpassword123"
#define MESH_PORT     5555
#define MESH_CHANNEL  1   // 1, 6 of 11: volgens je docent

#define LED D9            // Ingebouwde LED (GPIO 2)
const int buttonPin = 27; // Ingebouwde drukknop op FireBeetle 2 ESP32-E (GPIO 27)

Scheduler userScheduler;
painlessMesh mesh;

bool ledAan = false;              // Huidige LED-status (bijgewerkt bij ontvangst)
int vorigeKnopStatus = HIGH;      // Knop is actief LAAG, dus in rust HIGH

void stuurLedCommando(uint32_t doelNodeId, bool zetAan) {
  StaticJsonDocument<128> doc;
  doc["target"] = doelNodeId;
  doc["led"] = zetAan ? 1 : 0;

  String jsonBericht;
  serializeJson(doc, jsonBericht);

  Serial.printf("[Verzenden] JSON-pakket: %s\n", jsonBericht.c_str());
  mesh.sendBroadcast(jsonBericht, true);
}

void receivedCallback(uint32_t from, String &msg) {
  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, msg);

  if (error) {
    Serial.printf("[Ontvangen] Geen geldig JSON-bericht van %u: %s\n", from, msg.c_str());
    return;
  }

  uint32_t targetId = doc["target"];
  int ledWaarde = doc["led"];

  // Voer het commando enkel uit als het voor mijn Node ID bestemd is
  if (targetId == mesh.getNodeId()) {
    ledAan = (ledWaarde == 1);
    digitalWrite(LED, ledAan ? HIGH : LOW);
    Serial.printf("[Actie] LED op GPIO 2 gezet op: %s\n", ledAan ? "AAN" : "UIT");
  }
}

// Controleert elke 20 ms de knop; reageert enkel op de overgang HIGH -> LOW
void checkButton() {
  int knopStatus = digitalRead(buttonPin);

  if (vorigeKnopStatus == HIGH && knopStatus == LOW) {
    bool nieuweStatus = !ledAan;
    Serial.printf("[Knop] Ingedrukt -> LED %s\n", nieuweStatus ? "AAN" : "UIT");
    stuurLedCommando(mesh.getNodeId(), nieuweStatus);
  }

  vorigeKnopStatus = knopStatus;
}

Task taskButton(20 * TASK_MILLISECOND, TASK_FOREVER, &checkButton);

void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  pinMode(buttonPin, INPUT_PULLUP); // Knop is actief LAAG (LOW bij indrukken)

  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA, MESH_CHANNEL);
  mesh.onReceive(&receivedCallback);

  userScheduler.addTask(taskButton);
  taskButton.enable();

  Serial.printf("Node opgestart met uniek ID: %u\n", mesh.getNodeId());
  Serial.println("Typ '1' (LED AAN) of '0' (LED UIT) in de seriële monitor, of druk op de knop...");
}

void loop() {
  mesh.update();

  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '1') {
      stuurLedCommando(mesh.getNodeId(), true);
    } else if (c == '0') {
      stuurLedCommando(mesh.getNodeId(), false);
    }
  }
}
