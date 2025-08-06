/**
 * @file  gateway-espnow.ino
 * @brief Implementa uma Unidade EspNow da Bifrost.
 *
 * Usa o envelopamento padrão de mensagens para fazer a leitura, validação e roteamento
 * dos dados do protocolo espnow. 
 */
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <espnow.h>

#define RX D5
#define TX D6
SoftwareSerial bifrostSerial(RX, TX);

/// Estrutura de dados da Bifrost
/// Usamos um struct para encapsular os blocos de dados, isso facilita
/// o envio dos dados espnow com esp_now_send().
typedef struct struct_message {
  char data[240];
} struct_message;

const int SERIAL_BAUDRATE = 115200;
const int BIFROST_BAUDRATE = 9600;

/**
 */
void setup() {
  Serial.begin(SERIAL_BAUDRATE);
  bifrostSerial.begin(BIFROST_BAUDRATE);

  Serial.println();
  Serial.println(F("Iniciando dispositivo..."));
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  wifi_promiscuous_enable(1);
  wifi_set_channel(1);
  wifi_promiscuous_enable(0);

  if (esp_now_init() != 0) {
    Serial.println(F("Erro ao inicializar ESP-NOW"));
    delay(2000);

    ESP.restart();
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  Serial.println(F("Dispositivo pronto para receber dados"));
}

/**
 */
void loop() {
  processaMensagemUART();
  yield();
}

/**
 */
void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {

  // Cria um buffer local e copia os dados para ele
  char payload[len + 1];
  memcpy(payload, incomingData, len);

  // Garante que tenha um fim de string
  payload[len] = '\0';

  // Envia o texto pela software serial.
  bifrostSerial.println(payload);
}

/**
 */
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print(F("Status do envio: "));
  
  if (sendStatus == 0) {
    Serial.println(F("Sucesso"));
  } else {
    Serial.print(F("Falha (código "));
    Serial.print(sendStatus);
    Serial.println(F(")"));
  }
}

/**
 */
void processaMensagemUART() {

  /// Só processa quando existem dados na serial
  if (bifrostSerial.available() > 0) {

    /// Lẽ o JSON e remove espaços vazios.
    String rawJson = Serial.readStringUntil('\n');
    rawJson.trim();
    
    /// Evita erros e processamento desnecessário.
    if (rawJson.length() == 0) return;
    
    /// Sinalizar que chegaram dados. 
    //Serial.println();
    //Serial.println(F("Raw JSON:"));
    //Serial.println(rawJson);
    //
    
    /// Desserializamos o JSON para saber o destinatário da mensagem.
    /// Atualmente essa é a única utilidade do JSON, já que não é necessário validar. 
    /// A validação ocorre pelo Dispatcher, no Raspberry.
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, rawJson);
    
    /// Em caso de erro ao converter, simplesmente retorna.
    if (error) {
      Serial.print(F("Erro ao parsear JSON: "));
      Serial.println(error.c_str());
      return;
    }
    
    /// Caso não possuir uma versão válida de envelope, retorna
    if (!doc.containsKey("v")) {
      Serial.println(F("Mensagem recebida não contém requisitos minimos."));
      return;
    }

    /// !-------------------------------
    /// Por enquanto NÃO MUDAMOS A FORMA
    /// que ele se comporta de acordo com
    /// a versão.
    /// !-------------------------------

    const char* macStr = doc["dst"];
    uint8_t mac[6];

    // Converte a string MAC para bytes
    if (sscanf(macStr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
      Serial.println(F("Erro: Formato de MAC inválido. Use AA:BB:CC:DD:EE:FF"));
      return;
    }

    struct_message message;
    strlcpy(message.data, rawJson.c_str(), sizeof(message.data));

    esp_now_del_peer(mac);
    if (esp_now_add_peer(mac, ESP_NOW_ROLE_COMBO, 1, NULL, 0) != 0) {
      Serial.println(F("Erro ao adicionar peer ESP-NOW"));
      return;
    }

    int result = esp_now_send(mac, (uint8_t *)&message, sizeof(message));

    if (result == 0) {
      Serial.println(F("Mensagem enviada com sucesso via ESP-NOW"));
    } else {
      Serial.print(F("Falha no envio. Código: "));
      Serial.println(result);

      // Tentativa de reconexão
      esp_now_del_peer(mac);
      if (esp_now_add_peer(mac, ESP_NOW_ROLE_COMBO, 1, NULL, 0) != 0) {
        Serial.println(F("Erro ao adicionar peer na segunda tentativa"));
        return;
      }
        
      delay(50);

      result = esp_now_send(mac, (uint8_t *)&message, sizeof(message));
      if (result == 0) {
        Serial.println(F("Mensagem enviada na segunda tentativa"));
      } else {
        Serial.print(F("Falha na segunda tentativa. Código: "));
        Serial.println(result);
      }
    }
  }
}