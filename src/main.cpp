//https://wokwi.com/projects/346341081902744147


/*
TODO:
- webseite mit zählern (reset um mitternacht)
- ntp einbauen (sommer winterzeit nicht vergessen)
- einstellbares zeitfenster wann die klingel arbeiten soll
- FS für klingelton, abspielbar auf desktop
- videoplayer mit kamerafeed
*/
#include <Arduino.h>
//------------------------ Basic Configuration----------------------------

#define sensorIn_1 8 // Pin of the first sensor when entering the room
#define sensorIn_2 9 // Pin of the second sensor when entering the room
#define buzzerOut 11  // Pin for the buzzer that can make *BEEP BOOP BEEP*
#define bellOut 10    // Pin for external switch or relay that will ring the big bell

#define sensorState_1 false // idle state of sensor 1
#define sensorState_2 false // idle state of sensor 2
#define bellOutState false  // idle state of pinout external switch

long unsigned int coolDownTime = 2000; //time after bell rings, to get back to detection
long unsigned int bellSignalTime = 1500; //signal duration for bell pin
long unsigned int signalTimeout = 1000; //time after one pin is triggered to get back to detection

#define stateLedR 6
#define stateLedG 5
#define stateLedB 3

//------------------------------------------------------------------------

typedef enum
{
  IDLE,
  ARMED,
  LAUNCH,
  OUT,
  COOLDOWN
} stateList;            // list of all states
stateList state = IDLE; // set the initial state
bool sensor1;
bool sensor2;
bool buzzer;
bool bell;
int mountIn;
int amountOut;
long unsigned int lastStateMillis;

void setup()
{
  pinMode(sensorIn_1, INPUT);
  pinMode(sensorIn_2, INPUT);
  pinMode(buzzerOut, OUTPUT);
  pinMode(bellOut, OUTPUT);

  pinMode(stateLedR, OUTPUT);
  pinMode(stateLedG, OUTPUT);
  pinMode(stateLedB, OUTPUT);

  tone(buzzerOut, 55, 500); //https://github.com/connornishijima/arduino-volume1
}

void loop()
{
  sensor1 = digitalRead(sensorIn_1);
  sensor2 = digitalRead(sensorIn_2);
  digitalWrite(bellOut, bell);
  switch (state)
  {
  case IDLE:
    stateLED(0,255,0);
    if (sensor1 != sensorState_1)
    {
      state = ARMED;
      lastStateMillis = millis();
    }
    else if (sensor2 != sensorState_2)
    {
      state = OUT;
      lastStateMillis = millis();
    }
    break;

  case ARMED:

  //weiteres case in dem überprüft wird das wenn zweiter sensor aktiviert und erster deaktiviert wird gewartet wird bis zweiter auch wieder deaktiviert, dann erst auslösen
      stateLED(155,255,0);
    if (sensor2 != sensorState_2)
    {
      state = LAUNCH;
      lastStateMillis = millis();
    }
    else if (millis() >= (lastStateMillis + signalTimeout))
    {
        state = IDLE;
    }
    break;

  case LAUNCH:
  stateLED(255,0,0);
    bell = true;
    if (millis() >= (lastStateMillis + bellSignalTime))
    {
      bell = false;
      mountIn++;
      state = COOLDOWN;
      lastStateMillis = millis();
    }
    break;

  case OUT:
    stateLED(0,255,255);
    if (sensor1 != sensorState_1)
    {
      amountOut++;
      state = IDLE;
    }
    else if (millis() >= (lastStateMillis + signalTimeout))
    {
        state = IDLE;
    }
    break;

  case COOLDOWN:
  stateLED(0,0,255);
    if (millis() >= (lastStateMillis + coolDownTime))
    {
      state = IDLE;
    }
    else if(sensor1 != sensorState_1 || sensor2 != sensorState_2)
    {
      lastStateMillis = millis();
    }
    break;

  default:
    break;
  }
}

void stateLED(int r, int g, int b)
{
  analogWrite(stateLedR, r);
  analogWrite(stateLedG, g);
  analogWrite(stateLedB, b);
}