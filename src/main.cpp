#include <EEPROM.h>

// Pines
const int ledRojo = 27;
const int ledAmarillo = 26;
const int ledVerde = 25;
const int buzzer = 32;
const int boton = 33;

// Estados
bool alertaActiva = false;
bool confirmacionHecha = false;

unsigned long tiempoInicio;
const unsigned long tiempoAlerta = 10000; // 10 segundos

void setup() {
  Serial.begin(115200);

  pinMode(ledRojo, OUTPUT);
  pinMode(ledAmarillo, OUTPUT);
  pinMode(ledVerde, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(boton, INPUT_PULLUP); // Botón a GND

  // Estado inicial
  digitalWrite(ledRojo, HIGH);      // Aún no es hora → LED rojo encendido
  digitalWrite(ledAmarillo, LOW);
  digitalWrite(ledVerde, LOW);
  digitalWrite(buzzer, LOW);

  EEPROM.begin(512);
  tiempoInicio = millis();

  Serial.println("🟢 Sistema iniciado. Esperando la hora de medicación...");
}

void loop() {
  unsigned long ahora = millis();

  // Cuando llega la hora de tomar el medicamento
  if (!alertaActiva && ahora - tiempoInicio >= tiempoAlerta) {
    alertaActiva = true;

    digitalWrite(ledRojo, LOW);       // Apagar rojo
    digitalWrite(ledAmarillo, HIGH);  // Enciende amarillo
    digitalWrite(buzzer, HIGH);       // Suena buzzer

    Serial.println("🔔 ¡Hora de tomar el medicamento!");
  }

  // Si se presiona el botón
  if (alertaActiva && !confirmacionHecha && digitalRead(boton) == LOW) {
    delay(200); // debounce
    confirmacionHecha = true;

    digitalWrite(ledAmarillo, LOW);  // Apagar amarillo
    digitalWrite(buzzer, LOW);       // Apagar buzzer
    digitalWrite(ledVerde, HIGH);    // Encender verde

    EEPROM.put(0, 1);
    EEPROM.commit();

    Serial.println("✅ Medicamento confirmado correctamente.");
  }
}
