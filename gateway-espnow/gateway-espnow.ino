/*
  ___ _  __            _   
 | _ |_)/ _|_ _ ___ __| |_ 
 | _ \ |  _| '_/ _ (_-<  _|
 |___/_|_| |_| \___/__/\__|

 Bifrost - Gateway EspNow  

 Esse código implementa uma Unidade EspNow da Bifrost. Usa o envelopamento padrão de mensagens 
 para fazer a leitura, validação e roteamento dos dados do protocolo espnow. 

*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>

#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <ArduinoLog.h>

// Separamos as conexões seriais para que não gere um looping
// Usar a mesma faz com que o Dispatcher leia tudo o que a Unidade printa.
const uint8_t RX_PIN = D5;
const uint8_t TX_PIN = D6;
SoftwareSerial bifrostSerial(RX_PIN, TX_PIN);

const int SERIAL_BAUDRATE = 115200;
const int BIFROST_BAUDRATE = 9600;

void initSerial() {

  // Inicia serial
  Serial.begin(SERIAL_BAUDRATE);

  // Inicia SoftwareSerial
  bifrostSerial.begin(BIFROST_BAUDRATE);

  // Os níveis disponíveis são:
  // LOG_LEVEL_SILENT, LOG_LEVEL_FATAL, LOG_LEVEL_ERROR, LOG_LEVEL_WARNING, LOG_LEVEL_INFO, LOG_LEVEL_TRACE, LOG_LEVEL_VERBOSE
  Log.begin(LOG_LEVEL_VERBOSE, &Serial);
}

void initWiFi() {
  // Ajusta o modo do Wifi e garante que está desconectado
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Define o canal do Wifi
  wifi_promiscuous_enable(1);
  wifi_set_channel(1);
  wifi_promiscuous_enable(0);
}

void initEspNow() {
  if (esp_now_init() != 0) {
    Log.fatalln(F("Erro ao inicializar ESP-NOW!"));
    delay(2000);
    ESP.restart();
  }

  // Envia e recebe dados, registra as funções de callback
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
}

/**
 * @brief Configurando Seriais, Wifi e Espnow
 */
void setup() {
  initSerial();
  initWiFi();
  initEspNow();

  const String MAC = WiFi.macAddress();
  Log.noticeln(F(CR "Unidade EspNow Iniciada!."));
  Log.noticeln(F("MAC: %s") , MAC.c_str());
}

/**
 * @brief Loop principal do código
 *
 * Faz a leitura contínua da Serial da Dispatcher. Sempre que receber dados,
 * envia a mensagem para o destinatário correspondente.
 */
void loop() {
  processaMensagemUART();
  yield();
}

/**
 * @brief Callback que encaminha mensagens para o dispatcher
 *
 * É uma função callback que faz a leitura espnow e reencaminha para a dispatcher, via Serial. 
 * 
 * @param  mac           Endereço MAC do remetente EspNow
 * @param  incomingData  Mensagem enviada pelo remetente
 * @param  len           Tamanho da mensagem recebida
 */
void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {

  // Cria um buffer local e copia os dados para ele
  char payload[len + 1];
  memcpy(payload, incomingData, len);

  // Finaliza como string
  payload[len] = '\0'; 

  // Envia o texto pela software serial.
  bifrostSerial.println(payload);

  Log.verboseln(F("\n--- Pacote recebido ---"));
  Log.verboseln(F("MAC origem: %s"), convertMacIntoString(mac));
  Log.verboseln(F("Tamanho: %i"), len);
  Log.verboseln(F("Conteúdo: %s"), payload);
  Log.verboseln(F("------------------------------------"));
}

/**
 * @brief Callback para receber status do envio espnow
 * 
 * @param mac_addr endereço mac do dispositivo que tentou enviar dados          
 * @param sendStatus código com o status do envio
 */
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print(F("Status do envio: "));
  
  if (sendStatus == 0) {
    Log.verboseln(F("Sucesso"));
  } else {
    Log.verboseln(F("Falha (código %i)"), sendStatus);
  }
}

/**
 * @brief Faz a leitura continua da serial do dispatcher, roteia mensagens espnow
 *
 * Não faz a validação das mensagens (esse é o papel do dispatcher), apenas 
 * confere se possúi versão e destinatário.
 */
bool processaMensagemUART() {
  if (bifrostSerial.available() > 0) {

    /// Lẽ o JSON e remove espaços vazios.
    String rawJson = bifrostSerial.readStringUntil('\n');
    rawJson.trim();
    
    /// Evita erros e processamento desnecessário.
    if (rawJson.length() == 0) return false;

    Log.verboseln(F("\n--- Mensagem Recebida via Dispatcher ---"));
    Log.verboseln(F("\nRaw JSON: %s"), rawJson);
    Log.verboseln(F("------------------------------------"));

    /// Desserializamos o JSON para saber o destinatário da mensagem.
    /// Atualmente essa é a única utilidade do JSON, já que não é necessário validar. 
    /// A validação ocorre pelo Dispatcher, no Raspberry.
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, rawJson);
    
    /// Em caso de erro ao converter, simplesmente retorna.
    if (error) {
      Log.verboseln(F("Erro ao parsear JSON: %s"), error.c_str());
      return false;
    }
    
    /// Caso não possuir uma versão válida de envelope, retorna
    if (!doc.containsKey("v")) {
      Log.verboseln(F("Mensagem recebida não contém requisitos minimos."));
      return false;
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
      Log.warning(F("Erro: Formato de MAC inválido. Use AA:BB:CC:DD:EE:FF"));
      return false; 
    }

    esp_now_del_peer(mac);

    if (esp_now_add_peer(mac, ESP_NOW_ROLE_COMBO, 1, NULL, 0) != 0) {
      Log.warning(F("Erro ao adicionar peer ESP-NOW"));
      return false;
    }

    int result = esp_now_send(mac, (uint8_t *)rawJson.c_str(), rawJson.length());

    if (result == 0) return true;
    
    else return false;
  } 

  else return false;
}

/**
 * @brief Faz a conversão do MAC para strings legíveis
 * 
 * @param mac_addr Um uint8_t do mac a ser convertido    
 *
 * @return Uma string equivalente ao mac convertido.
 */
String convertMacIntoString(uint8_t *mac) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  return String(macStr);
}