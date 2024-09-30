// https://wokwi.com/projects/346341081902744147

/*
  TODO:
  - webseite mit zählern (reset um mitternacht)
  - ntp einbauen (sommer winterzeit nicht vergessen)
  - einstellbares zeitfenster wann die klingel arbeiten soll - verworfen, led nach zeit x ausschalten
  - FS für klingelton, abspielbar auf desktop
  - videoplayer mit kamerafeed
*/
#include <Arduino.h>
#include <EEPROM.h>

#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFiUdp.h>
#include <FastLED.h>
#include "Settings.h" // save and load config data

#include "html.h"
#include "htmlProzessor.h" // The html Prozessor
#include "jslibs/bootstrap_bundle_min_js.gz.h"
#include "jslibs/bootstrap_icons_css.gz.h"
#include "jslibs/bootstrap_min_css.gz.h"
#include "jslibs/jquery_min_js.gz.h"


//------------------------ Basic Configuration----------------------------
#define sensorIn_1 D5 // Pin of the first sensor when entering the room
#define sensorIn_2 D6 // Pin of the second sensor when entering the room
#define buzzerOut D7  // Pin for the buzzer that can make *BEEP BOOP BEEP*
#define bellOut D8    // Pin for external switch or relay that will ring the big bell
#define ledPin D0     // pin for ws2812 rgb stripe

#define sensorState_1 true // idle state of sensor 1
#define sensorState_2 true // idle state of sensor 2
#define bellOutState false // idle state of pinout external switch
#define amount_led 8

//-----------------------------set internal variables-------------------------------------------
typedef enum
{
  IDLE,                            // idle, wait for signal
  IN,                              // Possibil a person going in, first sensor triggered frist
  RING,                            // first sensor triggered, second sensor triggered, make a ring
  OUT,                             // Possibil a person going out, second sensor triggered first
  COOLDOWN                         // after action, let cooldown
} stateList;                       // list of all states
stateList state = IDLE;            // set the initial state
bool sensor1;                      // sensor 1 state
bool sensor2;                      // sensor 2 state
bool buzzer;                       // buzzer switch
bool bell;                         // bell outgoing switch
byte ledChange;                    // switch for changed led data
byte stateChange;                  // switch for changed state data
int amountIn = 0;                      // counter ingoing
int amountOut = 0;                     // counter outgoing
long unsigned int lastStateMillis; // time from last statechange
long unsigned int wsTime = 0;      // animate timer
int wsPixNum = 0;                  // animate led counter
bool shouldSaveConfig = false;     // flag for saving data
bool restartNow = false;           // restart flag
char jsonBuffer[1024];             // buffer for serialize json
float vmaxIngoing = 0.0;           // max measured ingoing speed
float vmaxOutgoing = 0.0;          // max measured outgoing speed
float vmaxOutTemp = 0.0;                 // vmax calc temp value out
float vmaxInTemp = 0.0;                  // vmax calc temp value in

long unsigned int testtime;

//------------------------------- init struct and classes---------------------------------------
CRGB leds[amount_led];
WiFiClient client;
Settings settings;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncWebSocketClient *wsClient;
DynamicJsonDocument jSon(1024); // main Json

static void handle_update_progress_cb(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  uint32_t free_space = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
  if (!index)
  {
    Update.runAsync(true);
    if (!Update.begin(free_space))
    {
      Update.printError(Serial);
    }
  }

  if (Update.write(data, len) != len)
  {
    Update.printError(Serial);
  }

  if (final)
  {
    if (!Update.end(true))
    {
      Update.printError(Serial);
    }
    else
    {
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Please wait while the device is booting new Firmware");
      response->addHeader("Refresh", "10; url=/");
      response->addHeader("Connection", "close");
      request->send(response);
      restartNow = true; // Set flag so main loop can issue restart call
      serialState("Update Complete restarting....");
    }
  }
}

void notifyClients() // Call client for new data
{
  if (wsClient != nullptr && wsClient->canSend())
  {
    serializeJson(jSon, jsonBuffer);
    wsClient->text(jsonBuffer);
  }
}


