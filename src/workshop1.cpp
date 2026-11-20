// Workshop Wifi-Mesh - Groepsnetwerk - Deel 1
// Opdracht 1: Strategie A (rolnaam in JSON + broadcast)
#include <Arduino.h>
#include <ArduinoJson.h>
#include <painlessMesh.h>

// 1. Kies samen één unieke groepsnaam en gebruik het kanaal van je docent!
//    Beide moeten op ALLE boards van de groep identiek zijn.
#define MESH_PREFIX "KdG_Mesh_Groep6"
#define MESH_PASSWORD "meshpassword123"
#define MESH_PORT 5555
#define MESH_CHANNEL 6 // 1, 6 of 11

// 2. 'true' voor Node A (Base in het lokaal), 'false' voor Nodes B, C en D.
//    Wordt gezet via platformio.ini: env workshop1_base (-D BASE_NODE=1)
//    of env workshop1_mobiel. Zonder vlag is het bordje een mobiele node.
#ifndef BASE_NODE
#define BASE_NODE 0
#endif
const bool IS_BASE_NODE = BASE_NODE;

const int ledPin = D9;    // Ingebouwde LED (GPIO 2)
const int buttonPin = 27; // Ingebouwde drukknop (GPIO 27)

Scheduler userScheduler;
painlessMesh mesh;

bool ledAanBase = false; // LED-stand op Node A (enkel daar gebruikt)
bool vorigeKnopStatus = HIGH;
bool burstLedState = false;
uint32_t baseNodeId = 0; // Wordt gebruikt in Strategie B

// Functiedeclaraties
void checkButton();
void printTopology();
void blinkStep();
void stuurBeacon();
void receivedCallback(uint32_t from, String &msg);
void newConnectionCallback(uint32_t nodeId);
void droppedConnectionCallback(uint32_t nodeId);
void changedConnectionCallback();

// Taken
Task taskButton(20 * TASK_MILLISECOND, TASK_FOREVER, &checkButton);
Task taskTopology(TASK_SECOND * 5, TASK_FOREVER, &printTopology);
Task taskBlink(150 * TASK_MILLISECOND, 0, &blinkStep);
Task taskBeacon(TASK_SECOND * 5, TASK_FOREVER,
                &stuurBeacon); // enkel actief op Node A

void stuurToggleCommando() {
  if (IS_BASE_NODE)
    return; // Node A stuurt geen commando naar zichzelf

  StaticJsonDocument<128> doc;

  // Opdracht 1 - Strategie A: rolnaam als doel, broadcast naar iedereen
  doc["target"] = "BASE";
  doc["cmd"] = "toggle";

  String jsonBericht;
  serializeJson(doc, jsonBericht);
  Serial.printf("[%6lu ms] [Verzenden] Broadcast: %s\n", millis(),
                jsonBericht.c_str());
  mesh.sendBroadcast(jsonBericht, false);

  // TODO (Opdracht 2 - Strategie B): controleer of baseNodeId != 0, zet enkel
  // doc["cmd"] = "toggle";
  //                                  stuur met mesh.sendSingle(baseNodeId, ...)
  //                                  en print of het gelukt is
}

void checkButton() {
  bool knopStatus = digitalRead(buttonPin);

  // Reageer enkel op de overgang HIGH -> LOW (het moment van indrukken)
  if (vorigeKnopStatus == HIGH && knopStatus == LOW) {
    stuurToggleCommando();
  }
  vorigeKnopStatus = knopStatus;
}

void printTopology() {
  int totaalNodes = (int)mesh.getNodeList().size() + 1;
  Serial.printf("[%6lu ms] [Topologie] Actieve nodes: %d | Structuur: %s\n",
                millis(), totaalNodes, mesh.subConnectionJson().c_str());

  // Mobiele nodes tonen via hun blauwe LED hoeveel nodes in het mesh zitten
  // (aantal flitsen = aantal nodes). Node A houdt zijn LED vrij voor het
  // commando van buiten!
  if (!IS_BASE_NODE) {
    burstLedState = false;
    digitalWrite(ledPin, LOW);
    taskBlink.setIterations(totaalNodes * 2);
    taskBlink.restart();
  }
}

void stuurBeacon() {
  // TODO (Opdracht 2 - Strategie B): broadcast hier {"role":"BASE"} (zonder
  // includeSelf)
}

void blinkStep() {
  burstLedState = !burstLedState;
  digitalWrite(ledPin, burstLedState);
}

void receivedCallback(uint32_t from, String &msg) {
  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, msg);
  if (error)
    return;

  const char *role = doc["role"] | "";
  const char *cmd = doc["cmd"] | "";
  const char *target = doc["target"] | "";
  (void)role; // nog niet gebruikt in Strategie A

  // Opdracht 1 - Strategie A: enkel Node A voert
  // {"target":"BASE","cmd":"toggle"} uit
  if (strcmp(target, "BASE") == 0 && strcmp(cmd, "toggle") == 0) {
    if (IS_BASE_NODE) {
      ledAanBase = !ledAanBase;
      digitalWrite(ledPin, ledAanBase ? HIGH : LOW);
      Serial.printf("[%6lu ms] [Actie] Toggle van node %u -> LED %s\n",
                    millis(), from, ledAanBase ? "AAN" : "UIT");
    } else {
      Serial.printf("[%6lu ms] [Ontvangen] Toggle van node %u genegeerd (ik "
                    "ben geen BASE)\n",
                    millis(), from);
    }
  }

  // TODO (Opdracht 2 - Strategie B): Laat mobiele nodes {"role":"BASE"}
  // onthouden (baseNodeId = from)
  //                                  en laat Node A het gerichte
  //                                  {"cmd":"toggle"} uitvoeren
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf(
      "[%6lu ms] [Netwerk] Nieuwe RECHTSTREEKSE verbinding met node: %u\n",
      millis(), nodeId);
}

void droppedConnectionCallback(uint32_t nodeId) {
  Serial.printf(
      "[%6lu ms] [Netwerk] RECHTSTREEKSE verbinding VERLOREN met node: %u\n",
      millis(), nodeId);
}

void changedConnectionCallback() {
  // Gaat af bij ELKE wijziging in het mesh (ook bij nodes verderop in de boom)
  Serial.printf("[%6lu ms] [Topologie] GEWIJZIGD: %d nodes | Structuur: %s\n",
                millis(), (int)mesh.getNodeList().size() + 1,
                mesh.subConnectionJson().c_str());
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  pinMode(buttonPin, INPUT_PULLUP);

  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA,
            MESH_CHANNEL);

  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onDroppedConnection(&droppedConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);

  userScheduler.addTask(taskButton);
  userScheduler.addTask(taskTopology);
  userScheduler.addTask(taskBlink);
  userScheduler.addTask(taskBeacon);

  taskButton.enable();
  taskTopology.enable();
  if (IS_BASE_NODE)
    taskBeacon.enable(); // enkel Node A zendt de beacon

  Serial.printf("Groepsnode gestart! Mijn Node ID: %u (Base: %s)\n",
                mesh.getNodeId(), IS_BASE_NODE ? "JA" : "NEE");
}

void loop() { mesh.update(); }
