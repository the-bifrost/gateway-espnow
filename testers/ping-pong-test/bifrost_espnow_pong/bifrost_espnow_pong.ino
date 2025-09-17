/**
 * @file espnow_pong_responder.ino
 * @brief Ouve pings via ESP-NOW e responde com pongs.
 *
 * @author Thales Martins
 * @version 1.0
 * @date 03/09/2025
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <ArduinoJson.h>

bool registered = false;
const String device_type = "pong";
const String device_id = "esp8266-pong";
const String destination_id = "esp8266-ping";
uint8_t broadcastAddress[] = {0xCC, 0x7B, 0x5C, 0x4F, 0x98, 0x80};

unsigned long lastRegisterAttempt = 0;
const unsigned long registerInterval = 10000;

String macAddress = "";

void OnDataRecv(uint8_t *mac_addr, uint8_t *incomingData, uint8_t len) {
  // Parseia o JSON recebido
  DynamicJsonDocument doc(240);
  DeserializationError error = deserializeJson(doc, incomingData, len);

  if (error) {
    Serial.print("Erro ao parsear JSON: ");
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
  else {
    
    // Verifica se é uma mensagem de ping
    String state = doc["payload"]["state"];
    
    if (type == "echo" && state == "ping") {
      Serial.println("Ping recebido! Enviando pong de volta...");

      // Cria o documento de resposta
      DynamicJsonDocument responseDoc(240);
      responseDoc["v"] = 1;
      responseDoc["src"] = device_id;
      responseDoc["dst"] = destination_id;
      responseDoc["type"] = "echo_response";
      responseDoc["protocol"] = "espnow";
      responseDoc["ts"] = doc["ts"];

      JsonObject payload = responseDoc.createNestedObject("payload");
      payload["state"] = "pong";
      payload["seq"] = doc["payload"]["seq"]; // Devolve o mesmo número de sequência
      
      sendMessage(responseDoc);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Inicializando Dispositivo PONG...");

  WiFi.mode(WIFI_STA);
  macAddress = WiFi.macAddress(); // Guarda o próprio MAC
  WiFi.disconnect();

  if (esp_now_init() != 0) {
    Serial.println("ERRO: Falha ao iniciar ESP-NOW");
    ESP.restart();
  }

  // O receptor pode ser um SLAVE ou COMBO
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(OnDataRecv);

  if (esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMBO, 1, NULL, 0) != 0) {
    Serial.println("Erro ESP-Now: Falha ao adicionar peer");
  }

  attemptRegistration();
}

void loop() {
   if (!registered && (millis() - lastRegisterAttempt > registerInterval)) {
    attemptRegistration();
    lastRegisterAttempt = millis();
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

void sendMessage(JsonDocument& doc) {
  char buffer[240];
  size_t n = serializeJson(doc, buffer);
  esp_now_send(broadcastAddress, (uint8_t *)buffer, n);
}