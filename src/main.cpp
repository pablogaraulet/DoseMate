#include <Wire.h>
#include <EEPROM.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <HttpClient.h>
#include <time.h>
#include <Adafruit_AHTX0.h>
#include <TFT_eSPI.h>

// Pins
const int ledRed = 27;
const int ledYellow = 26;
const int ledGreen = 25;
const int buzzer = 32;
const int button = 33;

// WiFi
const char* ssid = "BLVD63";
const char* password = "sdBLVD63";
const char* server = "3.144.105.179";
const int port = 5000;
const char* device_id = "ttgo01";

// BLE UUIDs
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcd1234-ab12-cd34-ef56-abcdef123456"

// BLE, Flags
NimBLECharacteristic* pCharacteristic;
bool bleConnected = false;
bool alertActive = false;
bool confirmationDone = false;

unsigned long startTime;
const unsigned long alertDelay = 10000;
unsigned long lastNotification = 0;
unsigned long lastBlink = 0;
bool blinkState = false;

// Sensor
Adafruit_AHTX0 aht;

// TFT Display
TFT_eSPI tft = TFT_eSPI();

// Centered Message
void showMessage(const String& text, uint16_t color = TFT_WHITE, uint8_t size = 2) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextSize(size);
  int x = (tft.width() - text.length() * 6 * size) / 2;
  int y = (tft.height() - 8 * size) / 2;
  tft.setCursor(x, y);
  tft.println(text);
}

class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) { bleConnected = true; Serial.println("📶 BLE device connected"); }
  void onDisconnect(NimBLEServer* pServer) { bleConnected = false; Serial.println("📴 BLE device disconnected"); }
};

void connectWiFi() {
  Serial.println("🌐 Connecting to WiFi...");
  WiFi.begin(ssid, password);
  int tries = 60;
  while (WiFi.status() != WL_CONNECTED && tries-- > 0) { delay(500); Serial.print("."); }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected");
    Serial.print("📡 IP: ");
    Serial.println(WiFi.localIP());
    configTime(0, 0, "pool.ntp.org");
    struct tm timeinfo;
    Serial.println("🕒 Waiting for NTP sync...");
    while (!getLocalTime(&timeinfo)) { Serial.println("❌ Failed to sync time..."); delay(1000); }
    Serial.println("✅ Time synchronized");
  } else {
    Serial.println("\n⚠️ WiFi Failed. Retrying...");
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

  String path = "/envlog?device_id=" + String(device_id) + "&temperature=" + String(temp,1) + "&humidity=" + String(hum,1) + "&status=" + status + "&timestamp=" + timeString;

  Serial.println("📡 Uploading data...");
  showMessage("📡 Uploading...", TFT_CYAN, 1);
  int err = client.get(server, port, path.c_str());

  if (err == 0) {
    Serial.println("🌐 Upload OK");
  } else {
    Serial.println("❌ Upload Error");
  }
  client.stop();
}

void setup() {
  Serial.begin(115200);

  pinMode(ledRed, OUTPUT);
  pinMode(ledYellow, OUTPUT);
  pinMode(ledGreen, OUTPUT);
  pinMode(button, INPUT_PULLUP);

  ledcSetup(0, 2000, 8);
  ledcAttachPin(buzzer, 0);
  ledcWriteTone(0, 0);

  digitalWrite(ledRed, HIGH);
  digitalWrite(ledYellow, LOW);
  digitalWrite(ledGreen, LOW);

  EEPROM.begin(512);
  startTime = millis();

  connectWiFi();

  Wire.begin(21, 22);
  delay(200);

  tft.init();
  tft.setRotation(1);
  showMessage("DoseMate Ready!", TFT_GREEN, 3);

  bool sensorOK = false;
  for (int i = 0; i < 5; i++) {
    if (aht.begin(&Wire)) { sensorOK = true; break; }
    delay(500);
  }

  if (!sensorOK) {
    showMessage("⚠️ Sensor Error!", TFT_RED, 2);
    while (1) delay(10);
  }

  NimBLEDevice::init("DoseMate");
  NimBLEServer* pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  NimBLEService* pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  pCharacteristic->setValue("🔋 Ready");
  pService->start();
  NimBLEDevice::getAdvertising()->start();
}

void loop() {
  unsigned long now = millis();

  if (!alertActive && !confirmationDone && now - startTime >= alertDelay) {
    alertActive = true;
    lastNotification = 0;
    digitalWrite(ledRed, LOW);
    digitalWrite(ledYellow, HIGH);
    ledcWriteTone(0, 2000);
  }

  if (!alertActive && !confirmationDone) {
    showMessage("Waiting...", TFT_WHITE, 1);
  }

  if (alertActive && !confirmationDone) {
    if (now - lastBlink >= 500) {
      blinkState = !blinkState;
      if (blinkState)
        showMessage("💊 Time for Meds!", TFT_YELLOW, 3);
      else
        showMessage("", TFT_BLACK, 1);
      lastBlink = now;
    }

    if (bleConnected && now - lastNotification >= 5000) {
      pCharacteristic->setValue("⏰ Time to take your medication!");
      pCharacteristic->notify();
      lastNotification = now;
    }

    if (digitalRead(button) == LOW) {
      delay(200);
      confirmationDone = true;
      digitalWrite(ledYellow, LOW);
      digitalWrite(ledGreen, HIGH);
      ledcWriteTone(0, 0);

      EEPROM.put(0, 1);
      EEPROM.commit();

      showMessage("✅ Meds Taken!", TFT_GREEN, 3);
      delay(1500);
      showMessage("Well done!", TFT_GREEN, 2);

      if (bleConnected) {
        pCharacteristic->setValue("✅ Medication confirmed");
        pCharacteristic->notify();
      }

      sensors_event_t humidity, temp;
      aht.getEvent(&humidity, &temp);

      if (!isnan(temp.temperature) && !isnan(humidity.relative_humidity)) {
        sendEnvironmentLog(temp.temperature, humidity.relative_humidity, evaluateEnvironment(temp.temperature, humidity.relative_humidity));
      }
    }
  }
}
