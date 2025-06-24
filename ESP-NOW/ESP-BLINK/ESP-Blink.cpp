#include <ESP8266WiFi.h>
#include <espnow.h>
#include <ArduinoJson.h>

// Configurações do dispositivo
#define LED_BUILTIN 2
uint8_t centralMac[] = {0xCC, 0x7B, 0x5C, 0x4F, 0x98, 0x80};

// Estrutura para mensagens
typedef struct struct_message {
  char data[200];
} struct_message;

// Variáveis globais
bool registered = false;
String macAddress = WiFi.macAddress();
String device_id = "ESP_Blink";
unsigned long lastRegisterAttempt = 0;
const unsigned long registerInterval = 10000; // 10 segundos

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != 0) {
    Serial.println("Erro ao inicializar ESP-NOW");
    ESP.restart();
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  
  // Configura peer da central
  esp_now_add_peer(centralMac, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
  
  Serial.println("Dispositivo iniciado. Tentando registrar...");
  Serial.print("ID do dispositivo: ");
  Serial.println(device_id);
}

void loop() {
  // Tentativa de registro periódica
  if (!registered && (millis() - lastRegisterAttempt > registerInterval)) {
    attemptRegistration();
    lastRegisterAttempt = millis();
  }
  
  delay(100);
}

void attemptRegistration() {
  DynamicJsonDocument doc(200);
  doc["v"] = 1;
  doc["src"] = macAddress;
  doc["dst"] = "central";
  doc["type"] = "register";
  doc["protocol"] = "espnow";
  
  JsonObject payload = doc.createNestedObject("payload");
  payload["id"] = device_id;
  
  sendMessage(doc);
  Serial.println("Enviando solicitação de registro...");
}

void sendMessage(JsonDocument& doc) {
  String jsonStr;
  serializeJson(doc, jsonStr);
  
  // Envia tanto por Serial quanto por ESP-NOW
  Serial.println(jsonStr);
  
  struct_message message;
  strlcpy(message.data, jsonStr.c_str(), sizeof(message.data));
  
  esp_now_send(centralMac, (uint8_t *)&message, sizeof(message));
}

void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  char payload[len + 1];
  memcpy(payload, incomingData, len);
  payload[len] = '\0';
  
  Serial.print("Dados recebidos: ");
  Serial.println(payload);

  DynamicJsonDocument doc(200);
  DeserializationError error = deserializeJson(doc, payload);
  
  if (error) {
    Serial.print("Erro ao parsear JSON: ");
    Serial.println(error.c_str());
    return;
  }

  // Processa resposta de registro
  if (doc["dst"] == macAddress && doc["type"] == "register_response") {
    String status = doc["payload"]["status"];
    if (status == "registered") {
      registered = true;
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.println("Registro confirmado pela central!");
    }
  }
  
  // Processa comandos da central
  if (doc["dst"] == macAddress && doc["type"] == "command") {
    processCommand(doc["payload"]);
  }
}

void processCommand(JsonObject payload) {
  if (payload.containsKey("led")) {
    int ledState = payload["led"];
    digitalWrite(LED_BUILTIN, ledState ? LOW : HIGH);
    Serial.print("LED alterado para: ");
    Serial.println(ledState);
  }
}

void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  if (sendStatus == 0) {
    Serial.println("Envio bem-sucedido");
  } else {
    Serial.println("Falha no envio");
    // Tenta reconectar se houver falha
    esp_now_add_peer(centralMac, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
  }
}