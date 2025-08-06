#include <ESP8266WiFi.h>
#include <espnow.h>
#include <ArduinoJson.h>

// Estrutura para dados ESP-NOW
typedef struct struct_message {
  char data[240];
} struct_message;

// Declaração antecipada das funções
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus);
void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len);
void printJsonObject(JsonObject obj, int indent = 0);
void processaMensagemUART();

// Função para imprimir JsonObject
void printJsonObject(JsonObject obj, int indent) {
  String indentStr;
  for (int i = 0; i < indent; i++) {
    indentStr += "  ";
  }
  
  for (JsonPair kv : obj) {
    Serial.print(indentStr);
    Serial.print(kv.key().c_str());
    Serial.print(": ");
    
    if (kv.value().is<JsonObject>()) {
      Serial.println();
      printJsonObject(kv.value().as<JsonObject>(), indent + 1);
    } 
    else if (kv.value().is<JsonArray>()) {
      Serial.println();
      JsonArray arr = kv.value().as<JsonArray>();
      for (JsonVariant v : arr) {
        Serial.print(indentStr);
        Serial.print("- ");
        if (v.is<JsonObject>()) {
          Serial.println();
          printJsonObject(v.as<JsonObject>(), indent + 1);
        } else {
          if (v.is<const char*>()) {
            Serial.println(v.as<const char*>());
          } else if (v.is<int>()) {
            Serial.println(v.as<int>());
          } else if (v.is<float>()) {
            Serial.println(v.as<float>(), 2);
          } else if (v.is<bool>()) {
            Serial.println(v.as<bool>() ? "true" : "false");
          }
        }
      }
    }
    else {
      if (kv.value().is<const char*>()) {
        Serial.println(kv.value().as<const char*>());
      }
      else if (kv.value().is<int>()) {
        Serial.println(kv.value().as<int>());
      }
      else if (kv.value().is<float>()) {
        Serial.println(kv.value().as<float>(), 2);
      }
      else if (kv.value().is<bool>()) {
        Serial.println(kv.value().as<bool>() ? "true" : "false");
      }
    }
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("\nIniciando dispositivo...");
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  // Configuração mais robusta do canal WiFi
  wifi_promiscuous_enable(1);
  wifi_set_channel(1);
  wifi_promiscuous_enable(0);

  if (esp_now_init() != 0) {
    Serial.println("Erro ao inicializar ESP-NOW");
    delay(1000);
    ESP.restart();
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  
  Serial.println("Dispositivo pronto para receber dados");
}

void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  char payload[len + 1];
  memcpy(payload, incomingData, len);
  payload[len] = '\0';
  Serial.println(payload);
}

void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("Status do envio: ");
  if (sendStatus == 0) {
    Serial.println("Sucesso");
  } else {
    Serial.print("Falha (código ");
    Serial.print(sendStatus);
    Serial.println(")");
  }
}

void processaMensagemUART() {
  if (Serial.available() > 0) {
    String rawJson = Serial.readStringUntil('\n');
    rawJson.trim();  // Remove espaços em branco extras
    
    if (rawJson.length() == 0) return;
    
    Serial.println("\n--- Mensagem Recebida via UART ---");
    Serial.println("Raw JSON:");
    Serial.println(rawJson);
    
    // Usando ArduinoJson 7
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, rawJson);
    
    if (error) {
      Serial.print("Erro ao parsear JSON: ");
      Serial.println(error.c_str());
      return;
    }
    
    Serial.println("\nCampos extraídos:");
    if (doc.containsKey("v")) {
      Serial.print("Versão: "); Serial.println(doc["v"].as<int>());
    }
    if (doc.containsKey("src")) {
      Serial.print("Origem: "); Serial.println(doc["src"].as<const char*>());
    }
    if (doc.containsKey("dst")) {
      Serial.print("Destino: "); Serial.println(doc["dst"].as<const char*>());
    }
    if (doc.containsKey("type")) {
      Serial.print("Tipo: "); Serial.println(doc["type"].as<const char*>());
    }
    if (doc.containsKey("ts")) {
      Serial.print("Timestamp: "); Serial.println(doc["ts"].as<long>());
    }
    
    if (doc.containsKey("payload")) {
      JsonObject payload = doc["payload"].as<JsonObject>();
      Serial.println("\nConteúdo do Payload:");
      printJsonObject(payload);
    }

    if (doc.containsKey("type") && (strcmp(doc["type"], "command") == 0 || strcmp(doc["type"], "register_response") == 0)) {
        // Verifica se existe o campo "dst" no JSON
        if (!doc.containsKey("dst")) {
            Serial.println("Erro: Campo 'dst' não encontrado no JSON");
            return;
        }

        const char* macStr = doc["dst"];
        uint8_t mac[6];

        // Converte a string MAC para bytes
        if (sscanf(macStr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
                  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
            Serial.println("Erro: Formato de MAC inválido. Use AA:BB:CC:DD:EE:FF");
            return;
        }

      struct_message message;
      strlcpy(message.data, rawJson.c_str(), sizeof(message.data));

      esp_now_del_peer(mac);
      if (esp_now_add_peer(mac, ESP_NOW_ROLE_COMBO, 1, NULL, 0) != 0) {
        Serial.println("Erro ao adicionar peer ESP-NOW");
        return;
      }

      int result = esp_now_send(mac, (uint8_t *)&message, sizeof(message));
      if (result == 0) {
        Serial.println("Mensagem enviada com sucesso via ESP-NOW");
      } else {
        Serial.print("Falha no envio. Código: ");
        Serial.println(result);

        // Tentativa de reconexão
        esp_now_del_peer(mac);
        if (esp_now_add_peer(mac, ESP_NOW_ROLE_COMBO, 1, NULL, 0) != 0) {
          Serial.println("Erro ao adicionar peer na segunda tentativa");
          return;
        }
        
        delay(50);
        result = esp_now_send(mac, (uint8_t *)&message, sizeof(message));
        if (result == 0) {
          Serial.println("Mensagem enviada na segunda tentativa");
        } else {
          Serial.print("Falha na segunda tentativa. Código: ");
          Serial.println(result);
        }
      }
      Serial.print("Tamanho da mensagem: ");
      Serial.println(sizeof(message));
    }
    
    Serial.println("-----------------------");
  }
}

void loop() {
  processaMensagemUART();
  delay(100);
}