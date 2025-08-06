//SENDER ESP-NOW ESP8266 WEMOS
#include <ESP8266WiFi.h>
#include <espnow.h>

// Endereço MAC do receptor
uint8_t broadcastAddress[] = {0xCC, 0x7B, 0x5C, 0x4F, 0x98, 0x80}; //MAC real do receptor

// Função callback do espnow
void onSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("Status do envio: ");
  Serial.println(sendStatus == 0 ? "Sucesso" : "Falha");
}

void setup() {

  Serial.begin(9600);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != 0) {
    Serial.println("Erro ao iniciar ESP-NOW");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(onSent);
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
}

  // Monta o envelope JSON completo com dados de teste
  // Estamos usando dados fixos como exemplo
void loop() {
  // MENSAGEM MQTT PRO TOPICO "/home/esp32"
  String payload = "{"
    "\"v\": 1,"
    "\"src\": \"thales\","
    "\"dst\": \"esp32\","
    "\"type\": \"state\","
    "\"ts\": 1728102345,"
    "\"payload\": {"
      "\"temperature\": 23.5,"
      "\"humidity\": 60"
    "}"
  "}";

  esp_now_send(broadcastAddress, (uint8_t *)payload.c_str(), payload.length());

  Serial.println("Envelope enviado:");
  Serial.println(payload);
  delay(1000);

  // MENSAGEM MQTT PRO TOPICO "/home/device1"
payload = "{"
  "\"v\": 1,"
  "\"src\": \"kerlon\","
  "\"dst\": \"device1\","
  "\"type\": \"state\","
  "\"ts\": 16211453659,"
  "\"payload\": {"
    "\"nome\": \"thalesmartins\","
    "\"humildade\": 69"
  "}"
"}";

  esp_now_send(broadcastAddress, (uint8_t *)payload.c_str(), payload.length());

  Serial.println("Envelope enviado:");
  Serial.println(payload);
  delay(1000);
}