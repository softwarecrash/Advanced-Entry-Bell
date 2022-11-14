// https://wokwi.com/projects/346341081902744147

// das noch einbauen
// https://github.com/thomasfredericks/Bounce2

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
#include <ESPAsyncWiFiManager.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <NTPClient.h> //https://lastminuteengineers.com/esp8266-ntp-server-date-time-tutorial/

//einbauen & testen
#include "NTP.h"//https://github.com/sstaub/NTP?utm_source=platformio&utm_medium=piohome


#include <WiFiUdp.h>

#include <FastLED.h>

#include "Settings.h" // save and load config data

#include "webpages/htmlCase.h"     // The HTML Konstructor
#include "webpages/main.h"         // landing page with menu
#include "webpages/settings.h"     // settings page
#include "webpages/settingsedit.h" // edit settings page

//------------------------ Basic Configuration----------------------------
#define sensorIn_1 D5 // Pin of the first sensor when entering the room
#define sensorIn_2 D6 // Pin of the second sensor when entering the room
#define buzzerOut D7  // Pin for the buzzer that can make *BEEP BOOP BEEP*
#define bellOut D8    // Pin for external switch or relay that will ring the big bell
#define ledPin D0     // pin for ws2812 rgb stripe

#define sensorState_1 true // idle state of sensor 1
#define sensorState_2 true // idle state of sensor 2
#define bellOutState false  // idle state of pinout external switch
#define amount_led 8

long unsigned int coolDownTime = 2000;   // time after bell rings, to get back to detection
long unsigned int bellSignalTime = 1500; // signal duration for bell pin
long unsigned int signalTimeout = 1000;  // time after one pin is triggered to get back to detection

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
int amountIn;                      // counter ingoing
int amountOut;                     // counter outgoing
long unsigned int lastStateMillis; // time from last statechange
long unsigned int wsTime = 0;      // animate timer
int wsPixNum = 0;                  // animate led counter
bool shouldSaveConfig = false;     // flag for saving data
bool restartNow = false;           // restart flag
char jsonBuffer[1024];             // buffer for serialize json
const long utcOffsetSec = 3600;    // Time offset in Seconds
const long ntpUpdate = 60000;      // ntp update interval
float vmaxIngoing = 0.0;           // max measured ingoing speed
float vmaxOutgoing = 0.0;          // max measured outgoing speed
float vmaxOutTemp;
float vmaxInTemp;

long unsigned int testtime;

//------------------------------- init struct and classes---------------------------------------
CRGB leds[amount_led];
WiFiClient client;
Settings settings;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncWebSocketClient *wsClient;
DNSServer dns;
DynamicJsonDocument jSon(1024); // main Json
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetSec, ntpUpdate);

