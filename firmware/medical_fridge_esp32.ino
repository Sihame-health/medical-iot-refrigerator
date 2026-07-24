/*
 * Smart Medical Refrigerator - ESP32 Firmware
 * -----------------------------------------------
 * Reads temperature (DHT11), door state (reed switch), and content
 * presence (IR obstacle sensor), drives a Peltier cooling module and
 * a servo-controlled door, and publishes/subscribes over MQTT so a
 * Node-RED dashboard can supervise and control the fridge remotely.
 *
 *
 * NOTE: Wi-Fi/MQTT credentials below are placeholders - replace with
 * your own before flashing.
 */

#include <WiFi.h>
#include <PubSubClient.h>   // MQTT
#include <DHT.h>            // Temperature sensor
#include <ESP32Servo.h>     // Door servo

// ---------- Wi-Fi / MQTT configuration ----------
const char* ssid       = "YOUR_WIFI_SSID";
const char* password   = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "YOUR_MQTT_BROKER_IP";

// Reference access points used for Wi-Fi fingerprinting (indoor positioning)
const char* SSID_ROOM_A = "ROOM_A_AP_SSID";
const char* SSID_ROOM_C = "ROOM_C_AP_SSID";

// ---------- Pin mapping ----------
#define DHTPIN         4    // DHT11 temperature/humidity sensor
#define DHTTYPE        DHT11
#define PELTIER_PIN    13   // Peltier module (via MOSFET driver stage)
#define REED_SWITCH    14   // Door state (magnetic reed switch)
#define BUZZER_PIN     33   // Alert buzzer
#define LED_ALARM_PIN  32   // Alert LED
#define SERVO_PIN      15   // Door servo
#define OBSTACLE_PIN   27   // Content presence (IR obstacle sensor)

// ---------- Objects ----------
DHT dht(DHTPIN, DHTTYPE);
Servo doorServo;
WiFiClient espClient;
PubSubClient client(espClient);

// ---------- State ----------
float currentTemp = NAN;
float targetTemp = 22.0;         // Default target (overwritten by mode selection)
float currentHysteresis = 3.0;   // Default hysteresis band
unsigned long lastPublish = 0;
String currentLocation = "Unknown";
unsigned long doorOpenTime = 0;
bool doorTimerRunning = false;
bool obstacleDetected = false;

// ---------- Alarm helpers ----------
void stopAlarm() {
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_ALARM_PIN, LOW);
}

void playPreAlarm() {
  static unsigned long lastBeep = 0;
  static bool state = false;
  if (millis() - lastBeep > 1000) {
    lastBeep = millis();
    state = !state;
    digitalWrite(BUZZER_PIN, state);
    digitalWrite(LED_ALARM_PIN, state);
  }
}

void playCriticalAlarm() {
  static unsigned long lastToggle = 0;
  static bool state = false;
  if (millis() - lastToggle > 150) {
    lastToggle = millis();
    state = !state;
    digitalWrite(BUZZER_PIN, state);
    digitalWrite(LED_ALARM_PIN, state);
  }
}

// ---------- Indoor positioning (Wi-Fi RSSI fingerprinting) ----------
void updateIndoorLocation() {
  int n = WiFi.scanNetworks();
  long rssiA = -100, rssiC = -100;
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == SSID_ROOM_A) rssiA = WiFi.RSSI(i);
    if (WiFi.SSID(i) == SSID_ROOM_C) rssiC = WiFi.RSSI(i);
  }
  if (rssiA == -100 && rssiC == -100) currentLocation = "Unknown";
  else if (rssiA > rssiC) currentLocation = "Room A";
  else currentLocation = "Room C";
  WiFi.scanDelete();
}

// ---------- Door security (open-door alarm) ----------
void handleDoorSecurity(bool doorOpen) {
  if (doorOpen) {
    if (!doorTimerRunning) {
      doorOpenTime = millis();
      doorTimerRunning = true;
    }
    unsigned long duration = millis() - doorOpenTime;
    if (duration > 30000) {
      playCriticalAlarm();
      doorServo.write(0); // force-close after 30s
    } else if (duration > 20000) {
      playPreAlarm();
    }
  } else {
    doorTimerRunning = false;
    stopAlarm();
  }
}

// ---------- MQTT callback (incoming commands) ----------
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  if (String(topic) == "frigo/mode/set") {
    if (msg == "SANG")               { targetTemp = 4.0;  currentHysteresis = 2.0; } // Blood
    else if (msg == "PLAQUETTES")    { targetTemp = 22.0; currentHysteresis = 2.0; } // Platelets
    else if (msg == "VACCINS")       { targetTemp = 5.0;  currentHysteresis = 3.0; } // Vaccines
    else if (msg == "REACTIFS")      { targetTemp = 10.0; currentHysteresis = 2.0; } // Reagents
  }

  if (String(topic) == "frigo/servo/cmd") {
    if (msg == "OPEN")  doorServo.write(180);
    if (msg == "CLOSE") doorServo.write(0);
  }
}

// ---------- Wi-Fi / MQTT connection ----------
void setup_wifi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP32_Medical_Fridge")) {
      client.subscribe("frigo/mode/set");
      client.subscribe("frigo/servo/cmd");
    } else {
      delay(3000);
    }
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);

  pinMode(PELTIER_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_ALARM_PIN, OUTPUT);
  pinMode(REED_SWITCH, INPUT_PULLUP);
  pinMode(OBSTACLE_PIN, INPUT);
  digitalWrite(PELTIER_PIN, LOW);
  stopAlarm();

  doorServo.attach(SERVO_PIN);
  doorServo.write(0);

  dht.begin();
  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

// ---------- Main loop ----------
void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  obstacleDetected = (digitalRead(OBSTACLE_PIN) == LOW);
  bool doorOpen = (digitalRead(REED_SWITCH) == LOW);
  currentTemp = dht.readTemperature();

  handleDoorSecurity(doorOpen);

  float tempMax = targetTemp + currentHysteresis;
  float tempMin = targetTemp - currentHysteresis;

  // Priority 1: no content -> Peltier OFF (energy saving + module lifespan)
  // Priority 2: content present -> hysteresis-based thermal regulation
  if (obstacleDetected && !isnan(currentTemp)) {
    if (currentTemp > tempMax) digitalWrite(PELTIER_PIN, LOW);       // active low -> ON
    else if (currentTemp < tempMin) digitalWrite(PELTIER_PIN, HIGH); // OFF
  } else {
    digitalWrite(PELTIER_PIN, HIGH); // OFF - fridge empty
  }

  if (millis() - lastPublish > 2000) {
    lastPublish = millis();
    updateIndoorLocation();

    Serial.printf("Temp: %.2f | Door: %s\n", currentTemp, doorOpen ? "OPEN" : "CLOSED");

    client.publish("frigo/temp/current", String(currentTemp).c_str());
    client.publish("frigo/door/status", doorOpen ? "OPEN" : "CLOSED");
    client.publish("frigo/obstacle", obstacleDetected ? "PLEIN" : "VIDE");
    client.publish("frigo/peltier/state", digitalRead(PELTIER_PIN) ? "OFF" : "ON");
    client.publish("frigo/location", currentLocation.c_str());
  }
}
