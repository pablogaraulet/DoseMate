#include <Wire.h>
#include <EEPROM.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <HttpClient.h>
#include <time.h>
#include <Adafruit_AHTX0.h>

// Pins
const int ledRed = 27;
const int ledYellow = 26;
const int ledGreen = 25;
const int buzzer = 32;
const int button = 33;

// WiFi
const char* ssid = "BLVD63";
const char* password = "sdBLVD63";
const char* server = "18.223.170.211";
const int port = 5000;
const char* device_id = "ttgo01";

// Buzzer PWM
#define BUZZER_CHANNEL 0
#define BUZZER_FREQUENCY 2000
#define BUZZER_RESOLUTION 8

// BLE UUIDs
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcd1234-ab12-cd34-ef56-abcdef123456"

NimBLECharacteristic* pCharacteristic;
bool bleConnected = false;
bool alertActive = false;
bool confirmationDone = false;

unsigned long startTime;
const unsigned long alertDelay = 10000;
unsigned long lastNotification = 0;

// Sensor
Adafruit_AHTX0 aht;

class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) { bleConnected = true; Serial.println("📶 BLE device connected"); }
  void onDisconnect(NimBLEServer* pServer) { bleConnected = false; Serial.println("📴 BLE device disconnected"); }
};

void connectWiFi() {
  Serial.println("🌐 Connecting to WiFi...");
  WiFi.begin(ssid, password);
  int maxTries = 60;
  while (WiFi.status() != WL_CONNECTED && maxTries-- > 0) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected");
    Serial.print("📡 IP: ");
    Serial.println(WiFi.localIP());
    configTime(0, 0, "pool.ntp.org");
    struct tm timeinfo;
    Serial.println("🕒 Waiting for NTP sync...");
    while (!getLocalTime(&timeinfo)) {
      Serial.println("❌ Failed to get time, retrying...");
      delay(1000);
    }
    Serial.println("✅ Time synchronized");
  } else {
    Serial.println("\n⚠️ WiFi connection failed. Retrying...");
    delay(2000);
    connectWiFi();
  }
}

String evaluateEnvironment(float temp, float hum) {
  if (temp >= 18 && temp <= 25 && hum >= 40 && hum <= 55) return "optimal";
  if ((temp >= 10 && temp < 18) || (temp > 25 && temp <= 30) || (hum >= 30 && hum < 40) || (hum > 55 && hum <= 65)) return "regular";
  return "danger";
}

void sendEnvironmentLog(float temp, float hum, String status) {
  WiFiClient wifi;
  HttpClient client(wifi);
  char timeString[30];
  time_t now = time(nullptr);
  strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

  String path = "/envlog?device_id=" + String(device_id) +
                "&temperature=" + String(temp, 1) +
                "&humidity=" + String(hum, 1) +
                "&status=" + status +
                "&timestamp=" + String(timeString);

  Serial.println("🌡️ Sending environmental data...");
  int err = client.get(server, port, path.c_str());

  if (err == 0) {
    Serial.print("🌐 HTTP response: ");
    while (client.available()) { Serial.print(client.read()); }
    Serial.println();
  } else {
    Serial.print("❌ HTTP error: ");
    Serial.println(err);
  }
  client.stop();
}

void setup() {
  Serial.begin(115200);

  pinMode(ledRed, OUTPUT);
  pinMode(ledYellow, OUTPUT);
  pinMode(ledGreen, OUTPUT);
  pinMode(button, INPUT_PULLUP);

  ledcSetup(BUZZER_CHANNEL, BUZZER_FREQUENCY, BUZZER_RESOLUTION);
  ledcAttachPin(buzzer, BUZZER_CHANNEL);
  ledcWriteTone(BUZZER_CHANNEL, 0);

  digitalWrite(ledRed, HIGH);
  digitalWrite(ledYellow, LOW);
  digitalWrite(ledGreen, LOW);

  EEPROM.begin(512);
  startTime = millis();

  connectWiFi();

  Wire.begin(21, 22);  // SDA, SCL
  delay(200); // Stabilize

  // Retry initialization
  bool sensor_ok = false;
  for (int i = 0; i < 5; i++) {
    if (aht.begin(&Wire)) {
      sensor_ok = true;
      break;
    }
    Serial.println("⏳ Retrying sensor initialization...");
    delay(500);
  }

  if (!sensor_ok) {
    Serial.println("❌ Sensor AHT20/DHT20 not detected.");
    while (1) delay(10);
  }
  Serial.println("✅ Sensor AHT20 successfully initialized.");

  NimBLEDevice::init("DoseMate");
  NimBLEServer* pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  NimBLEService* pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  pCharacteristic->setValue("🔋 Ready");
  pService->start();
  NimBLEDevice::getAdvertising()->start();

  Serial.println("🟢 System initialized. Waiting...");
}

void loop() {
  unsigned long now = millis();

  if (!alertActive && !confirmationDone && (now - startTime >= alertDelay)) {
    alertActive = true;
    lastNotification = 0;
    digitalWrite(ledRed, LOW);
    digitalWrite(ledYellow, HIGH);
    ledcWriteTone(BUZZER_CHANNEL, BUZZER_FREQUENCY);
    Serial.println("🔔 Time to take your medication!");
  }

  if (alertActive && !confirmationDone) {
    if (bleConnected && (now - lastNotification >= 5000)) {
      pCharacteristic->setValue("⏰ Time to take your medication!");
      pCharacteristic->notify();
      lastNotification = now;
      Serial.println("📲 BLE notification sent.");
    }

    if (digitalRead(button) == LOW) {
      delay(200);
      confirmationDone = true;
      digitalWrite(ledYellow, LOW);
      digitalWrite(ledGreen, HIGH);
      ledcWriteTone(BUZZER_CHANNEL, 0);

      EEPROM.put(0, 1);
      EEPROM.commit();
      Serial.println("✅ Medication confirmed.");

      if (bleConnected) {
        pCharacteristic->setValue("✅ Medication confirmed");
        pCharacteristic->notify();
      }

      WiFiClient wifi;
      HttpClient client(wifi);
      char timeString[30];
      time_t now = time(nullptr);
      strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

      String path = "/medication?device_id=" + String(device_id) +
                    "&status=confirmed&timestamp=" + String(timeString);

      int err = client.get(server, port, path.c_str());
      if (err == 0) {
        Serial.print("🌐 HTTP response: ");
        while (client.available()) { Serial.print(client.read()); }
        Serial.println();
      } else {
        Serial.print("❌ HTTP error: ");
        Serial.println(err);
      }
      client.stop();

      // Read sensor
      sensors_event_t humidity, temp;
      aht.getEvent(&humidity, &temp);

      float t = temp.temperature;
      float h = humidity.relative_humidity;

      if (!isnan(t) && !isnan(h)) {
        String envStatus = evaluateEnvironment(t, h);
        Serial.printf("📊 Environment: %.1f°C | %.1f%% RH → %s\n", t, h, envStatus.c_str());

        if (bleConnected) {
          String msg = "🌡️ " + String(t,1) + "°C | 💧 " + String(h,1) + "% → " + envStatus;
          pCharacteristic->setValue(msg.c_str());
          pCharacteristic->notify();
        }

        sendEnvironmentLog(t, h, envStatus);
      } else {
        Serial.println("❌ Sensor read error.");
      }
    }
  }
}
