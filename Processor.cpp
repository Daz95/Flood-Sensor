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
 * Processor constructor
 * Sets default device settings values (delayPeriod etc...)
 * @param {Sensor} {*sensor} pointer to Sensor object.
 * @param {SDCard} {*sdCard} pointer to SDCard object.
 * @param {Lorawan} {*lorawan} pointer to Lorawan object.
 * @param {byte} {ledPin} LED pin number
 * @param {byte} {interruptPin} engineering menu button interrupt pin number
 * @return N/A
 */
Processor::Processor(Sensor *sensor, SDCard *sdCard, Lorawan *lorawan, byte ledPin, byte interruptPin)
{
  this->sensor = sensor;
  this->sdCard = sdCard;
  this->lorawan = lorawan;
  
  this->ledPin = ledPin;
  this->interruptPin = interruptPin;
  
  this->state = LOW;
  this->delayPeriod = 5000;
  this->delayPeriodARMode = 1000;
  this->ARModeOn = false;
  this->ARModeActivationThreshold = 20000; //Threshold (mm) to trigger Accelerated Readings mode
                                     //defaulted to a value that should never trigger (20 meters - out of range of sensor)
  this->ignoreThreshold = 0; //Threshold (mm) for which the sensor should ignore readings - don't send any info to server
}

// State Loop functions

/*
 * Initialise default sensor settings and values
 * Takes initial river depth input from engineer user from serial input to populate other defaults.
 * @param N/A
 * @return {void} N/A
 */
void Processor::init()
{
    //lorawan->join();
  
    String inStr;
    int16_t initialDistanceToRiverTop;
    int16_t currentDistanceToRiverTop;

    // Whilst inStr is null, do nothing, skip
    while ((inStr = Serial.readString()) == NULL){}
    initialRiverDepth = inStr.toInt();
    //Serial.println(initialRiverDepth); 
    initialDistanceToRiverTop = analogRead(sensor->analogPin) * 5;
    sensor->distanceToRiverBed = initialRiverDepth + initialDistanceToRiverTop;

    Serial.println("Current Measurement: ");
    Serial.println(sensor->getCurrentMeasurement());
}

/*
 * Calculates and returns current battery voltage value.
 * Gets an average of 5 readings over 5 seconds.
 * @param N/A
 * @return {float} returns measuredvbat value.
 */
float Processor::getBatteryVoltage()
{
  float avgMeasuredVBat;
  float totalMeasuredVBat = 0;
  
  // Get avg of 5 readings over a period of 4 seconds
  totalMeasuredVBat += analogRead(VBATPIN);
  for(uint8_t i = 0; i < 4; i++)
  {
    delay(1000); //wait for 1 second before taking checking voltage again
    totalMeasuredVBat += analogRead(VBATPIN);
  }
  avgMeasuredVBat = totalMeasuredVBat / 5;
  avgMeasuredVBat *= 2;    // we divided by 2, so multiply back
  avgMeasuredVBat *= 3.3;  // Multiply by 3.3V, our reference voltage
  avgMeasuredVBat /= 1024; // convert to voltage
  
  return avgMeasuredVBat;
}

/*
 * Converts battery voltage to a byte to be sent to the API 
 * Done to save TTN bandwidth (So we don't need to send send a full floating point number).
 * @param N/A
 * @return {uint8_t} returns voltage as an interger between 1 and 100 to be then recalculated in the API
 */
uint8_t Processor::getBatteryVoltageByte()
{
  return floor((getBatteryVoltage()- 3.2) * 100);
}

/*
 * Calculates and returns estimated battery percentage (powerlevel/capacity) based on voltage and mAh of the battery.
 * Expected default battery:
 * 4.2v max (and 3.2v min cut off)
 * 700mAh
 * @param N/A
 * @return {uint8_t} return battery percentage.
 */
uint8_t Processor::getEstimatedPowerLevel()
{
  //100% capacity based on 2.94 Watt hours (4.2v and 0.7Ah)
  //Remove usless power below 3.2v (2.94 - 2.24 = 0.7) - MaxCapacityVal
  //percentage = (( (voltage * Ah) -2.24) / MaxCapacityVal) * 100
  uint8_t estimatedPercentage = floor( ( ( ( (getBatteryVoltage() * 0.7) - 2.24) / 0.7) * 100)); // We are using a 700mAh battery
  return estimatedPercentage;
}

/*
 * Process current river depth measurement, and send if relevant.
 * @param N/A
 * @return {void} N/A
 */
