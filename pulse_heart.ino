
/*
>> Pulse Sensor Amped 1.1 <<
This code is for Pulse Sensor Amped by Joel Murphy and Yury Gitman
    www.pulsesensor.com 
    >>> Pulse Sensor purple wire goes to Analog Pin 0 <<<
Pulse Sensor sample aquisition and processing happens in the background via Timer 2 interrupt. 2mS sample rate.
PWM on pins 3 and 11 will not work when using this code, because we are using Timer 2!
The following variables are automatically updated:
Signal :    int that holds the analog signal data straight from the sensor. updated every 2mS.
IBI  :      int that holds the time interval between beats. 2mS resolution.
BPM  :      int that holds the heart rate value, derived every beat, from averaging previous 10 IBI values.
QS  :       boolean that is made true whenever Pulse is found and BPM is updated. User must reset.
Pulse :     boolean that is true when a heartbeat is sensed then false in time with pin13 LED going out.

This code is designed with output serial data to Processing sketch "PulseSensorAmped_Processing-xx"
The Processing sketch is a simple data visualizer. 
All the work to find the heartbeat and determine the heartrate happens in the code below.
Pin 13 LED will blink with heartbeat.
If you want to use pin 13 for something else, adjust the interrupt handler
It will also fade an LED on pin fadePin with every beat. Put an LED and series resistor from fadePin to GND.
Check here for detailed code walkthrough:
http://pulsesensor.myshopify.com/pages/pulse-sensor-amped-arduino-v1dot1

Code Version 02 by Joel Murphy & Yury Gitman  Fall 2012
This update changes the HRV variable name to IBI, which stands for Inter-Beat Interval, for clarity.
Switched the interrupt to Timer2.  500Hz sample rate, 2mS resolution IBI value.
Fade LED pin moved to pin 5 (use of Timer2 disables PWM on pins 3 & 11).
Tidied up inefficiencies since the last version. 
*/


//  VARIABLES
int pulsePin = 0;                 // Pulse Sensor purple wire connected to analog pin 0
// int blinkPin = 13;


// these variables are volatile because they are used during the interrupt service routine!
volatile int BPM;                   // used to hold the pulse rate
volatile int Signal;                // holds the incoming raw data
volatile int IBI = 600;             // holds the time between beats, the Inter-Beat Interval
volatile boolean Pulse = false;     // true when pulse wave is high, false when it's low
volatile boolean QS = false;        // becomes true when Arduoino finds a beat.

#include <avr/pgmspace.h>  //This is in the Arduino library 

int blinkdelay = 80; // Heart brightness control
int runspeed = 45; // How fast blinky

#define HEART_ARRAY_SIZE 27
byte Heart[][HEART_ARRAY_SIZE] PROGMEM={
//                     1                   2
// 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6
  {  1,1, 1,1, 1,1,1,1,1,1,1, 1,1,1,1,1,1,1, 1,1,1,1,1, 1,1,1, 1},
 // {  0,1, 1,0, 0,1,1,1,1,1,0, 0,1,1,1,1,1,0, 0,1,1,1,0, 0,1,0, 0},
  {  0,0, 0,0, 0,0,1,1,1,0,0, 0,0,1,1,1,0,0, 0,0,1,0,0, 0,0,0, 0},
 // {  0,0, 0,0, 0,0,0,1,0,0,0, 0,0,0,1,0,0,0, 0,0,0,0,0, 0,0,0, 0},
  {  0,0, 0,0, 0,0,0,0,0,0,0, 0,0,0,1,0,0,0, 0,0,0,0,0, 0,0,0, 0},
  {  0,0, 0,0, 0,0,0,0,0,0,0, 0,0,0,0,0,0,0, 0,0,0,0,0, 0,0,0, 0},

  {2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};

#define HEART_BLINKY_SIZE (sizeof(Heart) / HEART_ARRAY_SIZE)

int pin1 =8;
int pin2 =9;
int pin3 =10;
int pin4 =11;
int pin5 =12;
int pin6 =13;

const int pins[] = {
  pin1,pin2,pin3,pin4,pin5,pin6};

const int heartpins[27][2] ={
  {pin3, pin1},
  {pin1, pin3},
  {pin2, pin1},
  {pin1, pin2},
  {pin3, pin4},
  {pin4, pin1},
  {pin1, pin4},
  {pin1, pin5},
  {pin6, pin1},
  {pin1, pin6},
  {pin6, pin2},
  {pin4, pin3},
  {pin3, pin5},
  {pin5, pin3},
  {pin5, pin1},
  {pin2, pin5},
  {pin5, pin2},
  {pin2, pin6},
  {pin4, pin5},
  {pin5, pin4},
  {pin3, pin2},
  {pin6, pin5},
  {pin5, pin6},
  {pin4, pin6},
  {pin2, pin3},
  {pin6, pin4},
  {pin4, pin2}
 };


void setup(){

  interruptSetup();                 // sets up to read Pulse Sensor signal every 2mS 
   // UN-COMMENT THE NEXT LINE IF YOU ARE POWERING The Pulse Sensor AT LOW VOLTAGE, 
   // AND APPLY THAT VOLTAGE TO THE A-REF PIN
   //analogReference(EXTERNAL);   
}



void loop(){

  if (QS == true){                       // Quantified Self flag is true when arduino finds a heartbeat
        blink_heart();
        QS = false;                      // reset the Quantified Self flag for next time    
     }
    
//   delay(20);                             //  take a break
}

//
// Turn given pin on lol heart on
//
void turnon(int led) {
  int pospin = heartpins[led][0];
  int negpin = heartpins[led][1];
  pinMode (pospin, OUTPUT);
  pinMode (negpin, OUTPUT);
  digitalWrite (pospin, HIGH);
  digitalWrite (negpin, LOW);
}
  
// Turn heart LOL board all pins off
void alloff() {
  for(int i = 0; i < 6; i++)   {
    pinMode (pins[i], INPUT);
  }
}


// Blink heart LOL board
void blink_heart()
{
  boolean run = true;
  byte k;
  int t = 0;
  while(run == true)   {
    for(int i = 0; i < runspeed; i++)     {
      for(int j = 0; j < 27; j++)       {
        k = pgm_read_byte(&(Heart[t][j]));
        if (k == 2)         {
          t = 0;
          run = false;
        }
        else if(k == 1)         {
          turnon(j);
          delayMicroseconds(blinkdelay);
          alloff();
        }
        else if(k == 0)         {
          delayMicroseconds(blinkdelay);
        }
      }
    }     
    t++;
  }

}



