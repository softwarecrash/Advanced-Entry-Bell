//https://wokwi.com/projects/346341081902744147


//das noch einbauen
//https://github.com/thomasfredericks/Bounce2

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

#define sensorIn_1 D5 // Pin of the first sensor when entering the room
#define sensorIn_2 D6 // Pin of the second sensor when entering the room
#define buzzerOut D7  // Pin for the buzzer that can make *BEEP BOOP BEEP*
#define bellOut D8    // Pin for external switch or relay that will ring the big bell

#define sensorState_1 false // idle state of sensor 1
#define sensorState_2 false // idle state of sensor 2
#define bellOutState false  // idle state of pinout external switch

long unsigned int coolDownTime = 2000; //time after bell rings, to get back to detection
long unsigned int bellSignalTime = 1500; //signal duration for bell pin
long unsigned int signalTimeout = 1000; //time after one pin is triggered to get back to detection

#define stateLedR D4
#define stateLedG D3
#define stateLedB D2

//------------------------------------------------------------------------

typedef enum
{
  IDLE,   //idle, wait for signal
  PIN,    //Possibil a person going in, first sensor triggered frist
  RING,   //first sensor triggered, second sensor triggered, make a ring
  POUT,   //Possibil a person going out, second sensor triggered first
  COOLDOWN //after action, let cooldown
} stateList;            // list of all states
stateList state = IDLE; // set the initial state
bool sensor1;
bool sensor2;
bool buzzer;
bool bell;
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

  pinMode(stateLedR, OUTPUT);
  pinMode(stateLedG, OUTPUT);
  pinMode(stateLedB, OUTPUT);

  tone(buzzerOut, 55, 500); //https://github.com/connornishijima/arduino-volume1
  Serial.println("Setup Complete... Start watching");
}

void loop()
{
  sensor1 = digitalRead(sensorIn_1);
  sensor2 = digitalRead(sensorIn_2);
  digitalWrite(bellOut, bell);
  stateRing();
}

void stateRing()
{
switch (state)
  {
    case IDLE:
      stateLED(0, 255, 0);
      if (sensor1 != sensorState_1)
      {
        state = PIN;
        lastStateMillis = millis();
      }
      else if (sensor2 != sensorState_2)
      {
        state = POUT;
        lastStateMillis = millis();
      }
      break;

    case PIN:
      stateLED(155, 255, 0);
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
      stateLED(255, 0, 0);
      bell = true;
      if (millis() >= (lastStateMillis + bellSignalTime))
      {
        bell = false;
        amountIn++;
        serialState("Objects going in: "+String(amountIn));
        state = COOLDOWN;
        lastStateMillis = millis();
      }
      break;

    case POUT:
    //hier haut noch nicht ganz das hin was soll
      stateLED(0, 255, 255);
      if (sensor1 != sensorState_1)
      {
        amountOut++;
        serialState("Objects going out: "+String(amountOut));
        state = COOLDOWN;
      }
      else if (millis() >= (lastStateMillis + signalTimeout))
      {
        state = IDLE;
      }
      break;

    case COOLDOWN:
      stateLED(0, 0, 255);
      if (millis() >= (lastStateMillis + coolDownTime))
      {
        serialState("Objects inside: "+String((amountIn-amountOut)));
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

void stateLED(int r, int g, int b)
{
  analogWrite(stateLedR, r);
  analogWrite(stateLedG, g);
  analogWrite(stateLedB, b);
}

void serialState(String message)
{
  String tmpMessage;
  if(tmpMessage != message)
  {
  Serial.println(message);
  tmpMessage = message;
  }
}