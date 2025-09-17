/**
 * @file bifrost_espnow_ping.ino
 * @brief Envia uma payload ping para um dispositivo e mede a latência (RTT).
 *
 * @author Thales Martins
 * @version 1.1
 * @date 03/09/2025
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <ArduinoJson.h>

// --- Configurações do Ping ---
const int MAX_PINGS = 200;      // Número total de pings a serem enviados
const unsigned long PING_INTERVAL = 1000; // Intervalo entre pings em ms
const unsigned long PING_TIMEOUT = 2000;  // Tempo máximo de espera por uma resposta em ms

// --- Variáveis de estado do Ping ---
int pingCount = 0;
unsigned long pingStartTime = 0;
unsigned long lastPingSentTime = 0;
bool waitingForPong = false;
unsigned long totalRtt = 0;
int successfulPongs = 0;

/// ESP-NOW
bool registered = false;
String macAddress = WiFi.macAddress();
const String device_id = "esp8266-mqtt-ping";
const String destination_id = "esp8266-mqtt-pong"; // ID do dispositivo que responderá
const String device_type = "ping";
// IMPORTANTE: Coloque aqui o MAC Address do seu dispositivo "pong"
uint8_t broadcastAddress[] = {0xCC, 0x7B, 0x5C, 0x4F, 0x98, 0x80};

unsigned long lastRegisterAttempt = 0;
const unsigned long registerInterval = 10000;

// Callback para quando os dados são recebidos
void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len);
// Callback para quando os dados são enviados
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus);

typedef struct struct_message {
  char data[240];
} struct_message;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Inicializando Dispositivo PING...");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != 0) {
    Serial.println("ERRO: Falha ao iniciar ESP-NOW");
    ESP.restart();
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);

  if (esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMBO, 1, NULL, 0) != 0) {
    Serial.println("Erro ESP-Now: Falha ao adicionar peer");
  }

  attemptRegistration();
}

void loop() {
  // Tenta registrar se ainda não estiver registrado
  if (!registered && (millis() - lastRegisterAttempt > registerInterval)) {
    attemptRegistration();
    lastRegisterAttempt = millis();
  }

  // Se registrado, inicia a sequência de pings
  if (registered && pingCount < MAX_PINGS) {
    // Verifica se não está esperando um pong e se o intervalo já passou
    if (!waitingForPong && (millis() - lastPingSentTime > PING_INTERVAL)) {
      sendPing();
    }
  }

  // Verifica se um ping excedeu o tempo limite
  if (waitingForPong && (millis() - pingStartTime > PING_TIMEOUT)) {
    Serial.println("ERRO: Timeout! Nenhuma resposta pong recebida.");
    waitingForPong = false; // Permite o envio do próximo ping
    pingCount++; // Conta como uma tentativa falha
    lastPingSentTime = millis();
  }

  if (pingCount >= MAX_PINGS) {
    sendResults();
    delay(1000);
    ESP.restart();
  }
}

void attemptRegistration() {
  DynamicJsonDocument doc(240);
  doc["v"] = 1;
  doc["src"] = macAddress;
  doc["dst"] = "central";
  doc["type"] = "register";
  doc["protocol"] = "espnow";
  
  JsonObject payload = doc.createNestedObject("payload");
  payload["id"] = device_id;
  payload["device_type"] = device_type;
  
  Serial.println("Enviando solicitação de registro...");
  sendMessage(doc);
}

void sendPing() {
  DynamicJsonDocument doc(240);
  doc["v"] = 1;
  doc["src"] = device_id;
  doc["dst"] = destination_id;
  doc["protocol"] = "espnow";
  doc["type"] = "echo"; // Usamos um tipo "echo" para a requisição
  
  // O timestamp é a parte mais importante para o cálculo
  pingStartTime = millis();
  doc["ts"] = pingStartTime;

  JsonObject payload = doc.createNestedObject("payload");
  payload["state"] = "ping";
  payload["seq"] = pingCount + 1; // Número de sequência do ping
  
  Serial.printf("Enviando Ping #%d...\n", pingCount + 1);
  waitingForPong = true;
  sendMessage(doc);
}

void sendMessage(JsonDocument& doc) {
  char buffer[240];
  size_t n = serializeJson(doc, buffer);
  esp_now_send(broadcastAddress, (uint8_t *)buffer, n);
}

void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  if (sendStatus != 0) {
    Serial.println("Falha no envio do pacote ESP-NOW.");
  }
}

void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  char buffer[len + 1];
  memcpy(buffer, incomingData, len);
  buffer[len] = '\0';

  DynamicJsonDocument doc(240);
  DeserializationError error = deserializeJson(doc, buffer);

  if (error) {
    Serial.print("Erro ao parsear JSON recebido: ");
    Serial.println(error.c_str());
    return;
  }
  
  String type = doc["type"];
  String dst = doc["dst"];

  // Processa resposta de registro
  if (dst == macAddress && type == "register_response") {
    String status = doc["payload"]["status"];
    if (status == "registered" || status == "already_registered") {
      registered = true;
      Serial.println("Registro confirmado pela central! Iniciando pings...");
    }
  }
  // Processa resposta de PONG
  else if (dst == macAddress && type == "echo_response") {
    String state = doc["payload"]["state"];
    if (state == "pong" && waitingForPong) {
      unsigned long rtt = millis() - pingStartTime;
      totalRtt += rtt;
      successfulPongs++;
      
      Serial.printf("Pong #%d recebido! Latência (RTT): %lu ms\n", doc["payload"]["seq"].as<int>(), rtt);
      
      waitingForPong = false;
      pingCount++;
      lastPingSentTime = millis();

      if (pingCount >= MAX_PINGS) {
        Serial.println("\n--- Sequência de Pings Concluída ---");
        if(successfulPongs > 0) {
            Serial.printf("Pings bem-sucedidos: %d/%d\n", successfulPongs, MAX_PINGS);
            Serial.printf("Latência Média: %.2f ms\n", (float)totalRtt / successfulPongs);
        } else {
            Serial.println("Nenhum ping foi respondido com sucesso.");
        }
      }
    }
  }
}

void sendResults() {
  DynamicJsonDocument doc(240);
  doc["v"] = 1;
  doc["src"] = device_id;
  doc["dst"] = "central";
  doc["protocol"] = "espnow";
  doc["type"] = "lat";

  JsonObject payload = doc.createNestedObject("payload");
  payload["lat"] = (float)totalRtt / successfulPongs;
  payload["send"] = MAX_PINGS;
  payload["success"] = successfulPongs;
  
  Serial.printf("Enviando Total\n");
  sendMessage(doc);
}