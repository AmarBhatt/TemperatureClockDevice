#ifndef TempSense_h
#define TempSense_h

/*
* Name: DS18B20 Temperature Sensor
*
* Description: Digital thermometer
*
* Author: Amar Bhatt
*/

// OneWire DS18S20, DS18B20, DS1822 Temperature Example
//
// http://www.pjrc.com/teensy/td_libs_OneWire.html
//
// The DallasTemperature library can do all this work for you!
// http://milesburton.com/Dallas_Temperature_Control_Library

#include "../OneWire/OneWire.h"

using byte = unsigned char;

int connectTemperatureSensor(OneWire* ds, byte *addr, byte* type_s);
bool tempConnected(OneWire* ds, byte *addr); 
int16_t readTempSensor(OneWire* ds, byte *addr, byte *data, byte* present, byte* type_s); 
float convertTemp(bool celsius, int16_t raw); 


#endif
