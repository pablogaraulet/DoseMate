#include <Wire.h>
#include <EEPROM.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <HttpClient.h>
#include <time.h>
#include <Adafruit_AHTX0.h>
#include <TFT_eSPI.h>

// Pins
const int ledRed    = 27;
const int ledYellow = 26;
const int ledGreen  = 25;
const int buzzer    = 32;
const int button    = 33;

// WiFi
const char *ssid      = "BLVD63";
const char *password  = "sdBLVD63";
const char *server    = "18.227.105.20";
const int   port      = 5000;
const char *device_id = "ttgo01";

// BLE UUIDs
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcd1234-ab12-cd34-ef56-abcdef123456"

NimBLECharacteristic *pCharacteristic;
bool bleConnected     = false;
bool alertActive      = false;
bool confirmationDone = false;

unsigned long startTime;
const unsigned long alertDelay     = 2000;
unsigned long lastNotification     = 0;
unsigned long lastBlink            = 0;
bool blinkState                    = false;
int  notifyCount                   = 0;

Adafruit_AHTX0 aht;
TFT_eSPI     tft = TFT_eSPI();

// Helper to draw text on display
void fadeScreen() {
  tft.fillScreen(TFT_BLACK);
}
void showMessage(const String &text, uint16_t color = TFT_WHITE, uint8_t size = 2) {
  fadeScreen();
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextSize(size);
  int w = text.length() * 6 * size;
  int h = 8 * size;
  tft.setCursor((tft.width() - w) / 2, (tft.height() - h) / 2);
  tft.println(text);
}
void showTemporaryMessage(const String &text, uint16_t color, uint8_t size, unsigned long duration) {
  showMessage(text, color, size);
  delay(duration);
  fadeScreen();
}

// BLE Server callbacks
class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) {
    bleConnected = true;
    Serial.println("BLE device connected");
    // send initial notification immediately upon connection
    pCharacteristic->setValue("Time to take your medication! #0");
    pCharacteristic->notify();
    notifyCount = 1;
  }
  void onDisconnect(NimBLEServer* pServer) {
    bleConnected = false;
    Serial.println("BLE device disconnected");
  }
};

// WiFi + NTP setup
void connectWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  int tries = 60;
  while (WiFi.status() != WL_CONNECTED && tries-- > 0) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected, IP: " + WiFi.localIP().toString());
    setenv("TZ", "PDT7PDT,M3.2.0,M11.1.0", 1);
    tzset();
    configTime(0, 0, "pool.ntp.org");
    struct tm ti;
    Serial.println("Waiting for NTP sync...");
    while (!getLocalTime(&ti)) {
      Serial.print(".");
      delay(500);
    }
    Serial.println("\nTime synchronized");
  } else {
    Serial.println("\nWiFi failed; retrying...");
    delay(2000);
    connectWiFi();
  }
}

// Environment evaluation + upload
String evaluateEnvironment(float temp, float hum) {
  if (temp >= 18 && temp <= 25 && hum >= 40 && hum <= 55) return "optimal";
  if ((temp >= 10 && temp < 18) || (temp > 25 && temp <= 30) ||
      (hum >= 30 && hum < 40)  || (hum > 55 && hum <= 65)) return "regular";
  return "danger";
}
void sendEnvironmentLog(float temp, float hum, String status) {
  WiFiClient wifi;
  HttpClient client(wifi);
  char ts[30];
  time_t now = time(nullptr);
  strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", localtime(&now));
  String path = String("/envlog?device_id=") + device_id +
                "&temperature=" + String(temp,1) +
                "&humidity="    + String(hum,1) +
                "&status="      + status +
                "&timestamp="   + ts;
  Serial.println("Uploading data...");
  int err = client.get(server, port, path.c_str());
  Serial.println(err == 0 ? "Upload OK" : "Upload Error");
  client.stop();
}

void setup() {
  Serial.begin(115200);

  // initialize LEDs, buzzer, button
  pinMode(ledRed,    OUTPUT);
  pinMode(ledYellow, OUTPUT);
  pinMode(ledGreen,  OUTPUT);
  pinMode(button,    INPUT_PULLUP);

  ledcSetup(0, 2000, 8);
  ledcAttachPin(buzzer, 0);
  ledcWriteTone(0, 0);

  digitalWrite(ledRed,    HIGH);
  digitalWrite(ledYellow, LOW);
  digitalWrite(ledGreen,  LOW);

  EEPROM.begin(512);
  startTime = millis();

  // WiFi + NTP
  connectWiFi();

  // display init
  Wire.begin(21, 22);
  delay(200);
  tft.init();
  tft.setRotation(1);
  showMessage("DoseMate Ready!", TFT_GREEN, 3);

  // sensor init
  if (!aht.begin()) {
    showMessage("Sensor Error!", TFT_RED, 2);
    while (1) delay(10);
  }

  // BLE setup 
  NimBLEDevice::init("DoseMate");
  NimBLEServer *pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  NimBLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
  );

  
  pCharacteristic->setValue("Ready");
  pService->start();

  NimBLEAdvertising *pAdv = NimBLEDevice::getAdvertising();
  pAdv->addServiceUUID(SERVICE_UUID);
  pAdv->start();
  Serial.println("BLE Advertising started with service UUID");
}

void loop() {
  unsigned long now = millis();

  // after alertDelay, activate alert
  if (!alertActive && !confirmationDone && now - startTime >= alertDelay) {
    alertActive = true;
    lastNotification = now;

    // visual + buzzer alert
    digitalWrite(ledRed, LOW);
    digitalWrite(ledYellow, HIGH);
    ledcWriteTone(0, 2000);
  }

  if (!alertActive && !confirmationDone) {
    showMessage("Waiting...", TFT_WHITE, 2);
  }

  if (alertActive && !confirmationDone) {
    // blink message on screen
    if (now - lastBlink >= 600) {
      blinkState = !blinkState;
      if (blinkState) showMessage("Meds Time!", TFT_YELLOW, 2);
      else            fadeScreen();
      lastBlink = now;
    }

    // periodic BLE notify every 5s
    if (bleConnected && now - lastNotification >= 5000) {
      String msg = "Time to take your medication! #" + String(++notifyCount);
      Serial.println("Periodic notify: " + msg);
      pCharacteristic->setValue(msg.c_str());
      pCharacteristic->notify();
      lastNotification = now;
    }

    // on button press, confirm
    if (digitalRead(button) == LOW) {
      delay(200);
      confirmationDone = true;
      digitalWrite(ledYellow, LOW);
      digitalWrite(ledGreen,  HIGH);
      ledcWriteTone(0, 0);

      showTemporaryMessage("Meds Taken!", TFT_GREEN, 2, 1500);
      showTemporaryMessage("Well done!", TFT_GREEN, 2, 2000);

      if (bleConnected) {
        String msg = "Medication confirmed #" + String(++notifyCount);
        Serial.println("Confirm notify: " + msg);
        pCharacteristic->setValue(msg.c_str());
        pCharacteristic->notify();
      }

      sensors_event_t humidity, temp;
      aht.getEvent(&humidity, &temp);
      if (!isnan(temp.temperature) && !isnan(humidity.relative_humidity)) {
        sendEnvironmentLog(
          temp.temperature,
          humidity.relative_humidity,
          evaluateEnvironment(temp.temperature, humidity.relative_humidity)
        );
      }
    }
  }
}
