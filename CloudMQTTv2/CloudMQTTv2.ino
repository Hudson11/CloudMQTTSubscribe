
////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Arquivo:   MQTT_WiFiManager_Rele.ino
//Tipo:      Exemplo de uso das bibliotecas WiFiManager e PubSubClient para ESP8266 na IDE do Arduino
//Autor:     Marco Rabelo para o canal Infortronica Para Zumbis (www.youtube.com/c/InfortronicaParaZumbis)
//Descricao: Curso de ESP8266 - Utilizando MQTT e gerenciador de wifi para controlar um relé.
//Video:     https://youtu.be/oX4ttJEULmA
////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
Equivalencia das saidas Digitais entre NodeMCU e ESP8266 (na IDE do Arduino)
NodeMCU - ESP8266
D0 = 16;
D1 = 5;
D2 = 4;
D3 = 0;
D4 = 2;
D5 = 14;
D6 = 12;
D7 = 13;
D8 = 15;
D9 = 3;
D10 = 1;
*/

#include <FS.h>                 //Esta precisa ser a primeira referência, ou nada dará certo e sua vida será arruinada. kkk
#include <ESP8266WiFi.h>        //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>      //uso da lib http
#include <WiFiManager.h>        //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>        //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>
#include <EEPROM.h>
#include <time.h>

#define DEBUG                   //Se descomentar esta linha vai habilitar a 'impressão' na porta serial

#define servidor_mqtt             "m16.cloudmqtt.com"        // servidor CloudMQTT
#define servidor_mqtt_porta       "12792"        // Porta do servidor ClodMQTT
#define servidor_mqtt_usuario     "imnkyvpf"        // usuário CloudMQTT
#define servidor_mqtt_senha       "fjylwPd84hUB"        // Senha do servidor
#define mqtt_topico_sub           "mpm2ee/energyPolicy"        // Tópico assinado

//Declaração do pino que será utilizado e a memória alocada para armazenar o status deste pino na EEPROM
#define pino                      2                   //Pino que executara a acao dado no topico "esp8266/pincmd" e terá seu status informado no tópico "esp8266/pinstatus"
#define memoria_alocada           4                   //Define o quanto sera alocado na EEPROM (valores entre 4 e 4096 bytes)

WiFiClient espClient;                                 //Instância do WiFiClient
PubSubClient client(espClient);                       //Passando a instância do WiFiClient para a instância do PubSubClient

const char* IDNODE = "ESP1";

//Função para imprimir na porta serial
void imprimirSerial(bool linha, String mensagem){
  #ifdef DEBUG
    if(linha){
      Serial.println(mensagem);
    }else{
      Serial.print(mensagem);
    }
  #endif
}


//Função que reconecta ao servidor MQTT
void reconectar() {
  //Repete até conectar
  while (!client.connected()) {
    imprimirSerial(false, "Tentando conectar ao servidor MQTT...");
    
    //Tentativa de conectar. Se o MQTT precisa de autenticação, será chamada a função com autenticação, caso contrário, chama a sem autenticação. 
    bool conectado = strlen(servidor_mqtt_usuario) > 0 ?
                     client.connect("ESP8266Client", servidor_mqtt_usuario, servidor_mqtt_senha) :
                     client.connect("ESP8266Client");

    if(conectado) {
      imprimirSerial(true, "Conectado!");
      //Subscreve para monitorar os comandos recebidos
      client.subscribe(mqtt_topico_sub, 1); //QoS 1
    } else {
      imprimirSerial(false, "Falhou ao tentar conectar. Codigo: ");
      imprimirSerial(false, String(client.state()).c_str());
      imprimirSerial(true, " tentando novamente em 5 segundos");
      //Aguarda 5 segundos para tentar novamente
      delay(5000);
    }
  }
}

//Função que será chamada ao receber mensagem do servidor MQTT
void retorno(char* topico, byte* mensagem, unsigned int tamanho) {
  //Convertendo a mensagem recebida para string
  mensagem[tamanho] = '\0';
  String strMensagem = String((char*)mensagem);
  strMensagem.toLowerCase();
  //float f = s.toFloat();
  
  imprimirSerial(false, "Mensagem recebida! Topico: ");
  imprimirSerial(false, topico);
  imprimirSerial(false, ". Tamanho: ");
  imprimirSerial(false, String(tamanho).c_str());
  imprimirSerial(false, ". Mensagem: ");
  imprimirSerial(true, strMensagem);
}

