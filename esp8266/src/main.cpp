#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "secrets.h"
#include <WebSocketsClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char *websockets_server_host = "192.168.86.38"; // Enter server adress
const uint16_t websockets_server_port = 9002;         // Enter server port

WebSocketsClient websocketClient;

char last_payload_received[4] = {"0"};

const char *zero_fps = "0";
uint8_t zero_counter = 0;

void displayFPS(char *text)
{
  Serial.println("Display called");
  display.dim(false);
  display.clearDisplay();
  display.setTextSize(4); 
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, y1);
  display.print(text);
  display.display();
}

void turn_off_display()
{
  display.clearDisplay();
  display.dim(true);
  display.display();
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{

  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.printf("[WSc] Disconnected!\n");
    turn_off_display();
    break;
  case WStype_CONNECTED:
  {
    Serial.printf("[WSc] Connected to url: %s\n", payload);
  }
  break;
  case WStype_TEXT:
    Serial.printf("[WSc] got text: %s\n", payload);
    // Check if we recevied a 0 string back
    if (strcmp(zero_fps, (char *)payload) == 0)
    {
      // Turn off display if we keep receiving 0s
      if (zero_counter >= 10)
      {
        turn_off_display();
      }
      else
      {
        zero_counter++;
      }
    }
    else
    {
      // Check is payload has changed
      if (strcmp(last_payload_received, (char *)payload) != 0)
      {
        // reset the counter
        zero_counter = 0;

        //I know length will be 4 bytes max
        strcpy(last_payload_received, (char *)payload);
        displayFPS((char *)payload);
      }
    }

    break;
  case WStype_BIN:
    Serial.printf("[WSc] get binary length: %u\n", length);
    hexdump(payload, length);

    // send data to server
    // webSocket.sendBIN(payload, length);
    break;
  case WStype_PING:
    // pong will be send automatically
    Serial.printf("[WSc] get ping\n");
    break;
  case WStype_PONG:
    // answer to a ping we send
    Serial.printf("[WSc] get pong\n");
    break;
  }
}

void setup()
{
  Serial.begin(9600);

  WiFi.begin(ssid, password);
  // Wait some time to connect to wifi
  for (int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++)
  {
    Serial.print(".");
    delay(1000);
  }

  // Check if connected to wifi
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("No Wifi!");
    return;
  }

  // server address, port and URL
  websocketClient.begin(websockets_server_host, websockets_server_port, "/");

  // event handler
  websocketClient.onEvent(webSocketEvent);

  // try ever 5000 again if connection has failed
  websocketClient.setReconnectInterval(5000);

  // start heartbeat (optional)
  // ping server every 15000 ms
  // expect pong from server within 3000 ms
  // consider connection disconnected if pong is not received 2 times
  websocketClient.enableHeartbeat(15000, 3000, 2);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }
}

void loop()
{
  websocketClient.loop();
}