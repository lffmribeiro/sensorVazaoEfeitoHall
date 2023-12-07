/********************************************************************
-----------Módulo IOT de medição remota de consumo de água-----------
--------------------------Criadores:--------------------------------- 
----------------João Victor Bandeira dos Anjos-----------------------
------------------João Victor Ferreira Gomes-------------------------
------------Luiz Fernando Ferreira Messias Ribeiro-------------------
------------------Macgyver Cseh dos Santos---------------------------
------------------Wellington Gomes da Silva--------------------------
********************************************************************/

//-----------------------------------/SD/*
#include "FS.h"
#include <SD.h>
#include <SPI.h>

#define SD_MISO     19
#define SD_MOSI     23
#define SD_SCLK     18
#define SD_CS       5
SPIClass sdSPI(VSPI);

String dataMessage;
String dataMessage2;
//-----------------------------------//*/

//-----------------------------------/RTC/*
#include "RTClib.h"
#include <Wire.h>

RTC_DS3231 rtc;

char daysOfTheWeek[7][12] = {"domingo", "segunda", "terca", "quarta", "quinta", "sexta", "sabado"};
//-----------------------------------//*/

//-----------------------------------/Sensor Vazao/*

const byte interruptPin = 15;

uint32_t pulsoAcumulado;
float tempoSegundos = 60*20;
float vazaoLitroAcumulada;

void IRAM_ATTR isr() { // funçao chamada cada vez que o sensor de fluxo mandar um pulso
    pulsoAcumulado++;
}
//-----------------------------------//*/

//-----------------------------------//DHT
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define DHTPIN  4
#define DHTTYPE DHT22
DHT_Unified dht(DHTPIN, DHTTYPE);

sensor_t sensor;
sensors_event_t event;
//-----------------------------------//*/

//-----------------------------------/MQTT/*
#include <WiFi.h>
#include <PubSubClient.h>

// Update these with values suitable for your network.

const char* ssid                        = "JL_PNDS_2.4_plus+";
const char* password                    = "TccEG2023";
const char* mqtt_server                 = "mqtt.eclipseprojects.io";
const char* mqtt_clientId               = "esp32Medidor";
const char* topicoClienteID             = "tccSensorVazao_UAM_EG_C/clienteID";
const char* topicoTemperatura           = "tccSensorVazao_UAM_EG_C/temperatura";
const char* topicoUmidade               = "tccSensorVazao_UAM_EG_C/umidade";
const char* topicoDataHora              = "tccSensorVazao_UAM_EG_C/dataHora";
const char* topicoDiaSemana             = "tccSensorVazao_UAM_EG_C/diaSemana";
const char* topicoVazaoLitro            = "tccSensorVazao_UAM_EG_C/vazaoLitro";
const char* topicoVazaoLitroAcumulada   = "tccSensorVazao_UAM_EG_C/vazaoLitroAcumulada";
const char* topicoTodosOsItens          = "tccSensorVazao_UAM_EG_C/todosOsItens";


WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
}


void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_clientId)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
//-----------------------------------//*/
void setup() {

  Serial.begin(115200);

//-----------------------------------/RTC/*
  Wire.begin(21,22);

#ifndef ESP8266
  while (!Serial); // wait for serial port to connect. Needed for native USB
#endif

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    //rtc.adjust(DateTime(2023, 11, 23, 19, 23, 0));
  }

  // When time needs to be re-set on a previously configured device, the
  // following line sets the RTC to the date & time this sketch was compiled
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  // This line sets the RTC with an explicit date & time, for example to set
  // January 21, 2014 at 3am you would call:
  //rtc.adjust(DateTime(2023, 12, 3, 20, 29, 0));
//-----------------------------------//*/

//-----------------------------------/Sensor Vazao/*
  attachInterrupt(digitalPinToInterrupt(interruptPin), isr, RISING);
//-----------------------------------//*/

//-----------------------------------/SD/*
  SD.begin(SD_CS);  
  sdSPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  if(!SD.begin(SD_CS, sdSPI)) {
    Serial.println("Card Mount Failed");
    return;
  }
  Serial.println("1");
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  Serial.println("Initializing SD card...");
    if (!SD.begin(SD_CS)) {
    Serial.println("ERROR - SD card initialization failed!");
    return;    // init failed
  }
  Serial.println("2");
  // If the data.csv file doesn't exist
  // Create a file on the SD card and write the data labels
  File file = SD.open("/DadosColetados.csv");
  File file2 = SD.open("/DadosLeitura.csv");
  
  if(!file) {
    Serial.println("File doens't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/DadosColetados.csv", "data;hora;diaSemana;temperatura;umidadeRelativa;vazaoLitroAcumulada\r\n");
  }
  else {
    Serial.println("File already exists");  
  }

  if(!file2) {
    Serial.println("File doens't exist");
    Serial.println("Creating file...");
    writeFile(SD, "/DadosLeitura.csv", "0");
  }
  else {
    Serial.println("File already exists");  
  }

  pulsoAcumulado = uint32_t(file2.readString().toDouble());
  Serial.println(pulsoAcumulado);

  //(!)Criar a cópia do código de cima e instanciar uma linha com 0
  file.close();
  file2.close();
//-----------------------------------//*/

//-----------------------------------//DHT
  dht.begin();
  dht.temperature().getSensor(&sensor);
  dht.humidity().getSensor(&sensor);
  dht.temperature().getEvent(&event);
//-----------------------------------//*/

