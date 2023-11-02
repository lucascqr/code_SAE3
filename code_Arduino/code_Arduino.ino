//Inclusion des bibliothèques
#include <BH1750.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <math.h>
#include "SPI.h"
#include "MFRC522.h"
#include <SPI.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <Arduino_JSON.h>


//Déclarations de fonctions
void capteurs();
void ecran() ;
void displayData();
void RFID() ;
void Button() ;


BH1750 lightMeter;

//Déclarations de pins
#define MQ9_AOUT 36
#define BOUTON 25
#define RST_PIN  33 // RES pin
#define SS_PIN  27 // SDA (SS) pin
#define R_PHOTO 39
#define LED 2

//Déclarations de variables globales
bool buttonState = true;
String strBadgeID = "";
byte nuidPICC[4];
unsigned long now = 0 ;
int gasValue ;
float pression ;
float temperature ;
float lux ;
float photo_res ;
int frequence_Led = 500 ;


TFT_eSPI tft = TFT_eSPI(); // Invoke custom library
Adafruit_BMP280 bmp; // I2C
MFRC522 rfid(SS_PIN, RST_PIN);

// Réseau SSID & mot de passe (à remplacer par vos propres informations)
const char* ssid = "Lucas";
const char* password = "Mkjnc37v";

// Créer un objet serveur sur le port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

//--------------------------------------------------------------------------------------------------------------------------------
//Fonctions relatives au web server

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void initWebSocket() {
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
}

//--------------------------------------------------------------------------------------------------------------------------------

void setup() {
  //Initialisation du port série à 9600 baud
  Serial.begin(9600);
  Wire.begin();
  SPI.begin();
  rfid.PCD_Init();
  lightMeter.begin();

  // Initialiser SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Connexion Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println(WiFi.localIP());

  initWebSocket();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/script.js", "application/javascript");
  });

  server.onNotFound(notFound);

  server.begin();

  //Setup pour le BMP280
  /* Default settings from datasheet. */
  unsigned status;
  status = bmp.begin(0x76);
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */

  //Setup pour le TFT
  tft.init();
  tft.setRotation(7);
  uint16_t calData[5] = { 213, 3660, 296, 3540, 7 };
  tft.setTouch(calData);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  pinMode(BOUTON, OUTPUT) ;
  pinMode(LED, OUTPUT) ;
  pinMode(MQ9_AOUT, INPUT) ;
  pinMode(R_PHOTO, INPUT) ;
  Button();
  tft.fillRect(0, 220, tft.width(), 20, TFT_BLACK);  // Effacer la zone température
  tft.setCursor(0, 220);
  tft.print(WiFi.localIP());

}

//--------------------------------------------------------------------------------------------------------------------------------

void loop() {
  static unsigned long last = 0 , last2 = 0, last3 = 0, lastData = 0;
  static bool etatLed = 0 ;
  now = millis() ;

  if (now - last2 > 100) {
    RFID() ;
    bouton_tft() ;
  }
  if (now - last > 1000) {
    capteurs() ;
    displayData() ;
    Serial.println();
    Serial.println((String) "Frequence LED : " + frequence_Led);
    Serial.println();
    last = now;
  }

  if (now - lastData > 1000) { // Vérifier s'il y a des clients connectés
    JSONVar data;
    data["temperature"] = temperature;
    data["pression"] = pression;
    data["gasValue"] = gasValue;
    data["lux"] = lux;
    data["photo_res"] = photo_res;
    data["buttonState"] = buttonState;
    String jsonResponse = JSON.stringify(data);
    ws.textAll(jsonResponse);
    lastData = now ;
  }

  if (now - last3 > frequence_Led) {
    etatLed = 1 - etatLed ;
    digitalWrite(LED, etatLed) ;
    last3 = now ;
  }
}

//--------------------------------------------------------------------------------------------------------------------------------

void capteurs() {
  //BH1750
  lux = lightMeter.readLightLevel();
  Serial.println((String)"Light: " + lux + " lx");

  //BMP280
  temperature = bmp.readTemperature();
  Serial.println((String)"Temperature = " + temperature + " *C");
  pression = bmp.readPressure() / 100 ;
  Serial.println((String) "Pression = " + pression / 100 + " Pa");

  //MQ9
  float vOut = analogRead(MQ9_AOUT) * (3.3 / 4095.0); // Convertir la lecture ADC en tension
  gasValue = 595 * pow((3.3 / vOut - 1) * (996 / 850), -2.24) ; //996 = RO ; 850 = Rs ; 3.3 = V_MAX
  Serial.println((String)"Valeur lue: " + vOut + " V, Concentration: " + gasValue + " ppm");

  //Photo résistances
  float value = analogRead(R_PHOTO) * 3.3 / 4095.0 ;
  photo_res = 72 * pow(value, 1.429) ;
}

//--------------------------------------------------------------------------------------------------------------------------------

