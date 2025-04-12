#include <EEPROM.h>
#include <NimBLEDevice.h>  // Usamos NimBLE

// Pines
const int ledRojo = 27;
const int ledAmarillo = 26;
const int ledVerde = 25;
const int buzzer = 32;
const int boton = 33;

// Configuración del buzzer PWM
#define BUZZER_CHANNEL 0
#define BUZZER_FREQUENCY 2000
#define BUZZER_RESOLUTION 8

// UUIDs de BLE
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcd1234-ab12-cd34-ef56-abcdef123456"

NimBLECharacteristic *pCharacteristic;
bool bleConnected = false;

// Estados
bool alertaActiva = false;
bool confirmacionHecha = false;

unsigned long tiempoInicio;
const unsigned long tiempoAlerta = 10000; // 10 segundos
unsigned long ultimaNotificacion = 0;     // Para reenviar por BLE

// Callbacks BLE para conexión
class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) override {
    bleConnected = true;
    Serial.println("📶 BLE device connected");
  }

  void onDisconnect(NimBLEServer* pServer) override {
    bleConnected = false;
    Serial.println("📴 BLE device disconnected");
  }
};

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

  // BLE con NimBLE
  NimBLEDevice::init("DoseMate");
  NimBLEServer *pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  NimBLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
                    );
  pCharacteristic->setValue("🔋 Ready");
  pService->start();

  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->start();
  Serial.println("📡 BLE advertising started");

  Serial.println("🟢 System initialized. Waiting for medication time...");
}

void loop() {
  unsigned long ahora = millis();

  if (!alertaActiva && !confirmacionHecha && (ahora - tiempoInicio >= tiempoAlerta)) {
    alertaActiva = true;
    ultimaNotificacion = 0; // Forzar notificación inmediata
    digitalWrite(ledRojo, LOW);
    digitalWrite(ledAmarillo, HIGH);
    ledcWriteTone(BUZZER_CHANNEL, BUZZER_FREQUENCY);
    Serial.println("🔔 Time to take your medication!");
  }

  if (alertaActiva && !confirmacionHecha) {
    // Enviar notificación cada 5s si aún no se ha confirmado
    if (bleConnected && (ahora - ultimaNotificacion >= 5000)) {
      pCharacteristic->setValue("⏰ Time to take your medication!");
      pCharacteristic->notify();
      Serial.println("📲 BLE notification sent.");
      ultimaNotificacion = ahora;
    }

    // Confirmación por botón
    if (digitalRead(boton) == LOW) {
      delay(200);
      confirmacionHecha = true;

      digitalWrite(ledAmarillo, LOW);
      ledcWriteTone(BUZZER_CHANNEL, 0);
      digitalWrite(ledVerde, HIGH);

      EEPROM.put(0, 1);
      EEPROM.commit();

      Serial.println("✅ Medication confirmed.");

      if (bleConnected) {
        pCharacteristic->setValue("✅ Medication confirmed");
        pCharacteristic->notify();
      }
    }
  }
}
