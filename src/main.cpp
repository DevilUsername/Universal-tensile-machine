#include <Arduino.h>
#include <HX711.h>

// Піни тензодатчика
#define DTpin A0 
#define SCKpin A1

// Кінцевик
#define EndStop_pin 11

// Кроковий двигун
#define STEP 7
#define DIR 6

// Кількість кроків на 1 мм
#define stepsPerMM 5100

// Калібрування ваги
float calibrationFactor = 8800.0;

// Обʼєкт тензодатчика
HX711 scale;

// Стан системи
bool stopRequested = false;
bool stretching = false;
bool fixedLoadActive = false;

float maxForce = 0;
const float loadThreshold = 50.0;

// Для таймера ваги
unsigned long lastWeightTime = 0;
const unsigned long weightInterval = 200; // мс

void moveRelative(float mm, int direction);

void setup() {
  Serial.begin(9600);
  Serial.println("Machine startup");

  pinMode(STEP, OUTPUT);
  pinMode(DIR, OUTPUT);
  pinMode(EndStop_pin, INPUT_PULLUP);

  scale.begin(DTpin, SCKpin);
  if (!scale.is_ready()) {
    Serial.println("HX711 not ready");
  } else {
    Serial.println("HX711 ready");
  }

  scale.set_scale(calibrationFactor);
  scale.tare();
  Serial.println("Tensometr calibrated");

  Serial.println("write distance");
}

void loop() {
  float currentWeight = 0;

  // Читання ваги з періодичністю
  if (millis() - lastWeightTime >= weightInterval) {
    currentWeight = scale.get_units(1);
    Serial.print("W:");
    Serial.println(currentWeight, 2);
    lastWeightTime = millis();
  }

  // Фіксоване навантаження
  if (fixedLoadActive && !stopRequested) {
    if (currentWeight >= loadThreshold) {
      stopRequested = true;
      fixedLoadActive = false;
      Serial.println("Load threshold reached, stopping motor");
    } else {
      // Крок мотора
      digitalWrite(STEP, HIGH);
      delayMicroseconds(1000);
      digitalWrite(STEP, LOW);
      delayMicroseconds(1000);
    }
  }

  // Безперервне розтягування
  if (stretching && !stopRequested) {
    if (currentWeight > maxForce) {
      maxForce = currentWeight;
    }

    if (maxForce > 1.0 && currentWeight < (maxForce * 0.8)) {
      Serial.println("Sample broken! STOP");
      stopRequested = true;
      stretching = false;
      return;
    }

    digitalWrite(DIR, LOW);
    digitalWrite(STEP, HIGH);
    delayMicroseconds(500);
    digitalWrite(STEP, LOW);
    delayMicroseconds(500);
    return;
  }

  // Обробка команд
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.startsWith("GOTO:")) {
      int sampleLengthMM = input.substring(5).toInt();
      long totalSteps = (long)sampleLengthMM * stepsPerMM;

      Serial.println("move to ");
      digitalWrite(DIR, LOW);
      stopRequested = false;
      for (long i = 0; i < totalSteps; i++) {
        if (stopRequested) {
          Serial.println("Рух зупинено");
          break;
        }
        digitalWrite(STEP, HIGH);
        delayMicroseconds(500);
        digitalWrite(STEP, LOW);
        delayMicroseconds(500);

        if (millis() - lastWeightTime >= weightInterval) {
          float w = scale.get_units(1);
          Serial.print("W:");
          Serial.println(w, 2);
          lastWeightTime = millis();
        }
      }

      Serial.println("position is reached");
      return;
    }

    if (input.startsWith("LEFT:")) {
      float mm = input.substring(5).toFloat();
      stopRequested = false;
      moveRelative(mm, HIGH);
      return;
    }

    if (input.startsWith("RIGHT:")) {
      float mm = input.substring(6).toFloat();
      stopRequested = false;
      moveRelative(mm, LOW);
      return;
    }

    if (input == "STRETCH_START") {
      stretching = true;
      stopRequested = false;
      maxForce = 0;
      Serial.println("Розтягування почалося");
      return;
    }

    if (input == "LOAD_START") {
      Serial.println("Fixed load started");
      stopRequested = false;
      fixedLoadActive = true;
      digitalWrite(DIR, LOW); // напрямок до зразка
      maxForce = 0;
      lastWeightTime = millis();
      return;
    }

    if (input == "STOP") {
      stopRequested = true;
      fixedLoadActive = false;
      stretching = false;
      Serial.println("STOP command received");
      return;
    }
  }
}

void moveRelative(float mm, int direction) {
  long steps = (long)(mm * stepsPerMM);

  Serial.print("Moving ");
  Serial.print(mm);
  Serial.print(" mm in direction ");
  Serial.println(direction == HIGH ? "LEFT" : "RIGHT");

  digitalWrite(DIR, direction);
  for (long i = 0; i < steps; i++) {
    if (stopRequested) {
      Serial.println("Рух зупинено");
      break;
    }
    digitalWrite(STEP, HIGH);
    delayMicroseconds(500);
    digitalWrite(STEP, LOW);
    delayMicroseconds(500);

    if (millis() - lastWeightTime >= weightInterval) {
      float w = scale.get_units(1);
      Serial.print("W:");
      Serial.println(w, 2);
      lastWeightTime = millis();
    }
  }

  Serial.println("Relative move complete");
}
