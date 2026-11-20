// Workshop Wifi-Mesh - Groepsnetwerk - Deel 1
// Opdracht 2: Strategie B (beacon + sendSingle)
// Opdracht 3: uitvaldetectie (VERLOREN- en GEWIJZIGD-regels in de log)
#include <Adafruit_NeoPixel.h>
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
//    Wordt gezet via platformio.ini: env workshop2_base (-D BASE_NODE=1)
//    of env workshop2_mobiel. Zonder vlag is het bordje een mobiele node.
#ifndef BASE_NODE
#define BASE_NODE 0
#endif
const bool IS_BASE_NODE = BASE_NODE;

const int ledPin = D9;    // Ingebouwde LED (GPIO 2)
const int buttonPin = 27; // Ingebouwde drukknop (GPIO 27)

Scheduler userScheduler;
painlessMesh mesh;

// ---------------------------------------------------------------------------
// Status-LED: ingebouwde RGB-LED (WS2812) op GPIO 5
//
// Vaste status (achtergrond):
//   BLAUW   = alleen, zoekt nog naar andere nodes
//   ORANJE  = (mobiel) in het mesh, maar Base nog niet ontdekt
//   GROEN   = alles OK (mobiel: route naar Base; Base: andere nodes verbonden)
//   ROOD    = (mobiel) Base ontdekt maar momenteel onbereikbaar
//
// Korte flitsen (gebeurtenissen):
//   ROOD    2 s   = rechtstreekse verbinding VERLOREN
//   CYAAN   0,5 s = nieuwe rechtstreekse verbinding
//   GEEL    0,3 s = topologie gewijzigd
//   WIT     0,2 s = toggle verstuurd (sendSingle OK)
//   MAGENTA 0,5 s = toggle mislukt (geen route / Base onbekend)
//   PAARS   0,3 s = (Base) toggle-commando ontvangen
// ---------------------------------------------------------------------------
const int statusLedPin = 5;
Adafruit_NeoPixel statusLed(1, statusLedPin, NEO_GRB + NEO_KHZ800);

const uint32_t KLEUR_UIT = 0x000000;
const uint32_t KLEUR_BLAUW = 0x0000FF;
const uint32_t KLEUR_ORANJE = 0xFF5000;
const uint32_t KLEUR_GROEN = 0x00FF00;
const uint32_t KLEUR_ROOD = 0xFF0000;
const uint32_t KLEUR_CYAAN = 0x00FFFF;
const uint32_t KLEUR_GEEL = 0xFFC000;
const uint32_t KLEUR_WIT = 0xFFFFFF;
const uint32_t KLEUR_MAGENTA = 0xFF00FF;
const uint32_t KLEUR_PAARS = 0x8000FF;

uint32_t flitsKleur = KLEUR_UIT;
unsigned long flitsTot = 0; // millis() tot wanneer de flits zichtbaar blijft
uint32_t getoondeKleur = 0xFFFFFFFF; // forceer eerste update

// Toon een gebeurtenis-kleur gedurende 'duurMs'
void statusFlits(uint32_t kleur, unsigned long duurMs) {
  flitsKleur = kleur;
  flitsTot = millis() + duurMs;
}

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

// Berekent elke 50 ms de juiste kleur en werkt de RGB-LED enkel bij als die
// verandert
void updateStatusLed() {
  uint32_t kleur;

  if ((long)(flitsTot - millis()) > 0) {
    kleur = flitsKleur;
  } else {
    bool alleen = mesh.getNodeList().size() == 0;
    if (alleen) {
      kleur = KLEUR_BLAUW;
    } else if (IS_BASE_NODE) {
      kleur = KLEUR_GROEN;
    } else if (baseNodeId == 0) {
      kleur = KLEUR_ORANJE;
    } else if (!mesh.isConnected(baseNodeId)) {
      kleur = KLEUR_ROOD;
    } else {
      kleur = KLEUR_GROEN;
    }
  }

  if (kleur != getoondeKleur) {
    statusLed.setPixelColor(0, kleur);
    statusLed.show();
    getoondeKleur = kleur;
  }
}
Task taskStatusLed(50 * TASK_MILLISECOND, TASK_FOREVER, &updateStatusLed);

