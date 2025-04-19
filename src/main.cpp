#include <EEPROM.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <HttpClient.h>
#include <time.h>

// Pins
const int ledRojo = 27;
const int ledAmarillo = 26;
const int ledVerde = 25;
const int buzzer = 32;
const int boton = 33;

// WiFi
const char* ssid = "BLVD63";
const char* password = "sdBLVD63";
const char* server = "18.223.170.211";  // AWS IP
const int port = 5000;
const char* device_id = "ttgo01";       // Optional: unique identifier

// Buzzer PWM
#define BUZZER_CHANNEL 0
#define BUZZER_FREQUENCY 2000
#define BUZZER_RESOLUTION 8

// BLE UUIDs
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcd1234-ab12-cd34-ef56-abcdef123456"

NimBLECharacteristic* pCharacteristic;
bool bleConnected = false;
bool alertaActiva = false;
bool confirmacionHecha = false;

unsigned long tiempoInicio;
const unsigned long tiempoAlerta = 10000;
unsigned long ultimaNotificacion = 0;

// BLE Callbacks
class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) {
    bleConnected = true;
    Serial.println("📶 BLE device connected");
  }

  void onDisconnect(NimBLEServer* pServer) {
    bleConnected = false;
    Serial.println("📴 BLE device disconnected");
  }
};

void conectarWiFi() {
  Serial.println("🌐 Connecting to WiFi...");
  WiFi.begin(ssid, password);
  int maxIntentos = 60;
  while (WiFi.status() != WL_CONNECTED && maxIntentos-- > 0) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected");
    Serial.print("📡 IP: ");
    Serial.println(WiFi.localIP());

    // NTP Time sync
    configTime(0, 0, "pool.ntp.org"); // UTC
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
    conectarWiFi();
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(ledRojo, OUTPUT);
  pinMode(ledAmarillo, OUTPUT);
  pinMode(ledVerde, OUTPUT);
  pinMode(boton, INPUT_PULLUP);

  ledcSetup(BUZZER_CHANNEL, BUZZER_FREQUENCY, BUZZER_RESOLUTION);
  ledcAttachPin(buzzer, BUZZER_CHANNEL);
  ledcWriteTone(BUZZER_CHANNEL, 0);

  digitalWrite(ledRojo, HIGH);
  digitalWrite(ledAmarillo, LOW);
  digitalWrite(ledVerde, LOW);

  EEPROM.begin(512);
  tiempoInicio = millis();

  conectarWiFi();

  NimBLEDevice::init("DoseMate");
  NimBLEServer* pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  NimBLEService* pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  pCharacteristic->setValue("🔋 Ready");
  pService->start();
  NimBLEDevice::getAdvertising()->start();

  Serial.println("🟢 System initialized. Waiting...");
}

void loop() {
  unsigned long ahora = millis();

  if (!alertaActiva && !confirmacionHecha && (ahora - tiempoInicio >= tiempoAlerta)) {
    alertaActiva = true;
    ultimaNotificacion = 0;

    digitalWrite(ledRojo, LOW);
    digitalWrite(ledAmarillo, HIGH);
    ledcWriteTone(BUZZER_CHANNEL, BUZZER_FREQUENCY);

    Serial.println("🔔 Time to take your medication!");
  }

  if (alertaActiva && !confirmacionHecha) {
    if (bleConnected && (ahora - ultimaNotificacion >= 5000)) {
      pCharacteristic->setValue("⏰ Time to take your medication!");
      pCharacteristic->notify();
      ultimaNotificacion = ahora;
      Serial.println("📲 BLE notification sent.");
    }

    if (digitalRead(boton) == LOW) {
      delay(200);
      confirmacionHecha = true;

      digitalWrite(ledAmarillo, LOW);
      digitalWrite(ledVerde, HIGH);
      ledcWriteTone(BUZZER_CHANNEL, 0);

      EEPROM.put(0, 1);
      EEPROM.commit();

      Serial.println("✅ Medication confirmed.");

      if (bleConnected) {
        pCharacteristic->setValue("✅ Medication confirmed");
        pCharacteristic->notify();
      }

      // 🌍 HTTP request to Flask backend
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
        while (client.available()) {
          char c = client.read();
          Serial.print(c);
        }
        Serial.println();
      } else {
        Serial.print("❌ HTTP error: ");
        Serial.println(err);
      }

      client.stop();
    }
  }
}
