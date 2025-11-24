const int trigPin = 6;
const int echoPin = 10;

volatile unsigned long tBegin = 0;
volatile unsigned long tEnd = 0;
volatile bool echoDone = false;
volatile bool awaitingEcho = false;

unsigned long lastPingMs = 0;

void echoISR() {
  if (!awaitingEcho) return; // ignora ruído fora de ciclo
  if (digitalRead(echoPin) == HIGH) {
    tBegin = micros();       // subida: início do pulso
  } else {
    tEnd = micros();         // descida: fim do pulso
    echoDone = true;
    awaitingEcho = false;
  }
}

void triggerPing() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  awaitingEcho = true;
  lastPingMs = millis();
}

void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  Serial.begin(9600);
  attachInterrupt(digitalPinToInterrupt(echoPin), echoISR, CHANGE);
  triggerPing(); // primeiro disparo
}

void loop() {
  if (echoDone) {
    noInterrupts();
    unsigned long us = tEnd - tBegin;
    echoDone = false;
    interrupts();

    float distance = (us * 0.0343f) / 2.0f;
    Serial.print("Pulso(us): ");
    Serial.print(us);
    Serial.print("  | Distancia(cm): ");
    Serial.println(distance);
  }

  // quando não estamos à espera de ECHO e já passou o intervalo, volta a disparar
  if (!awaitingEcho && !echoDone) {
    triggerPing();
  }
}