void stuurToggleCommando() {
  if (IS_BASE_NODE)
    return; // Node A stuurt geen commando naar zichzelf

  // Opdracht 2 - Strategie B: gericht naar het ontdekte ID van Node A
  if (baseNodeId == 0) {
    Serial.printf("[%6lu ms] [Verzenden] Base nog niet ontdekt, wacht op "
                  "beacon...\n",
                  millis());
    statusFlits(KLEUR_MAGENTA, 500);
    return;
  }

  StaticJsonDocument<128> doc;
  doc["cmd"] = "toggle";

  String jsonBericht;
  serializeJson(doc, jsonBericht);

  bool gelukt = mesh.sendSingle(baseNodeId, jsonBericht);
  Serial.printf("[%6lu ms] [Verzenden] sendSingle naar %u: %s -> %s\n",
                millis(), baseNodeId, jsonBericht.c_str(),
                gelukt ? "OK" : "MISLUKT (geen route)");
  statusFlits(gelukt ? KLEUR_WIT : KLEUR_MAGENTA, gelukt ? 200 : 500);
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
  // Opdracht 2 - Strategie B: Node A maakt zich elke 5 s bekend
  StaticJsonDocument<64> doc;
  doc["role"] = "BASE";

  String jsonBericht;
  serializeJson(doc, jsonBericht);
  mesh.sendBroadcast(jsonBericht, false);
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
  (void)target; // niet meer gebruikt in Strategie B

  // Opdracht 2 - Strategie B: mobiele nodes onthouden het ID van Node A
  if (!IS_BASE_NODE && strcmp(role, "BASE") == 0) {
    if (baseNodeId != from) {
      Serial.printf("[%6lu ms] [Beacon] Base ontdekt! baseNodeId = %u\n",
                    millis(), from);
    }
    baseNodeId = from;
    return;
  }

  // Opdracht 2 - Strategie B: Node A voert het gerichte toggle-commando uit
  if (IS_BASE_NODE && strcmp(cmd, "toggle") == 0) {
    ledAanBase = !ledAanBase;
    digitalWrite(ledPin, ledAanBase ? HIGH : LOW);
    Serial.printf("[%6lu ms] [Actie] Toggle van node %u -> LED %s\n", millis(),
                  from, ledAanBase ? "AAN" : "UIT");
    statusFlits(KLEUR_PAARS, 300);
  }
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf(
      "[%6lu ms] [Netwerk] Nieuwe RECHTSTREEKSE verbinding met node: %u\n",
      millis(), nodeId);
  statusFlits(KLEUR_CYAAN, 500);
}

void droppedConnectionCallback(uint32_t nodeId) {
  Serial.printf(
      "[%6lu ms] [Netwerk] RECHTSTREEKSE verbinding VERLOREN met node: %u\n",
      millis(), nodeId);
  statusFlits(KLEUR_ROOD, 2000);
}

void changedConnectionCallback() {
  // Gaat af bij ELKE wijziging in het mesh (ook bij nodes verderop in de boom)
  Serial.printf("[%6lu ms] [Topologie] GEWIJZIGD: %d nodes | Structuur: %s\n",
                millis(), (int)mesh.getNodeList().size() + 1,
                mesh.subConnectionJson().c_str());
  // Een VERLOREN-flits (rood) niet overschrijven met de kortere gele flits
  if ((long)(flitsTot - millis()) <= 0) {
    statusFlits(KLEUR_GEEL, 300);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  pinMode(buttonPin, INPUT_PULLUP);

  statusLed.begin();
  statusLed.setBrightness(30); // fel genoeg, en spaart de batterij
  statusLed.clear();
  statusLed.show();

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
  userScheduler.addTask(taskStatusLed);

  taskButton.enable();
  taskStatusLed.enable();
  taskTopology.enable();
  if (IS_BASE_NODE)
    taskBeacon.enable(); // enkel Node A zendt de beacon

  Serial.printf("Groepsnode gestart! Mijn Node ID: %u (Base: %s)\n",
                mesh.getNodeId(), IS_BASE_NODE ? "JA" : "NEE");
}

void loop() { mesh.update(); }
