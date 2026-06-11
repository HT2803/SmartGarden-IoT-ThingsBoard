#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

const char* ssid = "Hoa";
const char* pass = "12345678";

const char* thingsboard_server = "thingsboard.cloud";
const int mqtt_port = 1883;
const char* access_token = "HWNXTu7adTvVtFd5TjO6";

#define LDR_PIN 34
#define SOIL_PIN 32
#define DHT_PIN 4
#define LED_PIN 2
#define PUMP_PIN 18
#define MIST_PIN 27

#define DHTTYPE DHT22
DHT dht(DHT_PIN, DHTTYPE);

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastSendTime = 0;
const unsigned long interval = 5000;
const int LIGHT_THRESHOLD = 2500;

bool ledState  = false;
bool pumpState = false;
bool mistState = false;
bool autoMode  = true;

float g_temperature = NAN;
float g_humidity    = NAN;
int   g_lightRaw    = 0;
int   g_soilRaw     = 0;

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, pass);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    attempts++;
  }
}

void sendTelemetryNow() {
  StaticJsonDocument<256> jsonDoc;
  if (!isnan(g_humidity) && !isnan(g_temperature)) {
    jsonDoc["temperature"] = g_temperature;
    jsonDoc["humidity"]    = g_humidity;
  }
  jsonDoc["lightRaw"]  = g_lightRaw;
  jsonDoc["soilRaw"]   = g_soilRaw;
  jsonDoc["ledState"]  = ledState  ? 1 : 0;
  jsonDoc["pumpState"] = pumpState ? 1 : 0;
  jsonDoc["mistState"] = mistState ? 1 : 0;
  jsonDoc["autoState"] = autoMode  ? 1 : 0;

  char telemetryPayload[256];
  serializeJson(jsonDoc, telemetryPayload);
  if (client.connected()) {
    client.publish("v1/devices/me/telemetry", telemetryPayload);
  }
}

bool parseParamsBool(JsonVariant params) {
  if (params.is<bool>()) return params.as<bool>();
  return params["state"] | false;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char jsonStr[256];
  if (length >= 256) length = 255;
  memcpy(jsonStr, payload, length);
  jsonStr[length] = '\0';

  Serial.print("📩 Lệnh nhận: ");
  Serial.println(jsonStr);

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, jsonStr);
  if (error) return;

  const char* method = doc["method"];
  if (method == nullptr) return;

  String topicStr  = String(topic);
  int lastSlash    = topicStr.lastIndexOf('/');
  String requestId = (lastSlash > 0) ? topicStr.substring(lastSlash + 1) : "0";

  if (strcmp(method, "setLedState") == 0) {
    ledState = parseParamsBool(doc["params"]);
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);   // active HIGH
    autoMode = false;
  }
  else if (strcmp(method, "setPumpState") == 0) {
    pumpState = parseParamsBool(doc["params"]);
    digitalWrite(PUMP_PIN, pumpState ? HIGH : LOW); // ✅ đổi: active HIGH
    autoMode = false;
  }
  else if (strcmp(method, "setMistState") == 0) {
    mistState = parseParamsBool(doc["params"]);
    digitalWrite(MIST_PIN, mistState ? HIGH : LOW); // ✅ đổi: active HIGH
    autoMode = false;
  }
  else if (strcmp(method, "setAutoState") == 0) {
    autoMode = parseParamsBool(doc["params"]);
    Serial.print("🔄 Chế độ tự động: ");
    Serial.println(autoMode ? "BẬT" : "TẮT");
  }

  sendTelemetryNow();

  String respTopic = "v1/devices/me/rpc/response/" + requestId;
  client.publish(respTopic.c_str(), "{\"result\":\"ok\"}");
}

void reconnectMQTT() {
  while (!client.connected()) {
    String clientId = "ESP32_";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), access_token, "")) {
      client.subscribe("v1/devices/me/rpc/request/+");
    } else {
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN,  OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(MIST_PIN, OUTPUT);

  digitalWrite(LED_PIN,  LOW);
  digitalWrite(PUMP_PIN, LOW);  // ✅ đổi: active HIGH → LOW = tắt khi khởi động
  digitalWrite(MIST_PIN, LOW);  // ✅ đổi: active HIGH → LOW = tắt khi khởi động

  dht.begin();
  setup_wifi();
  client.setServer(thingsboard_server, mqtt_port);
  client.setCallback(mqttCallback);
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  g_lightRaw    = analogRead(LDR_PIN);
  g_soilRaw     = analogRead(SOIL_PIN);
  g_humidity    = dht.readHumidity();
  g_temperature = dht.readTemperature();

  if (autoMode) {
    // --- Đèn ---
    if (g_lightRaw > LIGHT_THRESHOLD) {
      if (!ledState) { ledState = true;  digitalWrite(LED_PIN, HIGH); }
    } else {
      if (ledState)  { ledState = false; digitalWrite(LED_PIN, LOW);  }
    }

    // --- Bơm (hysteresis 2500–3000) ---
    if (g_soilRaw > 3000) {
      if (!pumpState) { pumpState = true;  digitalWrite(PUMP_PIN, HIGH); } // ✅ đổi
    } else if (g_soilRaw < 2500) {
      if (pumpState)  { pumpState = false; digitalWrite(PUMP_PIN, LOW);  } // ✅ đổi
    }
    // Vùng 2500–3000: giữ nguyên

    // --- Phun sương ---
    bool newMist = (!isnan(g_temperature) && g_temperature > 32.0);
    if (newMist != mistState) {
      mistState = newMist;
      digitalWrite(MIST_PIN, mistState ? HIGH : LOW); // ✅ đổi
    }
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastSendTime >= interval) {
    lastSendTime = currentMillis;
    sendTelemetryNow();

    Serial.println("----------- THONG SO -----------");
    Serial.print("💡 Anh sang  : "); Serial.println(g_lightRaw);
    if (!isnan(g_temperature)) {
      Serial.print("🌡️  Nhiet do  : "); Serial.print(g_temperature); Serial.println(" °C");
      Serial.print("💧 Do am KK  : "); Serial.print(g_humidity);    Serial.println(" %");
    }
    Serial.print("🌱 Do am dat : "); Serial.println(g_soilRaw);
    Serial.print("💡 ĐÈN       : "); Serial.println(ledState  ? "BAT" : "TAT");
    Serial.print("🔌 MÁY BƠM   : "); Serial.println(pumpState ? "BAT" : "TAT");
    Serial.print("💨 PHUN SUONG: "); Serial.println(mistState ? "BAT" : "TAT");
    Serial.println(autoMode ? "Che do: TU DONG" : "Che do: TAY");
    Serial.println("--------------------------------\n");
  }

  delay(100);
}
