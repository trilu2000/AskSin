//- -----------------------------------------------------------------------------------------------------------------------
// AskSin driver implementation
// 2013-08-03 <horst@diebittners.de> Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
//- Low battery alarm -----------------------------------------------------------------------------------------------------
//- with a lot of contribution from Dirk at FHEM forum
//- -----------------------------------------------------------------------------------------------------------------------

#ifndef BATTERY_H
#define BATTERY_H

#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif

#include <Serial.h>


class Battery {
  public://----------------------------------------------------------------------------------------------------------------
	
	uint8_t state;																		// status byte for message handling
	void config(uint8_t mode, uint8_t pin, uint16_t time);								// mode 0 is off, mode 1 for internal battery check - more to come
	void setVoltage(uint8_t tenthVolts);												// set the reference voltage in 0.1 volts

	void poll(void);																	// poll for periodic check

  private://---------------------------------------------------------------------------------------------------------------
	uint8_t  tMode;																		// remember the mode
	uint8_t  tTenthVolts;																// remember the tenth volts set
	uint16_t tTime;																		// remember the time for periodic check
	uint32_t nTime;																		// timer for periodic check

	#define BATTERY_NUM_MESS_ADC       64
	#define BATTERY_DUMMY_NUM_MESS_ADC 10
	#define AVR_BANDGAP_VOLTAGE        1100UL											// Band gap reference for Atmega328p
	uint16_t getBatteryVoltage(uint32_t bandgapVoltage);								// measure the battery
};

#endif
