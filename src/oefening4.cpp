// Oefening 4: LED binnen schakelen met een knop buiten, via meerdere mesh-nodes
//
// Eén bestand, drie rollen (gekozen via build_flags in platformio.ini):
//   ROL_BUITEN : persoon buiten, knop op GPIO 27 stuurt een "toggle"-commando.
//                De eigen LED toont de status van de LED binnen (bevestiging).
//   ROL_RELAY  : tussenliggende node, doet niets behalve het mesh doorgeven.
//                De LED flitst elke 6 s het aantal nodes in het mesh.
//   ROL_BINNEN : persoon binnen, schakelt de LED om en stuurt de status terug.
//
// painlessMesh stuurt berichten automatisch over meerdere hops door,
// dus de relay-nodes hebben geen eigen doorstuurcode nodig.

#include <Arduino.h>
#include <painlessMesh.h>
#include <ArduinoJson.h>

#if !defined(ROL_BUITEN) && !defined(ROL_RELAY) && !defined(ROL_BINNEN)
#error "Kies een rol: -D ROL_BUITEN, -D ROL_RELAY of -D ROL_BINNEN"
#endif

// Alle 4 de nodes MOETEN dezelfde naam, wachtwoord en kanaal hebben
#define MESH_PREFIX   "KdG_Mesh_Groep"
#define MESH_PASSWORD "meshpassword123"
#define MESH_PORT     5555
#define MESH_CHANNEL  1   // 1, 6 of 11: volgens je docent

#define LED D9            // Ingebouwde LED (GPIO 2)
const int buttonPin = 27; // Ingebouwde drukknop (GPIO 27)

Scheduler userScheduler;
painlessMesh mesh;

bool ledAan = false;

// ---------------------------------------------------------------------------
// Berichten
//   {"cmd":"toggle"}           buiten -> binnen
//   {"cmd":"status","led":1}   binnen -> buiten (bevestiging)
// ---------------------------------------------------------------------------
void stuurJson(const JsonDocument &doc) {
  String bericht;
  serializeJson(doc, bericht);
  Serial.printf("[Verzenden] %s\n", bericht.c_str());
  mesh.sendBroadcast(bericht);
}

void receivedCallback(uint32_t from, String &msg) {
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, msg)) {
    Serial.printf("[Ontvangen] Ongeldig bericht van %u: %s\n", from, msg.c_str());
    return;
  }

  const char *cmd = doc["cmd"] | "";
  Serial.printf("[Ontvangen] Van %u: %s\n", from, msg.c_str());

#ifdef ROL_BINNEN
  if (strcmp(cmd, "toggle") == 0) {
    ledAan = !ledAan;
    digitalWrite(LED, ledAan ? HIGH : LOW);
    Serial.printf("[Actie] LED %s\n", ledAan ? "AAN" : "UIT");

    // Stuur de nieuwe status terug zodat buiten weet dat het gelukt is
    StaticJsonDocument<64> antwoord;
    antwoord["cmd"] = "status";
    antwoord["led"] = ledAan ? 1 : 0;
    stuurJson(antwoord);
  }
#endif

#ifdef ROL_BUITEN
  if (strcmp(cmd, "status") == 0) {
    ledAan = doc["led"] == 1;
    digitalWrite(LED, ledAan ? HIGH : LOW);
    Serial.printf("[Bevestiging] LED binnen is %s\n", ledAan ? "AAN" : "UIT");
  }
#endif
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("[Netwerk] Nieuwe node verbonden: %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.printf("[Netwerk] Topologie gewijzigd, %u nodes in mesh\n",
                (unsigned)mesh.getNodeList().size() + 1);
}

// ---------------------------------------------------------------------------
// Knop (enkel buiten): elke 20 ms pollen, reageren op HIGH -> LOW
// ---------------------------------------------------------------------------
#ifdef ROL_BUITEN
int vorigeKnopStatus = HIGH;
unsigned long laatsteDruk = 0;

void checkButton() {
  int knopStatus = digitalRead(buttonPin);

  if (vorigeKnopStatus == HIGH && knopStatus == LOW && millis() - laatsteDruk > 200) {
    laatsteDruk = millis();
    Serial.println("[Knop] Ingedrukt");
    StaticJsonDocument<64> doc;
    doc["cmd"] = "toggle";
    stuurJson(doc);
  }
  vorigeKnopStatus = knopStatus;
}

Task taskButton(20 * TASK_MILLISECOND, TASK_FOREVER, &checkButton);
#endif

// ---------------------------------------------------------------------------
// Node-teller op de LED (enkel relay), zoals in oefening 2
// ---------------------------------------------------------------------------
#ifdef ROL_RELAY
bool ledState = false;
void blinkStep() {
  ledState = !ledState;
  digitalWrite(LED, ledState);
}
Task taskBlink(150 * TASK_MILLISECOND, 0, &blinkStep);

void startStatusBurst() {
  int totaalNodes = (int)mesh.getNodeList().size() + 1;
  Serial.printf("[Topologie] Totaal aantal nodes in netwerk: %d\n", totaalNodes);
  ledState = false;
  digitalWrite(LED, LOW);
  taskBlink.setIterations(totaalNodes * 2);
  taskBlink.restart();
}
Task taskStatusBurst(TASK_SECOND * 6, TASK_FOREVER, &startStatusBurst);
#endif

void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  pinMode(buttonPin, INPUT_PULLUP);

  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA, MESH_CHANNEL);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);

#ifdef ROL_BUITEN
  userScheduler.addTask(taskButton);
  taskButton.enable();
  const char *rol = "BUITEN";
#elif defined(ROL_BINNEN)
  const char *rol = "BINNEN";
#else
  userScheduler.addTask(taskStatusBurst);
  userScheduler.addTask(taskBlink);
  taskStatusBurst.enable();
  const char *rol = "RELAY";
#endif

  Serial.printf("Node opgestart als %s, ID: %u\n", rol, mesh.getNodeId());
}

void loop() {
  mesh.update();
}
