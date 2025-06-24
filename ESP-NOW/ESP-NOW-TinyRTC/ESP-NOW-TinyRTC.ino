#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Wire.h>
#include "RTClib.h"

// Configuração do TinyRTC
RTC_DS1307 rtc;

// Endereço MAC do receptor
uint8_t broadcastAddress[] = {0xCC, 0x7B, 0x5C, 0x4F, 0x98, 0x80}; //MAC real do receptor

// Estrutura para os dados do RTC
struct RTCData {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint32_t timestamp;
};

// Função callback do espnow
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("Status do envio: ");
  if (sendStatus == 0) {
    Serial.println("Sucesso");
  } else {
    Serial.println("Falha");
  }
}

void setup() {
  Serial.begin(9600);
  
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

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Inicializa ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Erro ao iniciar ESP-NOW");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(OnDataSent);
  
  // Adiciona o peer
  if (esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0) != 0) {
    Serial.println("Falha ao adicionar peer");
    return;
  }
}

void loop() {
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
  esp_now_send(broadcastAddress, (uint8_t *) &data, sizeof(data));

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
String payload = "{"
  "\"v\": 1,"
  "\"src\": \"esp8266-tempo\","
  "\"dst\": \"esp8266-tempo\","
  "\"type\": \"time\","
  "\"ts\": " + String(data.timestamp) + ","
  "\"payload\": {"
    "\"date\": \"" + String(data.year) + "-" + 
                 (data.month < 10 ? "0" : "") + String(data.month) + "-" + 
                 (data.day < 10 ? "0" : "") + String(data.day) + "\","
    "\"time\": \"" + (data.hour < 10 ? "0" : "") + String(data.hour) + ":" + 
                 (data.minute < 10 ? "0" : "") + String(data.minute) + ":" + 
                 (data.second < 10 ? "0" : "") + String(data.second) + "\""
  "}"
"}";

  esp_now_send(broadcastAddress, (uint8_t *)payload.c_str(), payload.length());
  Serial.println("Envelope JSON enviado:");
  Serial.println(payload);

  // Aguarda 1 segundo antes da próxima leitura
  delay(1000);
}