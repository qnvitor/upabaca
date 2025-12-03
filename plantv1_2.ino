// ==========================================================
// PROJETO: Sistema IoT de Monitoramento e Irrigação Automática
// PLATAFORMA: ESP32
//
// DESCRIÇÃO GERAL:
// Este código implementa um sistema IoT para monitoramento e irrigação
// automática de plantas, utilizando sensores ambientais, conexão Wi-Fi
// e envio de dados para o ThingSpeak. O sistema mede luminosidade,
// umidade do solo, temperatura, umidade do ar e nível do reservatório
// de água. Com base nessas leituras e no horário atual (obtido via NTP),
// ele decide quando acionar a bomba de água para irrigar a planta.
//
// PRINCIPAIS FUNCIONALIDADES:
// • Leitura dos sensores:
// - LDR → Detecta luminosidade (dia/noite)
// - Sensor de umidade do solo → Mede necessidade de irrigação
// - DHT11 → Temperatura e umidade do ar
// - Boia de nível → Verifica se há água no reservatório
//
// • Irrigação automática:
// - Só liga a bomba em horários configurados
// - Só irriga quando o solo está seco
// - Não aciona a bomba se o reservatório estiver sem água
//
// • Conexão Wi-Fi e sincronização de horário via NTP
// • Envio dos dados para o ThingSpeak para monitoramento remoto
// • LED indicador para alertas e status
// ==========================================================

#include <WiFi.h>
#include <time.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <HTTPClient.h>
#include "chaves.h"

// ---------------- CONFIGURAÇÕES DE PINOS ----------------
#define LDR_PIN 32      // Entrada analógica do sensor LDR (mede luminosidade)
#define SOIL_PIN 34     // Entrada analógica do sensor de umidade do solo
#define DHT_PIN 23      // Entrada do sensor DHT (temperatura e umidade do ar)
#define LED_PIN 26      // Saída para LED indicador (status do sistema)
#define BOIA_PIN 27     // Entrada da boia de nível de água (detecta reservatório cheio/vazio)
#define RELE_PIN 14     // Saída para o módulo relé (aciona bomba de água)

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
const int LDR_DARK_THRESHOLD = 1500;

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
void printSensorData(int lightValue, int soil, float humidity, float temperature, bool haAgua);

// ==========================================================
// SETUP
// ==========================================================
void setup() {
  Serial.begin(115200);               // Inicia comunicação serial para debugar (velocidade 115200 baud)

  pinMode(RELE_PIN, OUTPUT);          // Define o pino do relé como saída
  digitalWrite(RELE_PIN, HIGH);       // Mantém o relé desligado no início (nível HIGH = OFF na maioria dos relés)

  dht.begin();                        // Inicializa o sensor DHT (temperatura e umidade do ar)

  pinMode(LDR_PIN, INPUT);            // Configura o pino do sensor LDR como entrada
  pinMode(SOIL_PIN, INPUT);           // Configura o pino do sensor de umidade do solo como entrada
  pinMode(LED_PIN, OUTPUT);           // LED indicador como saída (para sinais visuais do sistema)
  pinMode(BOIA_PIN, INPUT_PULLUP);    // Pino da boia com resistor interno de pull-up (boia fecha para GND)

  Serial.println("Sistema IoT de Monitoramento de Planta iniciado!"); // Mensagem inicial de confirmação

  connectWiFi();                      // Tenta conectar ao Wi-Fi usando SSID e senha definidos em chaves.h
  
  if (wifiConnected) updateLocalTime(); // Se o Wi-Fi conectou, sincroniza o relógio via NTP
}

/// ==========================================================
// CONECTAR AO WI-FI
// ==========================================================
void connectWiFi() {
  Serial.println("Iniciando tentativa de conexão em redes salvas...");

  wifiConnected = false;

  // Tenta cada rede registrada em chaves.h
  for (int i = 0; i < WIFI_TOTAL; i++) {
    const char* ssid  = WIFI_LIST[i][0];
    const char* senha = WIFI_LIST[i][1];

    Serial.print("Tentando conectar em: ");
    Serial.println(ssid);

    WiFi.begin(ssid, senha);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < WIFI_MAX_ATTEMPTS) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    // Se conectou, parar o loop
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("\nConectado com sucesso em ");
      Serial.println(ssid);

      wifiConnected = true;

      // Configura horário NTP
      configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
      delay(2000);

      return; // Sai da função pois já conectou
    }

    Serial.println("\nFalha nesta rede, tentando próxima...\n");
  }

  // Se nenhuma rede conectou:
  Serial.println("Nenhuma das redes disponíveis funcionou. Continuando offline...");
  wifiConnected = false;
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
void printSensorData(int lightValue, int soil, float humidity, float temperature, bool haAgua) {
  Serial.println("-------------------------------------------------");
  Serial.print("Luminosidade: ");
  Serial.println(lightValue);

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
void handleWateringLogic(int soil, bool haAgua, int hour, int lightValue) {

  bool drySoil   = soil > SOIL_MOISTURE_THRESHOLD;
  bool isNight   = isNightHour(hour);
  bool isDark    = lightValue > LDR_DARK_THRESHOLD;
  int bombaLigada = 0;

  // ---------- Condições ----------
  // Para ligar a bomba agora exige:
  // 1. Solo seco
  // 2. Horário permitido (noite)
  // 3. Reservatório com água
  // 4. Luminosidade baixa (ambiente escuro)

  if (drySoil && isNight && !haAgua) {
    Serial.println("Encher reservatório!");
    blinkLED(20, LED_BLINK_SHORT);
    digitalWrite(RELE_PIN, HIGH);
  }

  else if (drySoil && isNight && haAgua && isDark) {
    Serial.println("Horário certo, baixa luminosidade — ligando a bomba!");

    digitalWrite(LED_PIN, HIGH);
    digitalWrite(RELE_PIN, LOW);  // Liga bomba

    bombaLigada = 1;
    delay(PUMP_ON_TIME_MS);

    digitalWrite(RELE_PIN, HIGH); // Desliga bomba
    digitalWrite(LED_PIN, LOW);
  }

  else if (drySoil && !isDark) {
    Serial.println("Solo seco, mas luminosidade está alta — não ligar a bomba.");
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
    lightValue,
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
  int lightValue = analogRead(LDR_PIN);
  int soilValue = analogRead(SOIL_PIN);
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  bool haAgua = digitalRead(BOIA_PIN) == LOW;

  printSensorData(lightValue, soilValue, humidity, temperature, haAgua);

  // ----- Hora atual -----
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  int hour = timeinfo.tm_hour;

  // ----- Lógica de irrigação -----
  handleWateringLogic(soilValue, haAgua, hour, lightValue);

  updateLocalTime();
  delay(LOOP_DELAY_MS);
}
