#include <ArduinoBLE.h>  // Inclui a biblioteca para BLE (Bluetooth Low Energy)

// Pinos para o sensor ultrassónico
const int trigPin = 6;   // pino de trigger (disparo do pulso ultrassónico)
const int echoPin = 10;  // pino de echo (recepção do pulso refletido)

// Variáveis voláteis (alteradas dentro de ISR) para medir o tempo do eco
volatile unsigned long tBegin = 0;     // momento em micros(segundos) da subida do eco
volatile unsigned long tEnd = 0;       // momento em micros(segundos) da descida do eco
volatile bool echoDone = false;        // flag: medição concluída
volatile bool awaitingEcho = false;    // flag: estamos actualmente à espera do eco

unsigned long lastPingMs = 0;          // momento em milissegundos do último ping disparado

// Criação do serviço BLE
BLEService sensorService("19b10000-e8f2-537e-4f6c-d104768a1214"); // UUID do serviço
BLEFloatCharacteristic distanceChar(
  "19b10001-e8f2-537e-4f6c-d104768a1214",  // UUID da característica
  BLERead | BLENotify                        // permite leitura e notificações
);

// Função de interrupção — chamada sempre que o pino echo muda de estado
void echoISR() {
  if (!awaitingEcho) return;            // se não estivermos à espera de echo, ignora (reduzir ruído)
  if (digitalRead(echoPin) == HIGH) {
    tBegin = micros();                   // pulso subiu → início da medição
  } else {
    tEnd = micros();                     // pulso desceu → fim da medição
    echoDone = true;                     // sinaliza que temos medição completa
    awaitingEcho = false;                // já não estamos à espera
  }
}

// Função para disparar o pulso ultrassónico
void triggerPing() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);          // gera pulso alto por 10 µs
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  awaitingEcho = true;                   // agora aguardamos o eco
  lastPingMs = millis();                 // guarda o momento em ms do disparo
}

void setup() {
  pinMode(trigPin, OUTPUT);              // define trigger como saída
  pinMode(echoPin, INPUT);               // define echo como entrada

  Serial.begin(9600);                     // inicializa comunicação serial para debug

  if (!BLE.begin()) {                     // inicializa BLE
    Serial.println("Falha ao iniciar o BLE!");
    while (1);                            // em caso de falha, fica aqui
  }

  BLE.setLocalName("UltrassomBLE");                      // nome do dispositivo BLE
  BLE.setAdvertisedService(sensorService);               // anúncio do serviço
  sensorService.addCharacteristic(distanceChar);         // adiciona a característica ao serviço
  BLE.addService(sensorService);                          // adiciona o serviço ao dispositivo
  distanceChar.writeValue(0.0f);                          // define valor inicial da distância
  BLE.advertise();                                        // inicia anúncio do dispositivo

  attachInterrupt(digitalPinToInterrupt(echoPin), echoISR, CHANGE);  // configura ISR para mudanças no pino echo
  triggerPing();                                           // primeiro disparo logo no setup
}

void loop() {
  if (echoDone) {                        // se temos medição completa
    noInterrupts();                      // desliga interrupções temporariamente para segurança
    unsigned long us = tEnd - tBegin;   // calcula o tempo do pulso em microssegundos
    echoDone = false;                   // limpa a flag
    interrupts();                       // volta a ligar interrupções

    // converte o tempo de voo em distância em cm
    float distance = (us * 0.0343f) / 2.0f;

    // imprime no monitor serial
    Serial.print("Pulso(us): ");
    Serial.print(us);
    Serial.print("  | Distancia(cm): ");
    Serial.println(distance);

    // envia o valor via BLE
    distanceChar.writeValue(distance);
  }

  // Se já não estamos à espera de eco e não há medição pendente, dispara de novo
  if (!awaitingEcho && !echoDone) {
    triggerPing();
  }
}

