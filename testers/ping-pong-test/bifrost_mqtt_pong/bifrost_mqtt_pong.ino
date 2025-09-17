/**
 * @file espnow_mqtt_pong.ino
 * @brief Ouve pings via MQTT e responde com pongs.
 *
 * @author Thales Martins
 * @version 1.0
 * @date 17/09/2025
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// --- Configurações de Wi-Fi ---
const char* WIFI_SSID = "bifa";
const char* WIFI_PASSWORD = "arduinagem";

// --- Configurações do Broker MQTT ---
const char* MQTT_BROKER = "10.42.0.1";
const int MQTT_PORT = 1883;

// --- Tópicos MQTT ---
const char* MQTT_SUB_TOPIC = "sensores/pong/command"; // Tópico para receber os pings
const char* MQTT_PUB_TOPIC = "sensores/pong/data";    // Tópico para enviar os pongs

// --- Identidade do Dispositivo ---
const String device_type = "pong";
const String device_id = "esp8266-mqtt-pong";
const String destination_id = "esp8266-mqtt-ping"; // ID do dispositivo que originou o ping

// --- Clientes ---
WiFiClient espClient;
PubSubClient client(espClient);

// Protótipos de funções
void setup_wifi();
void reconnect();
void mqttCallback(char* topic, byte* payload, unsigned int length);

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Inicializando o dispositivo Pong...");

  setup_wifi();

  // Configura o servidor e a função de callback do MQTT
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(mqttCallback);
}

void loop() {
  // Se não estiver conectado, tenta reconectar
  if (!client.connected()) {
    reconnect();
  }
  // Mantém o cliente MQTT processando mensagens e a conexão viva
  client.loop();
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando-se a ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi conectado!");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida no tópico: ");
  Serial.println(topic);

  // Converte o payload para um formato que o ArduinoJson entenda
  char buffer[length + 1];
  memcpy(buffer, payload, length);
  buffer[length] = '\0';

  // Parseia o JSON recebido
  DynamicJsonDocument doc(240);
  DeserializationError error = deserializeJson(doc, buffer);

  if (error) {
    Serial.print("Erro ao parsear JSON: ");
    Serial.println(error.c_str());
    return;
  }

  // Extrai os dados para verificação
  const char* type = doc["type"];
  const char* state = doc["payload"]["state"];

  // Verifica se é uma mensagem de ping
  if (strcmp(type, "echo") == 0 && strcmp(state, "ping") == 0) {
    Serial.println("Ping recebido! Enviando pong de volta...");

    // Cria o documento JSON de resposta
    DynamicJsonDocument responseDoc(240);
    responseDoc["v"] = 1;
    responseDoc["src"] = device_id;
    responseDoc["dst"] = destination_id;
    responseDoc["type"] = "echo_response";
    responseDoc["protocol"] = "mqtt"; // Protocolo agora é MQTT
    responseDoc["ts"] = doc["ts"];    // Mantém o timestamp original

    JsonObject payloadOut = responseDoc.createNestedObject("payload");
    payloadOut["state"] = "pong";
    payloadOut["seq"] = doc["payload"]["seq"]; // Devolve o mesmo número de sequência

    // Serializa o JSON para uma string
    char jsonBuffer[240];
    serializeJson(responseDoc, jsonBuffer);

    // Publica a resposta no tópico de dados
    client.publish(MQTT_PUB_TOPIC, jsonBuffer);
    Serial.print("Pong enviado para o tópico: ");
    Serial.println(MQTT_PUB_TOPIC);
  }
}

void reconnect() {
  // Loop até reconectar
  while (!client.connected()) {
    Serial.print("Tentando conectar ao Broker MQTT...");
    // Tenta conectar usando o device_id como client ID
    if (client.connect(device_id.c_str())) {
      Serial.println("Conectado!");
      // Assina o tópico para receber os comandos (pings)
      client.subscribe(MQTT_SUB_TOPIC);
      Serial.print("Inscrito no tópico: ");
      Serial.println(MQTT_SUB_TOPIC);
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 5 segundos");
      // Espera 5 segundos antes de tentar novamente
      delay(5000);
    }
  }
}