void Processor::readingProcess()
{
  int16_t currentRiverLevel = sensor->getCurrentMeasurement();

  //Check if it is worth sending and higher than (or equal to) the ignored depth threshold
  if(sensor->isCurrentWorthSending(currentRiverLevel) && currentRiverLevel >= ignoreThreshold)
  { 
    sdCard->printToLog(currentRiverLevel);
    ttn_response_t status = lorawan->sendReading(currentRiverLevel, getBatteryVoltageByte());

    //Log error in SDCard log
    if(status != TTN_ERROR_SEND_COMMAND_FAILED) {
      sensor->lastMeasurementSent = currentRiverLevel;
      sdCard->printToLog(currentRiverLevel);
    }
  } else {
    ttn_response_t status = lorawan->sendStillAlive(getBatteryVoltageByte());
  }

  //Check AR Mode threshold
  if((currentRiverLevel >= ARModeActivationThreshold && !this->ARModeOn) || (currentRiverLevel < ARModeActivationThreshold && this->ARModeOn))
  {
    activateOrDeactivateARMode();
  }
}

// Accelerated Readings (AR) Mode

/*
 * Set delay period to be used whilst the device is in AR (Accelerated Readings) Mode.
 * @param {int16_t} {newDelayPeriod} new delay period to be used.
 * @return {void} N/A
 */
void Processor::adjustARModeDelay(int16_t newDelayPeriod) //adjust accelerated readings mode with new delay period
{
  delayPeriodARMode = newDelayPeriod;
}

/*
 * Set max river depth threshold to trigger AR Mode (Measured in: mm).
 * @param {int16_t} {newActivationThreshold}  new maximum threshold to trigger AR mode.
 * @return {void} N/A
 */
void Processor::adjustARModeThreshold(int16_t newActivationThreshold)
{
  ARModeActivationThreshold = newActivationThreshold;
}

/*
 * Swaps delayPeriod and delayPeriodARMode variables to activate/deactivate Accelerated Readings mode.
 * @param N/A
 * @return {void} N/A
 */
void Processor::activateOrDeactivateARMode()
{
  int16_t tempPeriod = 0;
  tempPeriod = delayPeriod;
  delayPeriod = delayPeriodARMode;
  delayPeriodARMode = tempPeriod;
  ARModeOn = !ARModeOn;
}

/*
 * Set minimum river depth threshold (anything below this value will be ignored, and considered "not worth" sending) (mm).
 * @param {int16_t} {newIgnoreThreshold} new minimum river depth threshold to be set (mm).
 * @return {void} N/A
 */
void Processor::adjustIgnoreThreshold(int16_t newIgnoreThreshold)
{ 
  ignoreThreshold = newIgnoreThreshold;
}


// Helpers
/* 
 * 
 * @param N/A
 * @return {void} N/A
 */
void Processor::writeStatus()
{
  digitalWrite(this->ledPin, this->state);
}

/*
 * Makes the entire device sleep for a specified period of time.
 * Used to sleep inbetween taking river depth measurements.
 * @param N/A
 * @return {void} N/A
 */
void Processor::delayWithPeriod()
{
  //char currentMeasurementPeriodMessage[] PROGMEM = "Current measurement period is..";
  //Serial.println(currentMeasurementPeriodMessage);
  
  //Serial.println(this->delayPeriod);
  delay(this->delayPeriod);
}

// Setters

/*
 * Set the delay period between each measurement to be taken.
 * @param {int16_t} {minutes} new delay period (in minutes) set.
 * @return {void} N/A
 */
void Processor::changeMeasurementPeriod(int16_t minutes)
{
    this->delayPeriod = minutes * 1000;
}

/*
 * Call SDCard class function to store the last measurement sent to a log on the SD card.
 * @param {int16_t} {lastMeasurementSent} last measurement that was sent to the api sucessfully.
 * @return {void} N/A
 */
void Processor::printToSDLog(int16_t lastMeasurementSent)
{
  this->sdCard->printToLog(lastMeasurementSent);
}

/*
 * Call SDCard class function to store the current river depth measurement to a log on the SD card.
 * @param {int16_t} {currentMeasurement} current measurement value to be stored in the SD card log.
 * @return {void} N/A
 */
void Processor::printCurrentMeasurementToSD(int16_t currentMeasurement)
{
  this->sdCard->printCurrentMeasurement(currentMeasurement);
}


