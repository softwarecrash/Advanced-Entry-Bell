// https://wokwi.com/projects/346341081902744147

// das noch einbauen
// https://github.com/thomasfredericks/Bounce2

/*
  TODO:
  - webseite mit zählern (reset um mitternacht)
  - ntp einbauen (sommer winterzeit nicht vergessen)
  - einstellbares zeitfenster wann die klingel arbeiten soll
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
#include <FastLED.h>

#include "Settings.h"

#include "webpages/htmlCase.h"     // The HTML Konstructor
#include "webpages/main.h"         // landing page with menu
#include "webpages/settings.h"     // settings page
#include "webpages/settingsedit.h" // mqtt settings page
//------------------------ Basic Configuration----------------------------

#define sensorIn_1 D5 // Pin of the first sensor when entering the room
#define sensorIn_2 D6 // Pin of the second sensor when entering the room
#define buzzerOut D7  // Pin for the buzzer that can make *BEEP BOOP BEEP*
#define bellOut D8    // Pin for external switch or relay that will ring the big bell
#define ledPin D0     // pin for ws2812 rgb stripe

#define sensorState_1 false // idle state of sensor 1
#define sensorState_2 false // idle state of sensor 2
#define bellOutState false  // idle state of pinout external switch
#define amount_led 8

CRGB leds[amount_led];

long unsigned int coolDownTime = 2000;   // time after bell rings, to get back to detection
long unsigned int bellSignalTime = 1500; // signal duration for bell pin
long unsigned int signalTimeout = 1000;  // time after one pin is triggered to get back to detection

//------------------------------------------------------------------------

typedef enum
{
  IDLE,                 // idle, wait for signal
  IN,                   // Possibil a person going in, first sensor triggered frist
  RING,                 // first sensor triggered, second sensor triggered, make a ring
  OUT,                  // Possibil a person going out, second sensor triggered first
  COOLDOWN              // after action, let cooldown
} stateList;            // list of all states
stateList state = IDLE; // set the initial state
bool sensor1;
bool sensor2;
bool buzzer;
bool bell;
byte ledChange; // switch for changed led data
int amountIn;
int amountOut;
long unsigned int lastStateMillis;

void setup()
{
  Serial.begin(9600);
  pinMode(sensorIn_1, INPUT_PULLUP);
  pinMode(sensorIn_2, INPUT_PULLUP);
  pinMode(buzzerOut, OUTPUT);
  pinMode(bellOut, OUTPUT);

  FastLED.addLeds<WS2812B, ledPin, GRB>(leds, amount_led);
  //-----------------------------------------------------------------------------------
  for (size_t i = 750; i < 900; i++)
  {
    tone(buzzerOut, i);
    delay(2);
  }
  noTone(buzzerOut);

  Serial.println("Setup Complete... Start watching");

  leds[0] = CRGB::BlueViolet;
  leds[1] = CRGB::BlueViolet;
  leds[3] = CRGB::BlueViolet;
  leds[4] = CRGB::BlueViolet;
  leds[6] = CRGB::BlueViolet;
  leds[7] = CRGB::BlueViolet;
  FastLED.show();
}

void loop()
{
  sensor1 = digitalRead(sensorIn_1);
  sensor2 = digitalRead(sensorIn_2);
  digitalWrite(bellOut, bell);
  stateRing();
  stateLED();
}

void stateRing()
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
    if (millis() >= (lastStateMillis + bellSignalTime))
    {
      bell = false;
      amountIn++;
      serialState("Objects going in: " + String(amountIn));
      state = COOLDOWN;
      lastStateMillis = millis();
    }
    break;

  case OUT:
    // hier haut noch nicht ganz das hin was soll
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

void stateLED()
{
  if (state != ledChange)
  {
    ledChange = state; // set the current state to change

    FastLED.clear(true);
    switch (state)
    {
    case COOLDOWN:
      for (size_t i = 0; i < amount_led; i++)
      {
        leds[i] = CRGB::Blue;
      }
      break;

    case IDLE:
      for (size_t i = 0; i < amount_led; i++)
      {
        leds[i] = CRGB::Green;
      }
      break;

    case IN:
      for (int i = 0; i < amount_led; i++)
      {
        leds[i] = CRGB(255, 119, 0);
        FastLED.show();
        FastLED.delay(50);
        fadeToBlackBy(leds, amount_led, 200);
      }
      break;

    case OUT:
      for (int i = (amount_led - 1); i >= 0; i--)
      {
        leds[i] = CRGB(212, 255, 0);
        FastLED.show();
        FastLED.delay(50);
        fadeToBlackBy(leds, amount_led, 200);
      }
      break;
    }
    FastLED.show();
  }
}

void serialState(String message)
{
  String tmpMessage;
  if (tmpMessage != message)
  {
    Serial.println(message);
    tmpMessage = message;
  }
}