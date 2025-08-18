#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Wire.h>
#include <RTClib.h>
#include <ArduinoJson.h>

// Configuração do TinyRTC
RTC_DS1307 rtc;

// Endereço MAC do receptor (central)
uint8_t centralMac[] = {0xCC, 0x7B, 0x5C, 0x4F, 0x98, 0x80};

// Estrutura para mensagens ESP-NOW
typedef struct struct_message {
  char data[240];
} struct_message;

// Estrutura para os dados do RTC
typedef struct RTCData {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint32_t timestamp;
} RTCData;

// Variáveis globais
bool registered = false;
String macAddress = WiFi.macAddress();
String device_id = "ESP_RTC";
unsigned long lastRegisterAttempt = 0;
const unsigned long registerInterval = 10000; // 10 segundos

// Funções declaradas
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus);
void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len);
void attemptRegistration();
void sendMessage(JsonDocument& doc);
void sendRTCData();

void setup() {
  Serial.begin(9600);
  Serial.println("\nIniciando dispositivo...");
  
  // Inicializa I2C para o RTC
  Wire.begin(D2, D1); // SDA=D2 (GPIO4), SCL=D1 (GPIO5)
  
  // Inicializa o RTC
  if (!rtc.begin()) {
    Serial.println("Não foi possível encontrar o RTC");
    while (1);
  }

  // Se o RTC não estiver rodando, configura com a hora de compilação
  if (!rtc.isrunning()) {
    Serial.println("RTC não está rodando, configurando hora...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
  // Inicializa Wi-Fi no modo Station
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Inicializa ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Erro ao inicializar ESP-NOW");
    ESP.restart();
    return;
  }

  // Configura o dispositivo como COMBO (envia e recebe)
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  
  // Adiciona a central como peer (ESP8266 API)
  if (esp_now_add_peer(centralMac, ESP_NOW_ROLE_COMBO, 1, NULL, 0) != 0) {
    Serial.println("Falha ao adicionar peer");
    ESP.restart();
    return;
  }
  
  Serial.println("Dispositivo iniciado. Tentando registrar...");
  Serial.print("ID do dispositivo: ");
  Serial.println(device_id);
  
  // Envia solicitação de registro imediatamente
  attemptRegistration();
}

void loop() {
  // Tenta registrar periodicamente até receber confirmação
  if (!registered && (millis() - lastRegisterAttempt > registerInterval)) {
    attemptRegistration();
    lastRegisterAttempt = millis();
  }
  
  // Se registrado, envia dados do RTC a cada 1 segundo
  if (registered) {
    sendRTCData();
  }
  
  delay(1000); // Aguarda 1 segundo entre iterações
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
  
  sendMessage(doc);
  Serial.println("Enviando solicitação de registro...");
}

void sendMessage(JsonDocument& doc) {
  String jsonStr;
  serializeJson(doc, jsonStr);
  
  // Envia mensagem pelo Serial e ESP-NOW
  Serial.println(jsonStr);
  
  struct_message message;
  strlcpy(message.data, jsonStr.c_str(), sizeof(message.data));
  
  esp_now_send(centralMac, (uint8_t *)&message, sizeof(message));
}

void sendRTCData() {
  // Obtém a data e hora atual do RTC
  DateTime now = rtc.now();
  
  // Prepara os dados para envio
  RTCData data;
  data.year = now.year();
  data.month = now.month();
  data.day = now.day();
  data.hour = now.hour();
  data.minute = now.minute();
  data.second = now.second();
  data.timestamp = now.unixtime();

  // Envia os dados via ESP-NOW
  esp_now_send(centralMac, (uint8_t *)&data, sizeof(data));

  // Exibe os dados no monitor serial
  Serial.print("Data/Hora: ");
  Serial.print(data.year); Serial.print('/');
  Serial.print(data.month); Serial.print('/');
  Serial.print(data.day); Serial.print(' ');
  Serial.print(data.hour); Serial.print(':');
  Serial.print(data.minute); Serial.print(':');
  Serial.println(data.second);
  Serial.print("Timestamp UNIX: ");
  Serial.println(data.timestamp);

  // Cria e envia o payload JSON
  DynamicJsonDocument doc(240);
  doc["v"] = 1;
  doc["src"] = macAddress;
  doc["dst"] = "esp8266-tempo";
  doc["type"] = "time";
  doc["protocol"] = "espnow";
  doc["ts"] = data.timestamp;
  
  JsonObject payload = doc.createNestedObject("payload");
  payload["date"] = String(data.year) + "-" + 
                    (data.month < 10 ? "0" : "") + String(data.month) + "-" + 
                    (data.day < 10 ? "0" : "") + String(data.day);
  payload["time"] = (data.hour < 10 ? "0" : "") + String(data.hour) + ":" + 
                    (data.minute < 10 ? "0" : "") + String(data.minute) + ":" + 
                    (data.second < 10 ? "0" : "") + String(data.second);
  
  sendMessage(doc);
}

void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("Status do envio: ");
  if (sendStatus == 0) {
    Serial.println("Sucesso");
  } else {
    Serial.println("Falha");
    // Tenta reconectar o peer
    esp_now_add_peer(centralMac, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
  }
}

void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  char payload[len + 1];
  memcpy(payload, incomingData, len);
  payload[len] = '\0';
  
  Serial.print("Dados recebidos via ESP-NOW: ");
  Serial.println(payload);

  DynamicJsonDocument doc(240);
  DeserializationError error = deserializeJson(doc, payload);
  
  if (error) {
    Serial.print("Erro ao parsear JSON: ");
    Serial.println(error.c_str());
    return;
  }

  // Processa resposta de registro
  if (doc["dst"] == macAddress && doc["type"] == "register_response") {
    String status = doc["payload"]["status"];
    if (status == "registered" || status == "already_registered") {
      registered = true;
      Serial.println("Registro confirmado pela central! Status: " + status);
    }
  }
}
