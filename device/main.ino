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

#define DHTTYPE DHT22
DHT dht(DHT_PIN, DHTTYPE);

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastSendTime = 0;
const unsigned long interval = 5000;
const float LUX_THRESHOLD = 50.0;
bool ledState = false;

bool manualControl = false;
unsigned long manualTimeout = 0;
const unsigned long MANUAL_TIMEOUT = 30000;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Ket noi WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi OK!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ WiFi FAIL!");
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char jsonStr[256];
  if (length >= 256) length = 255;
  memcpy(jsonStr, payload, length);
  jsonStr[length] = '\0';

  Serial.print("📩 Nhận lệnh RPC từ topic [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(jsonStr);

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, jsonStr);
  if (error) {
    Serial.println("❌ Lỗi parse JSON");
    return;
  }

  const char* method = doc["method"];
  if (method == nullptr) return;

  String topicStr = String(topic);
  int lastSlash = topicStr.lastIndexOf('/');
  String requestId = (lastSlash > 0) ? topicStr.substring(lastSlash + 1) : "0";

  if (strcmp(method, "setLedState") == 0) {
    bool state = doc["params"]["state"] | false;
    ledState = state;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    manualControl = true;
    manualTimeout = millis();

    String respTopic = "v1/devices/me/rpc/response/" + requestId;
    StaticJsonDocument<64> respDoc;
    respDoc["result"] = "ok";
    char respBuffer[64];
    serializeJson(respDoc, respBuffer);
    client.publish(respTopic.c_str(), respBuffer);

    Serial.print("💡 LED đã ");
    Serial.println(ledState ? "BẬT (thủ công)" : "TẮT (thủ công)");
  }
  else if (strcmp(method, "setLedMode") == 0) {
    const char* mode = doc["params"]["mode"];
    if (mode && strcmp(mode, "auto") == 0) {
      manualControl = false;
      Serial.println("🔄 Chuyển sang chế độ TỰ ĐỘNG");
      String respTopic = "v1/devices/me/rpc/response/" + requestId;
      client.publish(respTopic.c_str(), "{\"result\":\"auto mode activated\"}");
    }
  }
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("MQTT connecting... ");
    
    String clientId = "ESP32_";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str(), access_token, "")) {
      Serial.println("✅ CONNECTED!");
      client.subscribe("v1/devices/me/rpc/request/+");
      Serial.println("📡 Đã subscribe RPC");
    } else {
      Serial.print("❌ FAILED (rc=");
      Serial.print(client.state());
      Serial.println(")");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  dht.begin();
  Serial.println("DHT22 da san sang!");
  delay(1000);

  setup_wifi();
  client.setServer(thingsboard_server, mqtt_port);
  client.setCallback(mqttCallback);
  
  Serial.println("Test LED tich hop...");
  digitalWrite(LED_PIN, HIGH);
  delay(300);
  digitalWrite(LED_PIN, LOW);
  delay(300);
  digitalWrite(LED_PIN, HIGH);
  delay(300);
  digitalWrite(LED_PIN, LOW);
  
  Serial.println("--- HE THONG VUON THONG MINH (co dieu khien tu xa) ---\n");
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  int lightRaw = analogRead(LDR_PIN);
  float lux = 0.0;

  if (lightRaw >= 4050) {
    lux = 0.0;
  } 
  else if (lightRaw <= 50) {
    lux = 500.0;
  } 
  else {
    float voltage = lightRaw * (3.3 / 4095.0);
    if (voltage < 0.01) voltage = 0.01;
    float ldrResistance = (3.3 - voltage) * 10000 / voltage;
    if (ldrResistance > 0) {
      lux = pow(500000 / ldrResistance, 1.4);
    } else {
      lux = 500.0;
    }
  }

  if (!manualControl) {
    if (lux < LUX_THRESHOLD) {
      ledState = true;
      digitalWrite(LED_PIN, HIGH);
    } else {
      ledState = false;
      digitalWrite(LED_PIN, LOW);
    }
  }
  else if (manualControl && (millis() - manualTimeout > MANUAL_TIMEOUT)) {
    manualControl = false;
    Serial.println("⏰ Hết thời gian chờ, quay lại chế độ tự động");
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastSendTime >= interval) {
    lastSendTime = currentMillis;

    float humidityAir = dht.readHumidity();
    float temperature = dht.readTemperature();

    int soilRaw = analogRead(SOIL_PIN);
    float soilPercent = 100.0 - (soilRaw / 4095.0) * 100.0;

    StaticJsonDocument<256> jsonDoc;
    
    if (!isnan(humidityAir) && !isnan(temperature)) {
      jsonDoc["temperature"] = temperature;
      jsonDoc["humidity"] = humidityAir;
    }
    
    jsonDoc["lightRaw"] = lightRaw;
    jsonDoc["lux"] = lux;
    jsonDoc["soilRaw"] = soilRaw;
    jsonDoc["soilPercent"] = soilPercent;
    jsonDoc["ledState"] = ledState ? 1 : 0;
    jsonDoc["manualControl"] = manualControl ? 1 : 0;

    char telemetryPayload[256];
    serializeJson(jsonDoc, telemetryPayload);

    if (client.connected()) {
      if (client.publish("v1/devices/me/telemetry", telemetryPayload)) {
        Serial.println("✅ DATA SENT!");
      } else {
        Serial.println("❌ PUBLISH FAILED!");
      }
    }

    Serial.println("----------- THONG SO -----------");
    Serial.print("💡 Anh sang: ");
    Serial.print(lux);
    Serial.println(" Lux");
    
    if (!isnan(temperature) && !isnan(humidityAir)) {
      Serial.print("🌡️  Nhiet do: ");
      Serial.print(temperature);
      Serial.println(" °C");
      Serial.print("💧 Do am KK: ");
      Serial.print(humidityAir);
      Serial.println(" %");
    } else {
      Serial.println("⚠️  DHT22: Loi doc du lieu!");
    }
    
    Serial.print("🌱 Do am dat: ");
    Serial.print(soilPercent);
    Serial.println(" %");
    
    Serial.print("💡 LED: ");
    Serial.print(ledState ? "BAT" : "TAT");
    if (manualControl) {
      Serial.print(" (Điều khiển tay)");
    } else {
      Serial.print(" (Tự động)");
    }
    Serial.println();
    
    if (soilPercent < 30) {
      Serial.println("⚠️  CANH BAO: DAT DANG KHO! CAN TUOI NUOC!");
    }
    
    if (soilRaw > 3500) {
      Serial.println("🚨 BAO DONG: DAT RAT KHO (SoilRaw > 3500)!");
    }
    
    Serial.println("--------------------------------\n");
  }
  
  delay(100);
}
