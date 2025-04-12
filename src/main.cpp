#include <EEPROM.h>

// Pins
const int ledRojo = 27;       // Red LED – "Not time yet"
const int ledAmarillo = 26;   // Yellow LED – "Time to take"
const int ledVerde = 25;      // Green LED – "Confirmed"
const int buzzer = 32;
const int boton = 33;         // Button input

// Buzzer PWM settings
#define BUZZER_CHANNEL 0
#define BUZZER_FREQUENCY 2000
#define BUZZER_RESOLUTION 8

// States
bool alertaActiva = false;
bool confirmacionHecha = false;

unsigned long tiempoInicio;
const unsigned long tiempoAlerta = 10000; // Alert after 10 seconds

void setup() {
  Serial.begin(115200);

  pinMode(ledRojo, OUTPUT);
  pinMode(ledAmarillo, OUTPUT);
  pinMode(ledVerde, OUTPUT);
  pinMode(boton, INPUT_PULLUP); // Button connected to GND

  // Setup buzzer PWM
  ledcSetup(BUZZER_CHANNEL, BUZZER_FREQUENCY, BUZZER_RESOLUTION);
  ledcAttachPin(buzzer, BUZZER_CHANNEL);
  ledcWriteTone(BUZZER_CHANNEL, 0); // Start silent

  // Initial state: medication not yet due
  digitalWrite(ledRojo, HIGH);      // Red LED ON at start
  digitalWrite(ledAmarillo, LOW);
  digitalWrite(ledVerde, LOW);

  EEPROM.begin(512);
  tiempoInicio = millis();

  Serial.println("🟢 System initialized. Waiting for medication time...");
}

void loop() {
  unsigned long ahora = millis();

  // Time to take medication
  if (!alertaActiva && ahora - tiempoInicio >= tiempoAlerta) {
    alertaActiva = true;

    digitalWrite(ledRojo, LOW);       // Turn off red LED
    digitalWrite(ledAmarillo, HIGH);  // Turn on yellow LED

    // Activate buzzer using tone
    ledcWriteTone(BUZZER_CHANNEL, BUZZER_FREQUENCY);

    Serial.println("🔔 Time to take your medication!");
  }

  // Button pressed to confirm
  if (alertaActiva && !confirmacionHecha && digitalRead(boton) == LOW) {
    delay(200); // debounce
    confirmacionHecha = true;

    digitalWrite(ledAmarillo, LOW);     // Turn off yellow LED
    ledcWriteTone(BUZZER_CHANNEL, 0);   // Turn off buzzer
    digitalWrite(ledVerde, HIGH);       // Turn on green LED

    EEPROM.put(0, 1);
    EEPROM.commit();

    Serial.println("✅ Medication confirmed.");
  }
}
