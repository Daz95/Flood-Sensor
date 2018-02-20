/*
  Processor.h - Library for dealing with processor settings i.e. its interrupt/led pins.
  Created by Natalie Mclaren, December 18, 2017.
*/

#ifndef Processor_h
#define Processor_h

#include "SDCard.h"
#include "Sensor.h"
#include "Lorawan.h"

/**
 * 
 */
class Processor
{  
  public:
    volatile byte state;
    int16_t delayPeriod;
    int16_t measurementPeriod;

    // Constructors
    Processor(Sensor *sensor, SDCard *sdCard, Lorawan *lorawan, byte ledPin, byte interrputPin, int16_t delayPeriod = 5000, int16_t delayPeriodARMode = 1000, int16_t ARModeActivationThreshold = 20000, int16_t ignoreThreshold = 0); 

    // Main States
    void init();
    void readingProcess();

    // AR mode
    void activateOrDeactivateARMode();
    void adjustIgnoreThreshold(int16_t newIgnoreThreshold);
    void adjustARModeDelay(int16_t newDelayPeriod);
    void adjustARModeThreshold(int16_t newActivationThreshold);

    // Helpers
    void writeStatus();
    void delayWithPeriod();
    void changeMeasurementPeriod(int16_t minutes);
    float getBatteryVoltage();
    uint8_t getPowerLevel();
    void printToSDLog(int16_t lastMeasurementSent);
    void printCurrentMeasurementToSD(int16_t currentMeasurement);
    
  private:
    // Components
    Sensor *sensor;
    SDCard *sdCard;
    Lorawan *lorawan;

    // Still Here count
    uint8_t stillHereCount;
    // Initial depth
    int16_t initialRiverDepth;
    // AR mode variables
    int16_t delayPeriodARMode;
    int16_t ARModeActivationThreshold; 
    int16_t ignoreThreshold;
    // Pins
    byte ledPin;
    byte interruptPin;
};

#endif
