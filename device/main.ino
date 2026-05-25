#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

const char* ssid = "Hoa";         
const char* pass = "12345678";    

const char* thingsboard_server = "thingsboard.cloud"; 
const char* access_token = "HWNXTu7adTvVtFd5TjO6"; 

#define LDR_PIN 34    
#define SOIL_PIN 32   
#define DHT_PIN 4     
#define LED_PIN 18    

#define DHTTYPE DHT22 
DHT dht(DHT_PIN, DHTTYPE);

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastSendTime = 0;
const unsigned long interval = 5000; 

const float LUX_THRESHOLD = 50.0;
bool ledState = false;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Dang ket noi den mang WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi da ket noi thanh cong!");
  Serial.print("Dia chi IP cua ESP32: ");
  Serial.println(WiFi.localIP());
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Dang ket noi den ThingsBoard MQTT Broker...");
    if (client.connect("ESP32_SmartGarden_Mach_That", access_token, NULL)) {
      Serial.println("KẾT NỐI THANH CÔNG!");
    } else {
      Serial.print("That bai, ma loi rc=");
      Serial.print(client.state());
      Serial.println(". Thu lai sau 5 giay...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  dht.begin();

  setup_wifi();
  client.setServer(thingsboard_server, 1883); 
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
    lux = pow(500000 / ldrResistance, 1.4); 
  }

  if (lux < LUX_THRESHOLD) { 
    ledState = true;  
  } else {
    ledState = false; 
  }
  
  if (ledState == true) {
    digitalWrite(LED_PIN, HIGH); 
  } else {
    digitalWrite(LED_PIN, LOW);  
  }
  
  digitalWrite(LED_PIN, ledState ? HIGH : LOW);

  unsigned long currentMillis = millis();
  if (currentMillis - lastSendTime >= interval) {
    lastSendTime = currentMillis;