void saveConfigCallback() // callback for data saving
{
  shouldSaveConfig = true;
}

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

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) // prosessing callbacks from website
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
    if (strcmp((char *)data, "dischargeFetSwitch_on") == 0)
    {
    }
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
    handleWebSocketMessage(arg, data, len);
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
  AsyncWiFiManager wm(&server, &dns); // in init teil verschoben
  wm.setSaveConfigCallback(saveConfigCallback);
  AsyncWiFiManagerParameter custom_device_name("device_name", "Device Name", "Advanced Entry Bell", 32);
  AsyncWiFiManagerParameter custom_coolDown_time("coolDown_time", "Cooldown Time (ms)", "1500", 5);
  AsyncWiFiManagerParameter custom_bellSignal_time("bellSignal_time", "Bell Pulse Time (ms)", "500", 5);
  AsyncWiFiManagerParameter custom_signal_timeout("signal_timeout", "Signal Timeout (ms)", "1000", 5);
  wm.addParameter(&custom_device_name);
  wm.addParameter(&custom_coolDown_time);
  wm.addParameter(&custom_bellSignal_time);
  wm.addParameter(&custom_signal_timeout);
  wm.setConnectTimeout(10);       // how long to try to connect for before continuing
  wm.setConfigPortalTimeout(120); // auto close configportal after n seconds
  leds[3] = CRGB::Green;          // wifi manager loaded OK
  FastLED.show();
  bool wifiConnected = wm.autoConnect("AEB-AP");

  if (shouldSaveConfig) // save settings if wifi setup is fire up
  {
    settings.deviceName = custom_device_name.getValue();
    settings.coolDownTime = atoi(custom_coolDown_time.getValue());
    settings.bellSignalTime = atoi(custom_bellSignal_time.getValue());
    settings.signalTimeout = atoi(custom_signal_timeout.getValue());
    settings.save();
    delay(500);
    ESP.restart();
  }

  if (wifiConnected) // if wifi connected, start some webservers
  {
    leds[4] = CRGB::Green; // wifi connect OK
    FastLED.show();
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                AsyncResponseStream *response = request->beginResponseStream("text/html");
                response->printf_P(HTML_HEAD);
                response->printf_P(HTML_MAIN);
                response->printf_P(HTML_FOOT);
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
                AsyncResponseStream *response = request->beginResponseStream("text/html");
                response->printf_P(HTML_HEAD);
                response->printf_P(HTML_CONFIRM_RESET);
                response->printf_P(HTML_FOOT);
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
                AsyncResponseStream *response = request->beginResponseStream("text/html");
                response->printf_P(HTML_HEAD);
                response->printf_P(HTML_SETTINGS);
                response->printf_P(HTML_FOOT);
                request->send(response); });

    server.on("/settingsedit", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                AsyncResponseStream *response = request->beginResponseStream("text/html");
                response->printf_P(HTML_HEAD);
                response->printf_P(HTML_SETTINGS_EDIT);
                response->printf_P(HTML_FOOT);
                request->send(response); });

    server.on("/settingsjson", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                AsyncResponseStream *response = request->beginResponseStream("application/json");
                DynamicJsonDocument SettingsJson(128);
                SettingsJson["devicename"] = settings.deviceName;
                SettingsJson["cooldowntime"] = settings.coolDownTime;
                SettingsJson["bellsignaltime"] = settings.bellSignalTime;
                SettingsJson["signaltimeout"] = settings.signalTimeout;
                //SettingsJson["ntp_time"] = _settings._mqttPassword; zeit folgt später

                serializeJson(SettingsJson, *response);
                request->send(response); });

    server.on("/settingssave", HTTP_POST, [](AsyncWebServerRequest *request)
              {
                request->redirect("/settings");
                settings.deviceName = request->arg("post_deviceName");
                settings.coolDownTime = request->arg("post_mqttServer").toInt();
                settings.bellSignalTime = request->arg("post_mqttPort").toInt();
                settings.signalTimeout = request->arg("post_mqttUser").toInt();
                settings.save();
                settings.load(); });

    server.on(
        "/update", HTTP_POST, [](AsyncWebServerRequest *request)
        {
          //updateProgress = true;
          //delay(500);
          request->send(200);
          request->redirect("/"); },
        handle_update_progress_cb);
    leds[5] = CRGB::Green; // Webserver Start OK
    FastLED.show();
    // set the device name
    // MDNS.begin(settings.deviceName);
    // WiFi.hostname(settings.deviceName);
    ws.onEvent(onEvent);
    server.addHandler(&ws);
    server.begin();
    MDNS.addService("http", "tcp", 80);
    timeClient.begin();
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
  // for (size_t i = 0; i < amount_led; i++) // Set the Led to start color
  //{
  //   leds[i] = CRGB::BlueViolet;
  // }
  Serial.println("Loading settings...");
  Serial.println("Device Name: " + settings.deviceName);
  Serial.println("Cooldown Time: " + String(settings.coolDownTime));
  Serial.println("Bell Signal Time: " + String(settings.bellSignalTime));
  Serial.println("Signal Timeout: " + String(settings.signalTimeout));
  Serial.println("RTSP URL: " + settings.rtspUrl);
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

  if (WiFi.status() == WL_CONNECTED) // No use going to next step unless WIFI is up and running.
  {

    timeClient.update();

    jSon["device_name"] = settings.deviceName;
    jSon["amountIn"] = amountIn;
    jSon["amountOut"] = amountOut;
    jSon["present"] = (amountIn - amountOut);
    jSon["vmaxin"] = vmaxIngoing;
    jSon["vmaxout"] = vmaxOutgoing;

    ws.cleanupClients(); // clean unused client connections
    MDNS.update();

    // only for testing
    if (millis() >= (testtime + 1000))
    {
      amountIn++;
      notifyClients();
      testtime = millis();
    }
  }

  if (timeClient.getHours() == 00 && timeClient.getMinutes() == 00 && timeClient.getSeconds() == 00) // restart at daychange
  {
    amountIn = 0;
    amountOut = 0;
  }

  if (restartNow)
  {
    serialState("Restart");
    delay(250);
    ESP.restart();
  }
  yield();
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
      vmaxInTemp = 98*3.6 / (millis() - lastStateMillis);
      if (vmaxIngoing < vmaxInTemp)
      {
        vmaxIngoing = vmaxInTemp;
      }
      serialState("Ingoing Speed: " + String(vmaxInTemp));
      serialState("Vmax Ingoing: " + String(vmaxIngoing));
      
      state = RING;
      lastStateMillis = millis();
    }
    else if (millis() >= (lastStateMillis + signalTimeout))
    {
      state = IDLE;
    }
    break;

  case RING:
    bell = true;
    tone(buzzerOut, 800);
    if (millis() >= (lastStateMillis + (bellSignalTime / 2)))
    {
      tone(buzzerOut, 650);
    }
    if (millis() >= (lastStateMillis + bellSignalTime))
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
      vmaxOutTemp = 98*3.6 / (millis() - lastStateMillis);
      if (vmaxOutgoing < vmaxOutTemp)
      {
        vmaxOutgoing = vmaxOutTemp;
      }
      serialState("Outgoing Speed: " + String(vmaxOutTemp));
      serialState("Vmax Outgoing: " + String(vmaxOutgoing));

    if (sensor2 != sensorState_2 && sensor1 != sensorState_1)
    {
      amountOut++;
      serialState("Objects going out: " + String(amountOut));
      state = COOLDOWN;
    }
    else if (millis() >= (lastStateMillis + signalTimeout))
    {
      state = IDLE;
    }
    break;

  case COOLDOWN:
    if (millis() >= (lastStateMillis + coolDownTime))
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
    if(millis() >= (lastStateMillis + 60000))
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

      if (wsPixNum < (amount_led - 1))
      {
        wsPixNum++;
      }
      else
      {
        wsPixNum = 0;
      }
      fadeToBlackBy(leds, amount_led, 200);
      wsTime = millis();
    }
    break;

  case OUT:

    if (millis() >= (wsTime + 50))
    {
      leds[wsPixNum] = CRGB(212, 255, 0);
      FastLED.show();
      if (wsPixNum > 0)
      {
        wsPixNum--;
      }
      else
      {
        wsPixNum = (amount_led - 1);
      }
      fadeToBlackBy(leds, amount_led, 200);
      wsTime = millis();
    }
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

boolean summertime_EU(int year, byte month, byte day, byte hour, byte tzHours)
// European Daylight Savings Time calculation by "jurs" for German Arduino Forum
// input parameters: "normal time" for year, month, day, hour and tzHours (0=UTC, 1=MEZ)
// return value: returns true during Daylight Saving Time, false otherwise
{ 
  if (month<3 || month>10) return false; // keine Sommerzeit in Jan, Feb, Nov, Dez
  if (month>3 && month<10) return true; // Sommerzeit in Apr, Mai, Jun, Jul, Aug, Sep
  if (month==3 && (hour + 24 * day)>=(1 + tzHours + 24*(31 - (5 * year /4 + 4) % 7)) || month==10 && (hour + 24 * day)<(1 + tzHours + 24*(31 - (5 * year /4 + 1) % 7))) 
    return true; 
  else 
    return false;
}