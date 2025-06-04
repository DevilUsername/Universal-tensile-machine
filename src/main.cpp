#include <Arduino.h>
#include <HX711.h>

// Піни тензодатчика
#define DTpin A0 
#define SCKpin A1

// Кінцевик
#define EndStop_pin 12

// Кроковий двигун
#define STEP 7
#define DIR 6

// Кількість кроків на 1 мм
#define stepsPerMM 5100

// Калібрування ваги
float calibrationFactor = 8800.0;

// Обʼєкт тензодатчика
HX711 scale;

// Флаг зупинки
bool stopRequested = false;

void moveRelative(float mm, int direction);

void setup() {
  Serial.begin(9600);
  Serial.println("Machine startup");

  pinMode(STEP, OUTPUT);
  pinMode(DIR, OUTPUT);
  pinMode(EndStop_pin, INPUT_PULLUP);

  // ініціалізація тензодатчика
  scale.begin(DTpin, SCKpin);
  if (!scale.is_ready()) {
    Serial.println("HX711 not ready");
  } else {
    Serial.println("HX711 ready");
  }

  scale.set_scale(calibrationFactor);
  scale.tare();
  Serial.println("Tensometr calibrated");

  // автоматичний пошук нуля
  Serial.println("Setuping zero point");
  digitalWrite(DIR, HIGH);
  while (digitalRead(EndStop_pin) == HIGH) {
    digitalWrite(STEP, HIGH);
    delay(1);
    digitalWrite(STEP, LOW);
    delay(1);
  }
  Serial.println("zero point detected");
  delay(1000);

  Serial.println("write distance");
}

void loop() {
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
          Serial.println("⛔ Рух зупинено");
          break;
        }
        digitalWrite(STEP, HIGH);
        delay(1);
        digitalWrite(STEP, LOW);
        delay(1);
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

    if (input == "WEIGHT") {
      float w = scale.get_units(10);
      Serial.print("W:");
      Serial.println(w, 2);
      return;
    }

    if (input == "STOP") {
      stopRequested = true;
      Serial.println("⛔ STOP received");
      return;
    }

    Serial.println("Invalid command");
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
      Serial.println("⛔ Рух зупинено");
      break;
    }
    digitalWrite(STEP, HIGH);
    delay(1);
    digitalWrite(STEP, LOW);
    delay(1);
  }

  Serial.println("Relative move complete");
}
