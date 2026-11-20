// Oefening 2: non-blocking LED-indicator (aantal flitsen = aantal nodes)
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

// --- LED-indicator: aantal flitsen = aantal nodes in het mesh ---
#define LED D9 // Ingebouwde LED op de FireBeetle 2 ESP32-E (GPIO 2)
bool ledState = false;

void startStatusBurst();
void blinkStep();

// Elke 6 seconden starten we een knipperreeks die het aantal nodes weergeeft
Task taskStatusBurst(TASK_SECOND * 6, TASK_FOREVER, &startStatusBurst);

// Hulpmiddel voor het knipperen zelf (elke 150 ms wisselen)
Task taskBlink(150 * TASK_MILLISECOND, 0, &blinkStep);

void startStatusBurst() {
  int totaalNodes = (int)mesh.getNodeList().size() + 1;
  Serial.printf("[Topologie] Totaal aantal nodes in netwerk: %d\n", totaalNodes);

  // Zorg dat we altijd vanuit UIT starten
  ledState = false;
  digitalWrite(LED, LOW);

  // Elke flits = 1x AAN + 1x UIT
  taskBlink.setIterations(totaalNodes * 2);
  taskBlink.restart();
}

void blinkStep() {
  ledState = !ledState;
  digitalWrite(LED, ledState);
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

  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

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

  // LED-taken: enkel de burst-taak inschakelen; taskBlink wordt door de burst gestart
  userScheduler.addTask(taskStatusBurst);
  userScheduler.addTask(taskBlink);
  taskStatusBurst.enable();

  Serial.printf("Node opgestart met uniek ID: %u\n", mesh.getNodeId());
}

void loop() {
  // Houdt zowel Wi-Fi mesh als alle TaskScheduler-taken draaiende (gebruik nooit delay!)
  mesh.update();
}
