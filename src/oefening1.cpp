// Oefening 1: eigen mesh-node opstarten en loopback-broadcast testen
#include <Arduino.h>
#include <painlessMesh.h>

// Eigen voornaam in de netwerknaam
#define MESH_PREFIX   "KdG_Mesh_Michael"
#define MESH_PASSWORD "meshpassword123"
#define MESH_PORT     5555
#define MESH_CHANNEL  1   // 1, 6 of 11: volgens je docent

#define MIJN_NAAM     "Michael"

Scheduler userScheduler;
painlessMesh mesh;

int berichtTeller = 0;

// Functiedeclaraties
void stuurBroadcast();
void receivedCallback(uint32_t from, String &msg);
void newConnectionCallback(uint32_t nodeId);
void changedConnectionCallback();

// Taak die elke 3 seconden een bericht uitstuurt
Task taskSendMessage(TASK_SECOND * 3, TASK_FOREVER, &stuurBroadcast);

void stuurBroadcast() {
  berichtTeller++;
  String bericht = "Hallo vanuit de node van " + String(MIJN_NAAM) +
                   " (Node ID " + String(mesh.getNodeId()) + ")! Bericht #" +
                   String(berichtTeller);

  // Tweede parameter 'true' (includeSelf) zorgt dat we het bericht ook zelf ontvangen
  mesh.sendBroadcast(bericht, true);
}

void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("[Ontvangen] Van node %u: %s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("[Netwerk] Nieuwe node verbonden: %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.println("[Netwerk] Topologie gewijzigd!");
}

void setup() {
  Serial.begin(115200);

  // Toon enkel foutmeldingen en opstartinfo van painlessMesh
  mesh.setDebugMsgTypes(ERROR | STARTUP);

  // Initialiseer het mesh-netwerk (met kanaal) en koppel de callbacks
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA, MESH_CHANNEL);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);

  // Voeg de zendtaak toe aan de scheduler
  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();

  Serial.printf("Node opgestart met uniek ID: %u\n", mesh.getNodeId());
}

void loop() {
  // Houdt zowel Wi-Fi mesh als alle TaskScheduler-taken draaiende (gebruik nooit delay!)
  mesh.update();
}
