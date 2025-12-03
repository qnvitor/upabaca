#include <WiFi.h>
#include <time.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <HTTPClient.h>
#include "chaves.h"

// ---------------- CONFIGURAÇÕES DE PINOS ----------------
#define LDR_PIN 17
#define SOIL_PIN 34
#define DHT_PIN 23
#define LED_PIN 26
#define BOIA_PIN 27
#define RELE_PIN 14

// ---------------- CONFIGURAÇÕES DO DHT -------------------
#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

// ---------------- CONFIGURAÇÕES DE TEMPO -----------------
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = -3 * 3600; // UTC-3
const int DAYLIGHT_OFFSET_SEC = 0;

// ---------------- LIMITES E CONSTANTES -------------------
const int SOIL_MOISTURE_THRESHOLD = 1200;
const int WIFI_MAX_ATTEMPTS = 15;
const int LED_BLINK_SHORT = 200;
const int LED_BLINK_LONG = 300;
const int PUMP_ON_TIME_MS = 3000;
const int LOOP_DELAY_MS = 15000;

// ---------------- HORÁRIOS PARA REGRAS -------------------
const int NIGHT_START_HOUR = 18;  // Início da noite
const int NIGHT_END_HOUR   = 12;   // Fim da noite

bool isNightHour(int hour) {
  return (hour >= NIGHT_START_HOUR || hour < NIGHT_END_HOUR);
}

// ---------------- VARIÁVEIS DE ESTADO --------------------
bool wifiConnected = false;
bool timeValid = false;

// ---------------- PROTÓTIPOS -----------------------------
void connectWiFi();
bool updateLocalTime();
void blinkLED(int times, int delayTime);
void sendToThingSpeak(int luz, int solo, float umid, float temp, int bomba, int agua);
void handleWateringLogic(int soil, bool haAgua, int hour);
void printSensorData(int lightState, int soil, float humidity, float temperature, bool haAgua);

// ==========================================================
// SETUP
// ==========================================================
void setup() {
  Serial.begin(115200);

  pinMode(RELE_PIN, OUTPUT);
  digitalWrite(RELE_PIN, HIGH);

  dht.begin();

  pinMode(LDR_PIN, INPUT);
  pinMode(SOIL_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BOIA_PIN, INPUT_PULLUP);

  Serial.println("Sistema IoT de Monitoramento de Planta iniciado!");

  connectWiFi();
  if (wifiConnected) updateLocalTime();
}

// ==========================================================
// CONECTAR AO WI-FI
// ==========================================================
void connectWiFi() {
  Serial.print("Conectando ao Wi-Fi...");
  WiFi.begin(WIFI_NAME, WIFI_SENHA);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < WIFI_MAX_ATTEMPTS) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi conectado!");
    wifiConnected = true;

    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    delay(2000);
  } else {
    Serial.println("\nFalha ao conectar ao Wi-Fi. Continuando offline...");
    wifiConnected = false;
  }
}

// ==========================================================
// ATUALIZAR HORA DO SISTEMA
// ==========================================================
bool updateLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Falha ao obter hora.");
    timeValid = false;
    return false;
  }

  Serial.print("Hora atual: ");
  Serial.println(&timeinfo, "%H:%M:%S");
  timeValid = true;
  return true;
}

// ==========================================================
// PISCAR LED
// ==========================================================
void blinkLED(int times, int delayTime) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(delayTime);
    digitalWrite(LED_PIN, LOW);
    delay(delayTime);
  }
}

// ==========================================================
// ENVIAR DADOS AO THINGSPEAK
// ==========================================================
void sendToThingSpeak(int luz, int solo, float umidAr, float temp, int bomba, int agua) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sem Wi-Fi, não enviando ao ThingSpeak.");
    return;
  }

  HTTPClient http;
  String url =
    String("http://api.thingspeak.com/update?api_key=") + WRITE_KEY +
    "&field1=" + luz +
    "&field2=" + solo +
    "&field3=" + umidAr +
    "&field4=" + temp +
    "&field5=" + bomba +
    "&field6=" + agua;

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    Serial.println("ThingSpeak atualizado! Código: " + String(httpCode));
  } else {
    Serial.println("Erro ao enviar ao ThingSpeak.");
  }

  http.end();
}

// ==========================================================
// PRINTAR DADOS NO SERIAL
// ==========================================================
void printSensorData(int lightState, int soil, float humidity, float temperature, bool haAgua) {
  Serial.println("-------------------------------------------------");
  Serial.print("Luminosidade (LDR): ");
  Serial.println(lightState == LOW ? "Alta" : "Baixa");

  Serial.print("Umidade do Solo: ");
  Serial.println(soil);

  Serial.print("Água no reservatório: ");
  Serial.println(haAgua ? "OK" : "BAIXA");

  Serial.print("Umidade do Ar: ");
  Serial.print(humidity);
  Serial.println("%");

  Serial.print("Temperatura: ");
  Serial.print(temperature);
  Serial.println("°C");
}

// ==========================================================
// LÓGICA DE IRRIGAÇÃO
// ==========================================================
void handleWateringLogic(int soil, bool haAgua, int hour) {
  bool drySoil = soil > SOIL_MOISTURE_THRESHOLD;
  bool isNight = isNightHour(hour);
  int bombaLigada = 0;

  if (drySoil && isNight && !haAgua) {
    Serial.println("Encher reservatório!");
    blinkLED(20, LED_BLINK_SHORT);
    digitalWrite(RELE_PIN, HIGH);  // Bomba desligada
  }

  else if (drySoil && isNight && haAgua) {
    Serial.println("Horário certo, ligando a bomba!");

    digitalWrite(LED_PIN, HIGH);
    digitalWrite(RELE_PIN, LOW);   // Liga bomba

    bombaLigada = 1;
    delay(PUMP_ON_TIME_MS);

    digitalWrite(RELE_PIN, HIGH);  // Desliga bomba
    digitalWrite(LED_PIN, LOW);
  }

  else if (drySoil) {
    Serial.println("Planta precisa de água!");
    blinkLED(5, LED_BLINK_LONG);
    digitalWrite(RELE_PIN, HIGH);
  }

  else {
    Serial.println("Planta em boas condições.");
    digitalWrite(RELE_PIN, HIGH);
    digitalWrite(LED_PIN, LOW);
  }

  // Enviar ao ThingSpeak
  sendToThingSpeak(
    digitalRead(LDR_PIN),
    soil,
    dht.readHumidity(),
    dht.readTemperature(),
    bombaLigada,
    haAgua
  );
}

// ==========================================================
// LOOP PRINCIPAL
// ==========================================================
void loop() {

  // Verifica conexão Wi-Fi
  if (WiFi.status() != WL_CONNECTED && wifiConnected) {
    Serial.println("Wi-Fi desconectado! Tentando reconectar...");
    wifiConnected = false;
    connectWiFi();
  }

  // Atualiza hora caso ainda não esteja válida
  if (wifiConnected && !timeValid) updateLocalTime();

  // ----- Leitura dos sensores -----
  int lightState = digitalRead(LDR_PIN);
  int soilValue = analogRead(SOIL_PIN);
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  bool haAgua = digitalRead(BOIA_PIN) == LOW;

  printSensorData(lightState, soilValue, humidity, temperature, haAgua);

  // ----- Hora atual -----
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  int hour = timeinfo.tm_hour;

  // ----- Lógica de irrigação -----
  handleWateringLogic(soilValue, haAgua, hour);

  updateLocalTime();
  delay(LOOP_DELAY_MS);
}
