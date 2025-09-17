/*
  ___ _  __            _   
 | _ |_)/ _|_ _ ___ __| |_ 
 | _ \ |  _| '_/ _ (_-<  _|
 |___/_|_| |_| \___/__/\__|

 Bifrost - Mqtt Pong  

 Esse código ouve pings via MQTT no padrão Envelope Bifrost e então responde com pongs.

*/

#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <ArduinoJson.h>
#include <ArduinoLog.h>
#include <PubSubClient.h>

#include "settings.h"

// Tópicos MQTT
const char* MQTT_SUB_TOPIC = "sensores/pong/command";  // Tópico para receber os pings
const char* MQTT_PUB_TOPIC = "sensores/pong/data";     // Tópico para enviar os pongs

// ID do dispositivo
const String DEVICE_TYPE = "pong";
const String DEVICE_ID   = "esp8266-mqtt-pong";

// Clientes
WiFiClient espClient;
PubSubClient client(espClient);


void setup() {
  Serial.begin(115200);
  while(!Serial && !Serial.available()){}

  // Os níveis disponíveis são:
  // LOG_LEVEL_SILENT, LOG_LEVEL_FATAL, LOG_LEVEL_ERROR, LOG_LEVEL_WARNING, LOG_LEVEL_INFO, LOG_LEVEL_TRACE, LOG_LEVEL_VERBOSE
  Log.begin(LOG_LEVEL_WARNING, &Serial);
  Log.notice(F("Inicializando o dispositivo Pong..."));

  // Configura o Wifi, servidor e a função de callback do MQTT
  setup_wifi();
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(mqttCallback);
}

void loop() {
  // O dispositivo só responde quando recebe algo no callback, portanto só usamos o loop para
  // tratar as desconexões e para o loop da conexão com a rede. 
  if (!client.connected()) {
    reconnect();
  }
  
  client.loop();
}

void setup_wifi() {
  delay(10);

  Log.notice(F(CR "Conectando-se a %s" CR), WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Aguarda até o wifi conectar
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Log.notice(".");
  }

  Log.notice(F( CR "Wi-Fi conectado!" CR));
  Log.notice(F("Endereço IP: %p" CR), WiFi.localIP());
}


void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Log.verboseln("Mensagem recebida no tópico: %C", topic);

  // Converte o payload para um formato que o ArduinoJson entenda
  char buffer[length + 1];
  memcpy(buffer, payload, length);
  buffer[length] = '\0';

  // Parseia o JSON recebido
  DynamicJsonDocument doc(240);
  DeserializationError error = deserializeJson(doc, buffer);

  if (error) {
    Log.verboseln("Erro ao parsear JSON: %s", error.c_str());
    return;
  }

  // Extrai os dados para verificação
  const char* type = doc["type"];
  const char* state = doc["payload"]["state"];

  // Verifica se é uma mensagem de ping
  if (strcmp(type, "echo") == 0 && strcmp(state, "ping") == 0) {
    Log.verboseln("Ping recebido! Enviando pong de volta...");

    // Cria o documento JSON de resposta
    DynamicJsonDocument responseDoc(240);
    responseDoc["v"] = 1;
    responseDoc["src"] = DEVICE_ID;
    responseDoc["dst"] = doc["src"];
    responseDoc["type"] = "echo_response";
    responseDoc["protocol"] = "mqtt"; 
    responseDoc["ts"] = doc["ts"];

    JsonObject payloadOut = responseDoc.createNestedObject("payload");
    payloadOut["state"] = DEVICE_TYPE;
    payloadOut["seq"] = doc["payload"]["seq"]; // Devolve o mesmo número de sequência

    // Serializa o JSON para uma string
    char jsonBuffer[240];
    serializeJson(responseDoc, jsonBuffer);

    // Publica a resposta no tópico de dados
    client.publish(MQTT_PUB_TOPIC, jsonBuffer);
    Log.verboseln("Pong enviado para o tópico: ", MQTT_PUB_TOPIC);
  }
}


void reconnect() {
  // Loopa o código até o esp se reconectar
  while (!client.connected()) {
    Log.warningln("Tentando conectar ao Broker MQTT...");

    // Usamos o device_id para tentar cadastrar no broker
    if (client.connect(DEVICE_ID.c_str())) {
      Log.warningln("Conectado!");

      // Assina o tópico para receber os comandos (pings)
      client.subscribe(MQTT_SUB_TOPIC);
      Log.warningln("Inscrito no tópico: %s", MQTT_SUB_TOPIC);
    } else {
      Log.fatalln("Falha, rc=%i", client.state());
      Log.fatalln(" Tentando novamente em 5 segundos");

      // Espera 5 segundos antes de tentar novamente
      delay(5000);
    }
  }
}