//-----------------------------------/MQTT/*
  setup_wifi();
  client.setServer(mqtt_server, 1883);
//-----------------------------------//*/
}
void loop() {
//-----------------------------------/RTC/*
  medirTempUmid();
  printarInformacoes();
//-----------------------------------//*/

//-----------------------------------/Sensor Vazao/*
  calcularVazao();  
//-----------------------------------//*/

//-----------------------------------/SD/*
  logSDCard(); 
//-----------------------------------//*/

//-----------------------------------/MQTT/*
  publicarNoTopico();
//-----------------------------------//
}
//-----------------------------------/RTC/*
void printarInformacoes() {
    DateTime now = rtc.now();

    Serial.println("{");

    Serial.print("  \"data\" : \"");
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print("\",");
    Serial.println();

    Serial.print("  \"diaSemana\" : \"");
    Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
    Serial.print("\",");
    Serial.println();

    Serial.print("  \"hora\" : \"");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.print("\",");
    Serial.println();

    Serial.print("  \"temperatura\" : \"");
    Serial.print(rtc.getTemperature());
    Serial.print(" C");
    Serial.print("\",");
    Serial.println();

    Serial.print("  \"umidade\" : \"");
    Serial.print(String(event.relative_humidity));
    Serial.print(" %\",");
    Serial.println();

    Serial.print("  \"pulsocumulado\" : \"");
    Serial.print(String(pulsoAcumulado));
    Serial.print("\",");
    Serial.println();

    Serial.print("  \"vazaoLitroAcumulada\" : \"");
    Serial.print(vazaoLitroAcumulada);

    Serial.println("\"},");

    delay(tempoSegundos*1000);
}
//-----------------------------------//*/

//-----------------------------------/Sensor Vazao/*
void calcularVazao() {
  vazaoLitroAcumulada = (pulsoAcumulado/340);
}
//-----------------------------------//*/

//-----------------------------------/SD/*
// Write the sensor readings on the SD card
void logSDCard() {
  DateTime now = rtc.now();
  dataMessage = String(rtc.now().day())+"/"+ String(rtc.now().month())+"/"+String(rtc.now().year())+";"+
                String(rtc.now().hour())+":"+String(rtc.now().minute())+";"+
                String(daysOfTheWeek[rtc.now().dayOfTheWeek()])+";"+
                String(rtc.getTemperature())+";"+
                String(event.relative_humidity)+";"+
                String(vazaoLitroAcumulada)+";"+"\r\n";
  dataMessage2 = String(pulsoAcumulado);
  Serial.print("Save data: ");
  Serial.println(dataMessage);
  appendFile(SD, "/DadosColetados.csv", dataMessage.c_str());
  writeFile(SD, "/DadosLeitura.csv", dataMessage2.c_str());
}

// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}

// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
void appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}
//-----------------------------------//*/

//-----------------------------------//DHT
void medirTempUmid() {
  if (isnan(event.temperature)) {
    Serial.println(F("Error reading temperature!"));
  }
  else {
    Serial.print(F("Temperature: "));
    Serial.print(event.temperature);
    Serial.println(F("°C"));
  }
  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println(F("Error reading humidity!"));
  }
  else {
    Serial.print(F("Humidity: "));
    Serial.print(event.relative_humidity);
    Serial.println(F("%"));
  }
}
//-----------------------------------//*/

//-----------------------------------/MQTT/*
void publicarNoTopico() {
  reconnect();
  //String mac = String(WiFi.macAddress()).c_str();
  client.publish(topicoClienteID, String(mqtt_clientId).c_str(), true);
  //client.publish(topicoTemperatura, String(event.temperature).c_str(), true);
  client.publish(topicoTemperatura, String(rtc.getTemperature()).c_str(), true);
  client.publish(topicoUmidade, String(event.relative_humidity).c_str(), true);
  client.publish(topicoDataHora, String(String(rtc.now().day())+"/"+String(rtc.now().month())+"/"+String(rtc.now().year())+" "+String(rtc.now().hour())+":"+String(rtc.now().minute())+":"+String(rtc.now().second())).c_str(), true);
  client.publish(topicoDiaSemana, String(String(daysOfTheWeek[rtc.now().dayOfTheWeek()])).c_str(), true);
  client.publish(topicoVazaoLitroAcumulada, String(vazaoLitroAcumulada).c_str(), true);  
  
  String txClienteID      = String(mqtt_clientId).c_str();
  //String txTemperatura    = String(event.temperature).c_str();
  String txTemperatura    = String(rtc.getTemperature()).c_str();
  String txUmidade        = String(event.relative_humidity).c_str();
  String txDataHora       = String(String(rtc.now().day())+"-"+String(rtc.now().month())+"-"+String(rtc.now().year())+"T"+String(rtc.now().hour())+":"+String(rtc.now().minute())+":"+String(rtc.now().second())).c_str();
  String txDiaSemana      = String(String(daysOfTheWeek[rtc.now().dayOfTheWeek()])).c_str();
  String txVazaoAcumulada = String(vazaoLitroAcumulada).c_str();
  String txPontoVirgula   = ";";
  client.publish(topicoTodosOsItens, 
    String(
      txClienteID+
      txPontoVirgula+
      txTemperatura+
      txPontoVirgula+
      txUmidade+
      txPontoVirgula+
      txDataHora+
      txPontoVirgula+
      txDiaSemana+
      txPontoVirgula+
      txVazaoAcumulada).c_str(), true);
}
//-----------------------------------//*/