void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) // ws Events
{
  switch (type)
  {
  case WS_EVT_CONNECT:
    wsClient = client;
    notifyClients();
    serialState("Websocket Client conneted: " + String(client->id()));
    break;
  case WS_EVT_DISCONNECT:
    wsClient = nullptr;
    serialState("Websocket Client disconneted: " + String(client->id()));
    break;
  case WS_EVT_DATA:
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}

void setup()
{
  FastLED.addLeds<WS2812B, ledPin, GRB>(leds, amount_led);
  leds[0] = CRGB::Green; // first Led init OK
  FastLED.show();
  Serial.begin(9600);
  pinMode(sensorIn_1, INPUT_PULLUP);
  pinMode(sensorIn_2, INPUT_PULLUP);
  pinMode(buzzerOut, OUTPUT);
  pinMode(bellOut, OUTPUT);
  leds[1] = CRGB::Green; // Pinmode init OK
  FastLED.show();
  settings.load();
  leds[2] = CRGB::Green; // settings load OK
  FastLED.show();
  WiFi.persistent(true);
  leds[3] = CRGB::Green;          // wifi manager loaded OK
  FastLED.show();
  bool wifiConnected = WiFi.softAP("AEB", "1234567890");

  if (wifiConnected) // if wifi connected, start some webservers
  {
    leds[4] = CRGB::Green; // wifi connect OK
    FastLED.show();

    server.on("/bootstrap.bundle.min.js", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                AsyncWebServerResponse *response = request->beginResponse_P( 200, "text/javascript", bootstrap_bundle_min_js, bootstrap_bundle_min_js_len, nullptr );
                response->addHeader("Content-Encoding", "gzip");
                request->send(response);
              });
    server.on("/bootstrap-icons.css", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                AsyncWebServerResponse *response = request->beginResponse_P( 200, "text/css", bootstrap_icons_css, bootstrap_icons_css_len, nullptr );
                response->addHeader("Content-Encoding", "gzip");
                request->send(response);
              });
    server.on("/bootstrap.min.css", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                AsyncWebServerResponse *response = request->beginResponse_P( 200, "text/css", bootstrap_min_css, bootstrap_min_css_len, nullptr );
                response->addHeader("Content-Encoding", "gzip");
                request->send(response);
              });

    server.on("/jquery.min.js", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                AsyncWebServerResponse *response = request->beginResponse_P( 200, "text/javascript", jquery_min_js, jquery_min_js_len, nullptr );
                response->addHeader("Content-Encoding", "gzip");
                request->send(response);
              });

        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", HTML_MAIN, htmlProcessor);
      request->send(response); });

    server.on("/livejson", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                AsyncResponseStream *response = request->beginResponseStream("application/json");
                serializeJson(jSon, *response);
                request->send(response); });

    server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Please wait while the device reboots...");
                response->addHeader("Refresh", "5; url=/");
                response->addHeader("Connection", "close");
                request->send(response);
                restartNow = true; });

      server.on("/confirmreset", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", HTML_CONFIRM_RESET, htmlProcessor);
      request->send(response); });

    server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Device is Erasing...");
                response->addHeader("Refresh", "15; url=/");
                response->addHeader("Connection", "close");
                request->send(response);
                delay(1000);
                settings.reset();
                ESP.eraseConfig();
                ESP.restart(); });

      server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", HTML_SETTINGS, htmlProcessor);
      request->send(response); });

      server.on("/settingsedit", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", HTML_SETTINGS_EDIT, htmlProcessor);
      request->send(response); });

    server.on("/settingsjson", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                AsyncResponseStream *response = request->beginResponseStream("application/json");
                DynamicJsonDocument SettingsJson(128);
                SettingsJson["devicename"] = settings.deviceName;
                SettingsJson["cooldowntime"] = settings.coolDownTime;
                SettingsJson["bellsignaltime"] = settings.bellSignalTime;
                SettingsJson["signaltimeout"] = settings.signalTimeout;

                serializeJson(SettingsJson, *response);
                request->send(response); });

    server.on("/settingssave", HTTP_POST, [](AsyncWebServerRequest *request)
              {
                request->redirect("/settings");
                settings.deviceName = request->arg("post_deviceName");
                settings.coolDownTime = request->arg("post_cooldownTime").toInt();
                settings.bellSignalTime = request->arg("post_bellSignalTime").toInt();
                settings.signalTimeout = request->arg("post_signalTimeout").toInt();
                settings.save();
                settings.load(); });

    server.on(
        "/update", HTTP_POST, [](AsyncWebServerRequest *request)
        {
          request->send(200);
          request->redirect("/"); },
        handle_update_progress_cb);
    leds[5] = CRGB::Green; // Webserver Start OK
    FastLED.show();
    ws.onEvent(onEvent);
    server.addHandler(&ws);
    server.begin();
  }
  else
  {
    leds[4] = CRGB::Red; // wifi not connected OK
    FastLED.show();
  }

  //-----------------------------------------------------------------------------------
  for (size_t i = 750; i < 900; i++) // make a startup Sound
  {
    tone(buzzerOut, i);
    delay(2);
  }
  noTone(buzzerOut); // shut off the tone

  Serial.println("Loading settings...");
  Serial.println("Device Name: " + settings.deviceName);
  Serial.println("Cooldown Time: " + String(settings.coolDownTime));
  Serial.println("Bell Signal Time: " + String(settings.bellSignalTime));
  Serial.println("Signal Timeout: " + String(settings.signalTimeout));
  Serial.println("Setup Complete... Start watching");
  leds[6] = CRGB::Green; // Buzzer Start OK
  FastLED.show();
  MDNS.begin(settings.deviceName);
  WiFi.hostname(settings.deviceName);
  leds[7] = CRGB::Green; // MDNS WIFI Start OK
  FastLED.show();
}

