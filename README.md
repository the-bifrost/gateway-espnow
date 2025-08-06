# Gateway & Sensores EspNow

Esse repositório contém os códigos para a implementação da Unidade Espnow da Dispatcher (Gateway), bem como a implementação de sensores e atuadores no padrão bifrost. 

Todos os códigos descritos aqui usam o envelopamento padrão de mensagens para a leitura, validação e roteamento dos dados. 

## Tutoriais

Sem tutoriais até o momento .-.

## Funcionalidades

- 📦 Despacha mensagens EspNow vindas do Dispatcher.

- 🧾 Retorna logs de Deug via Serial USB

- 🔁 Comunicação bi-direcional: _Sensores <--> Bifrost_

## Para fazer

### Unidade Espnow

- [X] Conexão SoftwareSerial com Dispatcher e Serial apenas para Debug
- [X] Roteamento bidirecional de mensagens
- [ ] Separar o tratamento de dados de acordo com a versão do envelope
- [ ] Reescrever comentários de Debug
- [ ] Implementar o JSON com [a lib da Bifrost](https://github.com/the-bifrost/gateway-lib)
- [ ] Validar o cadastramento de peers ESP-Now
- [ ] Melhorias na documentação _incode_.
- [ ] Criar um código utilizável tanto em ESP8266's quanto ESP32's.
- [ ] Validar a quantidade de perda de pacotes entre as mensagens recebidas/enviadas.

### Sensores

- [ ] Padronizar uma estrutura de código dos sensores
- [ ] Separar Sensores e atuadores
- [ ] Implementar o JSON com [a lib da Bifrost](https://github.com/the-bifrost/gateway-lib)