void RFID() {
  // Initialisé la boucle si aucun badge n'est présent
  if ( !rfid.PICC_IsNewCardPresent())
    return;

  // Vérifier la présence d'un nouveau badge
  if ( !rfid.PICC_ReadCardSerial())
    return;

  // Enregistrer l'ID du badge (4 octets)
  for (byte i = 0; i < 4; i++)
  {
    nuidPICC[i] = rfid.uid.uidByte[i];
  }

  // Mettre la valeur en string
  for (int i = 0; i < 4; i++) {
    if (nuidPICC[i] < 0x10) {
      strBadgeID += '0';
    }
    strBadgeID += String(nuidPICC[i], HEX);
  }

  //Serial.println(strBadgeID); affiche l'id du badge

  //Traitement du badge pour vérifier l'id
  if (strBadgeID == "5a7b28b0" or strBadgeID == "537cc8a9" or strBadgeID == "f406adee") {
    Button() ;
    Serial.println("Badge connu!");
  } else {
    Serial.println("Badge inconnu.");
  }
  Serial.println() ;

  //Réinitialisation strBadgeId pour le prochian scan
  strBadgeID = "";

  // Re-Init RFID
  rfid.PICC_HaltA(); // Halt PICC
  rfid.PCD_StopCrypto1(); // Stop encryption on PCD
}

//--------------------------------------------------------------------------------------------------------------------------------

void bouton_tft() {
  // Vérifier le toucher
  uint16_t x, y;
  if (tft.getTouch(&x, &y)) {
    if (x > (tft.width() / 2) - 50 && x < (tft.width() / 2) + 50 && y > (tft.height() / 2) - 20 && y < (tft.height() / 2) + 20) {
      Button() ;
    }
  }
}

//--------------------------------------------------------------------------------------------------------------------------------

void displayData() {
  //---------------------------------Température---------------------------------
  tft.fillRect(0, 0, tft.width(), 20, TFT_BLACK);  // Effacer la zone température
  tft.setCursor(0, 0);
  tft.print((String)"Temperature: " + temperature + " C");
  //-----------------------------------Lumière-----------------------------------
  tft.fillRect(0, 20, tft.width(), 20, TFT_BLACK);  // Effacer la zone luminosité
  tft.setCursor(0, 20);
  tft.print((String)"Lumiere: " + lux + " lux" );
  //---------------------------Concentration en gaz (MQ9)------------------------
  tft.fillRect(0, 40, tft.width(), 20, TFT_BLACK);  // Effacer la zone gaz
  tft.setCursor(0, 40);
  tft.print((String)"Gaz: " + gasValue + " ppm");
  //-----------------------------------Pression----------------------------------
  tft.fillRect(0, 60, tft.width(), 20, TFT_BLACK);  // Effacer la zone pression
  tft.setCursor(0, 60);
  tft.print((String)"Pression: " + pression + " hPa");
  //-------------------------------Photo résistance------------------------------
  tft.fillRect(0, 80, tft.width(), 20, TFT_BLACK);  // Effacer la zone pression
  tft.setCursor(0, 80);
  tft.print((String)"Resistance: " + photo_res + " lux");
}

//--------------------------------------------------------------------------------------------------------------------------------

void Button() {
  buttonState = !buttonState;
  digitalWrite(BOUTON, buttonState ? HIGH : LOW);
  String text = buttonState ? "Active" : "Desactive";
  int textWidth = tft.textWidth(text);
  int textHeight = 16;  // Estimation de la hauteur de texte pour la taille de police 2
  int padding = 10;  // Espace autour du texte
  int rectWidth = textWidth + 2 * padding;
  int rectHeight = textHeight + 2 * padding;
  int rectX = (tft.width() / 2) - (rectWidth / 2);
  int rectY = (tft.height() / 2) - (rectHeight / 2);

  // Efface la zone précédente
  tft.fillRect(0, rectY, tft.width(), rectHeight, TFT_BLACK);
  tft.fillRect(rectX, rectY, rectWidth, rectHeight, buttonState ? TFT_GREEN : TFT_RED);
  tft.setCursor(rectX + padding, rectY + padding);
  tft.print(text);
}

//--------------------------------------------------------------------------------------------------------------------------------

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
               void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.println("WebSocket client connected");
      break;
    case WS_EVT_DISCONNECT:
      Serial.println("WebSocket client disconnected");
      break;
    case WS_EVT_DATA:
      // Inclure le pointeur client lors de l'appel à handleWebSocketMessage
      handleWebSocketMessage(arg, data, len, client);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len, AsyncWebSocketClient *client) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    JSONVar message = JSON.parse((char*)data);
    if (JSON.typeof(message) == "undefined") {
      Serial.println("Invalid JSON received");
      return;
    }

    // Gérer la demande de données
    if (message.hasOwnProperty("action")) {
      String action = (const char*)message["action"];

      if (action == "requestData") {
        // Préparez les données des capteurs à envoyer
        JSONVar data;
        data["temperature"] = temperature;
        data["pression"] = pression;
        data["gasValue"] = gasValue;
        data["lux"] = lux;
        data["photo_res"] = photo_res;
        data["buttonState"] = buttonState;
        String jsonResponse = JSON.stringify(data);
        client->text(jsonResponse);
      } else if (action == "toggleButton") {
        // Basculez l'état du bouton
        Button() ;
        // Envoyer la confirmation de l'état du bouton
        JSONVar response;
        response["buttonState"] = buttonState;
        String responseStr = JSON.stringify(response);
        client->text(responseStr);
      } else if (action == "setFrequency") {
        if (message.hasOwnProperty("value")) {
          frequence_Led = int(message["value"]);
          // Envoyer la confirmation de la nouvelle fréquence
          JSONVar response;
          response["frequence_Led"] = frequence_Led;
          String responseStr = JSON.stringify(response);
          client->text(responseStr);
        }
      }
    }
  }
}