//Função salvar dados dos sensores
void salveSensorData(){
  Serial.println("******** Begin salveSensorData *********");
  HTTPClient http;
  http.begin("http://localhost:8080/mpm2ee/esp");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "aplication/json");

  //Informa os dados no node
  StaticJsonBuffer<300> jsonBuffer;
  JsonObject& node = jsonBuffer.createObject();
  node["id"] = IDNODE;
  node["regiao"] = "Recanto do Guerreiro";
  node["datahora"] = "2018-05-08";

  //Informa cada um dos sensores conectados ao node
  JsonObject& energia = jsonBuffer.createObject();
  energia["tipo"] = "energia";
  energia["dado"] = "48";

  JsonObject& agua = jsonBuffer.createObject();
  agua["tipo"] = "agua";
  agua["dado"] = "48";

  JsonObject& ph = jsonBuffer.createObject();
  ph["tipo"] = "PH";
  ph["dado"] = "98";

  //Insere os dados dos sensores ao node
  JsonArray& sensores = node.createNestedArray("sensores");
  sensores.add(ph);
  sensores.add(agua);
  sensores.add(energia);

  String sms;
  node.printTo(sms);
  Serial.println(sms);

  int httpCode = http.POST(sms);
  http.end();
  Serial.println("********* End salveSensorData *********");
}

//Função inicial (será executado SOMENTE quando ligar o ESP)
void setup() {

  Serial.begin(115200);

  //Parâmetros extras para configuração
  //Depois de conectar, parameter.getValue() vai pegar o valor configurado.
  //Os campos do WiFiManagerParameter são: id do parâmetro, nome, valor padrão, comprimento
  WiFiManagerParameter custom_mqtt_server("server", "Servidor MQTT", servidor_mqtt, 40);
  WiFiManagerParameter custom_mqtt_port("port", "Porta", servidor_mqtt_porta, 6);
  WiFiManagerParameter custom_mqtt_user("user", "Usuario", servidor_mqtt_usuario, 20);
  WiFiManagerParameter custom_mqtt_pass("pass", "Senha", servidor_mqtt_senha, 20);
  WiFiManagerParameter custom_mqtt_topic_sub("topic_sub", "Topico para subscrever", mqtt_topico_sub, 30);

  //Inicialização do WiFiManager. Uma vez iniciado não é necessário mantê-lo em memória.
  WiFiManager wifiManager;
  
  //Adicionando os parâmetros para conectar ao servidor MQTT
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);
  wifiManager.addParameter(&custom_mqtt_topic_sub);

  //Busca o ID e senha da rede wifi e tenta conectar.
  //Caso não consiga conectar ou não exista ID e senha,
  //cria um access point com o nome "AutoConnectAP" e a senha "senha123"
  //E entra em loop aguardando a configuração de uma rede WiFi válida.
  if (!wifiManager.autoConnect("AutoConnectAP", "senha123")) {
    imprimirSerial(true, "Falha ao conectar. Excedeu o tempo limite para conexao.");
    delay(3000);
    //Reinicia o ESP e tenta novamente ou entra em sono profundo (DeepSleep)
    ESP.reset();
    delay(5000);
  }

  //Se chegou até aqui é porque conectou na WiFi!
  imprimirSerial(true, "Conectado!! :)");
  imprimirSerial(false, "IP: ");
  imprimirSerial(true, WiFi.localIP().toString());

  //Informando ao client do PubSub a url do servidor e a porta.
  int portaInt = atoi(servidor_mqtt_porta);
  client.setServer(servidor_mqtt, portaInt);
  client.setCallback(retorno);
  
}

//Função de repetição (será executado INFINITAMENTE até o ESP ser desligado)
void loop() {
  if (!client.connected()) {
    reconectar();
  }
  salveSensorData();
  delay(10000);
  client.loop();
}

