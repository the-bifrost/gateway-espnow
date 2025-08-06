#include <ESP8266WiFi.h>
#include <espnow.h>
#include <DHT.h>

// Configuração do DHT11
#define DHTPIN D1      // Pino D0 conectado ao DHT11
#define DHTTYPE DHT11  // Tipo do sensor
DHT dht(DHTPIN, DHTTYPE);

// Endereço MAC do receptor
uint8_t broadcastAddress[] = {0xCC, 0x7B, 0x5C, 0x4F, 0x98, 0x80}; //MAC real do receptor

// Estrutura para os dados do sensor
struct SensorData {
  float temperature;
  float humidity;
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
  pinMode(D0, INPUT);
  Serial.begin(9600);
  
  // Inicializa o DHT11
  dht.begin();
  
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
  // Aguarda 2 segundos entre as leituras (DHT11 tem taxa de amostragem lenta)
  delay(2000);
  
  // Lê os dados do sensor
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  // Verifica se a leitura foi bem sucedida
  if (isnan(h) || isnan(t)) {
    Serial.println("Falha ao ler o sensor DHT11!");
    return;
  }

  // Prepara os dados para envio
  SensorData data;
  data.temperature = t;
  data.humidity = h;

  // Envia os dados via ESP-NOW
  esp_now_send(broadcastAddress, (uint8_t *) &data, sizeof(data));

  // Exibe os dados no monitor serial
  Serial.print("Temperatura: ");
  Serial.print(t);
  Serial.print(" °C, Umidade: ");
  Serial.print(h);
  Serial.println(" %");

  // Cria e envia o payload JSON (opcional, mantendo seu formato original)
  String payload = "{"
    "\"v\": 1,"
    "\"src\": \"esp8266\","
    "\"dst\": \"esp32\","
    "\"type\": \"state\","
    "\"ts\": " + String(millis()) + ","
    "\"payload\": {"
      "\"temperature\": " + String(t) + ","
      "\"humidity\": " + String(h) + 
    "}"
  "}";

  esp_now_send(broadcastAddress, (uint8_t *)payload.c_str(), payload.length());
  Serial.println("Envelope JSON enviado:");
  Serial.println(payload);
}