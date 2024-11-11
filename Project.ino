#include <WiFiNINA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Traffic light and IR sensor pin definitions
int redA = 2, yellowA = 3, greenA = 4;
int redB = 5, yellowB = 6, greenB = 7;
int IR1_A = 8, IR2_A = 9, IR3_A = 10;
int IR1_B = 11, IR2_B = 12, IR3_B = 13;

// Timing settings (in milliseconds)
unsigned long maxGreenTime = 45000;
unsigned long yellowTime = 5000;
unsigned long lastSwitchTime = 0;
bool laneAActive = false;
bool smartMode = true;  // Start in smart mode

// WiFi and MQTT settings
const char* ssid = "Vivo";
const char* password = "abcdefgh";
const char* mqtt_server = "broker.emqx.io";
const char* MQTT_TOPIC = "NanoTrafficCommand";

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// Fault tolerance settings
unsigned long lastMQTTReconnectAttempt = 0;
unsigned long lastWiFiCheckTime = 0;
const unsigned long reconnectInterval = 5000; // Interval for Wi-Fi checks
bool backupMode = false;

// Systematic mode timing (in milliseconds)
unsigned long systematicGreenTime = 20000; // Each lane gets 20 seconds
unsigned long systematicLastSwitchTime = 0;

void setup() {
  Serial.begin(9600);

  // Set up LED and sensor pins
  pinMode(redA, OUTPUT); pinMode(yellowA, OUTPUT); pinMode(greenA, OUTPUT);
  pinMode(redB, OUTPUT); pinMode(yellowB, OUTPUT); pinMode(greenB, OUTPUT);
  pinMode(IR1_A, INPUT); pinMode(IR2_A, INPUT); pinMode(IR3_A, INPUT);
  pinMode(IR1_B, INPUT); pinMode(IR2_B, INPUT); pinMode(IR3_B, INPUT);

  // Initialize with Lane B green and Lane A red
  digitalWrite(greenB, HIGH);  
  digitalWrite(redA, HIGH);
  laneAActive = false;
  lastSwitchTime = millis();

  // Attempt to connect to WiFi and MQTT
  connectWiFi();
  connectMQTT();
}

void loop() {
  if (backupMode) {
    runSystematicMode();
    checkWiFiReconnect(); // Keep checking if Wi-Fi becomes available
  } else {
    if (WiFi.status() != WL_CONNECTED) {
      backupMode = true; // Immediately switch to backup if Wi-Fi disconnects
      Serial.println("Wi-Fi disconnected. Switching to backup mode.");
    } else {
      client.loop(); // Handle MQTT messages if connected
    }
    
    if (smartMode && !backupMode) {
      runSmartMode();
    }
  }
}

void connectWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startTime > 3000) { // Wait for 3 seconds max
      Serial.println("\nFailed to connect. Switching to backup mode.");
      backupMode = true;
      return;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  backupMode = false; // Reset backup mode if connected
}

void connectMQTT() {
  client.setServer(mqtt_server, 1883);
  client.setCallback(onMessageReceived);

  while (!client.connected() && !backupMode) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("NanoTrafficController")) {
      Serial.println("Connected to MQTT Broker!");
      client.subscribe(MQTT_TOPIC);
    } else {
      Serial.print("Failed with state ");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

void checkWiFiReconnect() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastWiFiCheckTime > reconnectInterval) {
    lastWiFiCheckTime = currentMillis;
    Serial.println("Attempting to reconnect to WiFi...");
    connectWiFi();

    if (WiFi.status() == WL_CONNECTED) {
      connectMQTT(); // Also try to reconnect to MQTT
      if (client.connected()) {
        Serial.println("Wi-Fi and MQTT reconnected. Switching to smart mode.");
        backupMode = false;
        smartMode = true;
      }
    }
  }
}

void onMessageReceived(char* topic, byte* payload, unsigned int length) {
  char message[length + 1];
  strncpy(message, (char*)payload, length);
  message[length] = '\0';

  DynamicJsonDocument doc(512);
  deserializeJson(doc, message);

  if (doc["mode"] == "smart") {
    smartMode = true;
    backupMode = false;
  } else if (doc["mode"] == "manual") {
    smartMode = false;
    backupMode = false;
    String lane = doc["lane"];
    if (lane == "Lane A") {
      switchToLaneA();
    } else if (lane == "Lane B") {
      switchToLaneB();
    }
  }
}

void runSmartMode() {
  unsigned long currentTime = millis();
  int countA = digitalRead(IR1_A) + digitalRead(IR2_A) + digitalRead(IR3_A);
  int countB = digitalRead(IR1_B) + digitalRead(IR2_B) + digitalRead(IR3_B);

  if (countA == 3 && !laneAActive) {
    switchToLaneA();
    lastSwitchTime = millis();
  } else if (countB > countA && laneAActive) {
    switchToLaneB();
    lastSwitchTime = millis();
  }
}

void runSystematicMode() {
  unsigned long currentTime = millis();
  if (currentTime - systematicLastSwitchTime >= systematicGreenTime) {
    if (laneAActive) {
      switchToLaneB();
    } else {
      switchToLaneA();
    }
    systematicLastSwitchTime = currentTime;
  }
}

void switchToLaneA() {
  digitalWrite(greenB, LOW);
  digitalWrite(yellowB, HIGH);
  delay(yellowTime);
  digitalWrite(yellowB, LOW); digitalWrite(redB, HIGH);
  digitalWrite(redA, LOW); digitalWrite(greenA, HIGH);
  laneAActive = true;
}

void switchToLaneB() {
  digitalWrite(greenA, LOW);
  digitalWrite(yellowA, HIGH);
  delay(yellowTime);
  digitalWrite(yellowA, LOW); digitalWrite(redA, HIGH);
  digitalWrite(redB, LOW); digitalWrite(greenB, HIGH);
  laneAActive = false;
}
