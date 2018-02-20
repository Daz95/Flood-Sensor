// #include "Ticker-master/Ticker.h"
#include <SPI.h>
#include <SD.h>
#include "EngineeringMenu.h"
#include "EngineeringMenuOptions.h"
#include "Sensor.h"
#include "SDCard.h"
#include "Processor.h"
#include "Lorawan.h"
#define freqPlan TTN_FP_EU868

const byte ledPin = 1;
const byte interruptPin = 13;

Sensor sensor(0);
SDCard sdCard;
Lorawan lorawan(Serial, Serial1, freqPlan);
Processor processor(&sensor, &sdCard, &lorawan, ledPin, interruptPin);
EngineeringMenuOptions options;
EngineeringMenu menu(&sensor, &sdCard, &processor, &options, &lorawan);

/**
 * Sets up 
 * 
 */
void setup()
{
  //  Setup serial baus, Serial1 used for LoRaWAN, Serial for USB communication.
  Serial1.begin(57600);
  Serial.begin(9600); 
  
  // Wait for serial to connect
  while (!Serial){}
  
  // Check for incoming serial data:
  //if (Serial.available() > 0) {
    Serial.println("Set up");

    processor.init();
  //}

  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), setBringUpMenu, CHANGE);
}

/**
 * 
 */
void setBringUpMenu() 
{
  menu.bringUpMenu = true;
}

/**
 * 
 */
void loop()
{
  processor.writeStatus();

  // If button interrupt has been pressed
  if(menu.bringUpMenu) {
    // Load the engineering menu
    menu.loadEngineeringMenu();
  }

  processor.readingProcess();

  processor.delayWithPeriod();

}