void loop()
{
  sensor1 = digitalRead(sensorIn_1);
  sensor2 = digitalRead(sensorIn_2);
  digitalWrite(bellOut, bell);
  stateRing();
  stateLED();

    jSon["device_name"] = settings.deviceName;
    jSon["amountIn"] = amountIn;
    jSon["amountOut"] = amountOut;
    jSon["present"] = (amountIn - amountOut);
    jSon["vmaxin"] = vmaxIngoing;
    jSon["vmaxout"] = vmaxOutgoing;

    ws.cleanupClients(); // clean unused client connections

    if (stateChange != state)
    {
      notifyClients();
      stateChange = state;
    }

  if (restartNow)
  {
    serialState("Restart");
    delay(250);
    ESP.restart();
  }
  if (millis() >= 86400000) // reboot every 24h
  {
    serialState("Restart");
    delay(250);
    ESP.restart();
  }
}

void stateRing() // Statmachine for sensors
{
  switch (state)
  {
  case IDLE:
    if (sensor1 != sensorState_1)
    {
      state = IN;
      lastStateMillis = millis();
    }
    else if (sensor2 != sensorState_2)
    {
      state = OUT;
      lastStateMillis = millis();
    }
    break;

  case IN:
    if (sensor2 != sensorState_2)
    {
      vmaxInTemp = 98 * 3.6 / (millis() - lastStateMillis);
      if (vmaxIngoing < vmaxInTemp)
      {
        vmaxIngoing = vmaxInTemp;
      }
      serialState("Ingoing Speed: " + String(vmaxInTemp));
      serialState("Vmax Ingoing: " + String(vmaxIngoing));

      state = RING;
      lastStateMillis = millis();
    }
    else if (millis() >= (lastStateMillis + settings.signalTimeout) && sensor2 == sensorState_2 && sensor1 == sensorState_1)
    {
      state = IDLE;
    }
    break;

  case RING:
    bell = true;
    tone(buzzerOut, 800);
    if (millis() >= (lastStateMillis + (settings.bellSignalTime / 2)))
    {
      tone(buzzerOut, 650);
    }
    if (millis() >= (lastStateMillis + settings.bellSignalTime))
    {
      noTone(buzzerOut);
      bell = false;
      amountIn++;
      serialState("Objects going in: " + String(amountIn));
      state = COOLDOWN;
      lastStateMillis = millis();
    }
    break;

  case OUT:
    if (sensor1 != sensorState_1)
    {
      vmaxOutTemp = 98 * 3.6 / (millis() - lastStateMillis);
      if (vmaxOutgoing < vmaxOutTemp)
      {
        vmaxOutgoing = vmaxOutTemp;
      }
      serialState("Outgoing Speed: " + String(vmaxOutTemp));
      serialState("Vmax Outgoing: " + String(vmaxOutgoing));

      amountOut++;
      serialState("Objects going out: " + String(amountOut));
      state = COOLDOWN;
    }
    else if (millis() >= (lastStateMillis + settings.signalTimeout) && sensor2 == sensorState_2 && sensor1 == sensorState_1)
    {
      state = IDLE;
    }
    break;

  case COOLDOWN:
    if (millis() >= (lastStateMillis + settings.coolDownTime))
    {
      serialState("Objects inside: " + String((amountIn - amountOut)));
      state = IDLE;
    }
    else if (sensor1 != sensorState_1 || sensor2 != sensorState_2)
    {
      lastStateMillis = millis();
    }
    break;

  default:
    break;
  }
}

void stateLED() // LED animate states
{
  if (state != ledChange)
    FastLED.clear(true);
  switch (state)
  {
  case COOLDOWN:
    for (size_t i = 0; i < amount_led; i++)
    {
      leds[i] = CRGB::Blue;
    }
    if (state != ledChange)
      FastLED.show();
    break;

  case IDLE:
    if (state != ledChange)
    {
      for (size_t i = 0; i < amount_led; i++)
      {
        leds[i] = CRGB::Green;
      }
      FastLED.show();
    }
    if (millis() >= (lastStateMillis + 10000))
    {
      fadeToBlackBy(leds, amount_led, 1);
      FastLED.show();
    }
    break;

  case IN:
    if (millis() >= (wsTime + 50))
    {
      leds[wsPixNum] = CRGB(255, 119, 0);
      FastLED.show();
      (wsPixNum < (amount_led - 1)) ? wsPixNum++ : wsPixNum = 0;
      fadeToBlackBy(leds, amount_led, 50);
      wsTime = millis();
    }
    break;

  case OUT:
    if (millis() >= (wsTime + 50))
    {
      leds[wsPixNum] = CRGB(212, 255, 0);
      FastLED.show();
      (wsPixNum > 0) ? wsPixNum-- : wsPixNum = (amount_led - 1);
      fadeToBlackBy(leds, amount_led, 50);
      wsTime = millis();
    }
    break;

  case RING:
    for (size_t i = 0; i < amount_led; i++)
    {
      leds[i] = CRGB::Red;
    }
    if (state != ledChange)
      FastLED.show();
    break;

  default:
    break;
  }
  ledChange = state; // set the current state to change
}

void serialState(String message) // serial messages, only message changes will go out
{
  String tmpMessage;
  if (tmpMessage != message)
  {
    Serial.println(message);
    tmpMessage = message;
  }
}