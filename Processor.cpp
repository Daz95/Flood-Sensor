/*
  EngineeringMenu.h - Library for dealing with processor settings i.e. its interrupt/led pins.
  Created by Natalie Mclaren, December 18, 2017.
*/
#define VBATPIN A7

#include "Processor.h"
#include <TheThingsNetwork.h>
#include <SPI.h>
#include <SD.h>
#include "Arduino.h"
#include "SDCard.h"
#include "Sensor.h"
#include "Lorawan.h"

/*
 * 
 */
Processor::Processor(Sensor *sensor, SDCard *sdCard, Lorawan *lorawan, byte ledPin, byte interruptPin, int16_t delayPeriod, int16_t delayPeriodARMode, int16_t ARModeActivationThreshold, int16_t ignoreThreshold)
{
  this->sensor = sensor;
  this->sdCard = sdCard;
  this->lorawan = lorawan;
  
  this->ledPin = ledPin;
  this->interruptPin = interruptPin;
  
  this->delayPeriod = delayPeriod;
  this->delayPeriodARMode = delayPeriodARMode;
  this->ARModeActivationThreshold = ARModeActivationThreshold; //Threshold (mm) to trigger Accelerated Readings mode
                                     //defaulted to a value that should never trigger (20 meters - out of range of sensor)
  this->ignoreThreshold = ignoreThreshold; //Threshold (mm) for which the sensor should ignore readings - don't send any info to server
  
  this->state = LOW;
  this->stillHereCount = 0;
}

// State Loop functions

/*
 * 
 */
void Processor::init()
{
    lorawan->join();
  
    String inStr;
    int16_t initialDistanceToRiverTop;
    int16_t currentDistanceToRiverTop;

    // Whilst inStr is null, do nothing, skip
    while ((inStr = Serial.readString()) == NULL){}
    initialRiverDepth = inStr.toInt();
    //Serial.println(initialRiverDepth); 
    initialDistanceToRiverTop = analogRead(sensor->analogPin) * 5;
    sensor->distanceToRiverBed = initialRiverDepth + initialDistanceToRiverTop;
}

/*
 * 
 */
float Processor::getBatteryVoltage()
{
  float measuredvbat = analogRead(VBATPIN);
  //** Get a few samples, and smooth values to output (other influences can affect the voltage) e.g. over a period of 10 seconds **
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  return measuredvbat;
}

/*
 * 
 */
uint8_t Processor::getPowerLevel()
{
  return (uint8_t) ((getBatteryVoltage() - 3.2) * 100);
}

/*
 * 
 */
void Processor::readingProcess()
{
  int16_t currentRiverLevel = sensor->getCurrentMeasurement();
  
  if(sensor->isCurrentWorthSending(currentRiverLevel))
  {
    ttn_response_t status = lorawan->sendReading(currentRiverLevel, getPowerLevel());
   
    if(status != TTN_ERROR_SEND_COMMAND_FAILED) {
      sensor->lastMeasurementSent = currentRiverLevel;
      sdCard->printToLog(currentRiverLevel);
      stillHereCount = 0;
    } else {
      //ERROR
    }
  } else if (stillHereCount >= 24) {
    ttn_response_t status = lorawan->sendStillAlive(getPowerLevel());
       
    if(status == TTN_ERROR_SEND_COMMAND_FAILED) {
      // ERROR
    } else {
      stillHereCount = 0; 
    }
  } else {
    stillHereCount++;
  }
}

// AR Mode
void Processor::adjustARModeDelay(int16_t newDelayPeriod){ //adjust accelerated readings mode with new frequent delay period
  delayPeriodARMode = 1000;
}

void Processor::adjustARModeThreshold(int16_t newActivationThreshold){ //adjust accelerated readings mode with new activation threshold (mm)
  ARModeActivationThreshold = newActivationThreshold;
}

void Processor::activateOrDeactivateARMode() { // swaps delayPeriod and delayPeriodARMode variables to activate/deactivate Accelerated Readings mode
  int16_t tempPeriod = 0;
  tempPeriod = delayPeriod;
  delayPeriod = delayPeriodARMode;
  delayPeriodARMode = tempPeriod;
}

void Processor::adjustIgnoreThreshold(int16_t newIgnoreThreshold){ 
  ignoreThreshold = newIgnoreThreshold;
}


// Helpers
/*
 * 
 */
void Processor::writeStatus()
{
  digitalWrite(this->ledPin, this->state);
}

/*
 * 
 */
void Processor::delayWithPeriod()
{
  char currentMeasurementPeriodMessage[] PROGMEM = "Current measurement period is..";
  Serial.println(currentMeasurementPeriodMessage);
  
  Serial.println(this->delayPeriod);
  delay(this->delayPeriod);
}

// Setters

/*
 * 
 */
void Processor::changeMeasurementPeriod(int16_t minutes)
{
    this->delayPeriod = minutes;
}

/*
 * 
 */
void Processor::printToSDLog(int16_t lastMeasurementSent)
{
  this->sdCard->printToLog(lastMeasurementSent);
}

/*
 * 
 */
void Processor::printCurrentMeasurementToSD(int16_t currentMeasurement)
{
  this->sdCard->printCurrentMeasurement(currentMeasurement);
}


