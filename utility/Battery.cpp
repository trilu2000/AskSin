//- -----------------------------------------------------------------------------------------------------------------------
// AskSin driver implementation
// 2013-08-03 <horst@diebittners.de> Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
//- Low battery alarm -----------------------------------------------------------------------------------------------------
//- with a lot of contribution from Dirk at FHEM forum
//- -----------------------------------------------------------------------------------------------------------------------

#include "Battery.h"

void Battery::config(uint8_t mode, uint8_t pin, uint16_t time) {
	tMode = mode;
	tTime = time;
	if (tMode) nTime = millis();
}
void Battery::setVoltage(uint8_t tenthVolts) {
	tTenthVolts = tenthVolts;
}

void Battery::poll(void) {
	if ((tMode == 0) || (nTime > millis())) return;										// nothing to do, step out
	if (tMode == 1) {
		state = ((getBatteryVoltage(AVR_BANDGAP_VOLTAGE)/100) < tTenthVolts)?1:0;		// set the variable accordingly
		// Serial << "v:" << (getBatteryVoltage(AVR_BANDGAP_VOLTAGE)/100) << " s:" << state << '\n';
	}
	nTime = millis() + tTime;
}

/**
 * get battery voltage
 * Mesure AVCC again the the internal band gap reference
 *
 *	REFS1 REFS0          --> internal bandgap reference
 *	MUX3 MUX2 MUX1 MUX0  --> 1110 1.1V (VBG) (for instance Atmega 328p)
 */
uint16_t Battery::getBatteryVoltage(uint32_t bandgapVoltage) {
	uint16_t adcValue = 0;

	ADMUX = (0<<REFS1) | (1<<REFS0) | (0<<ADLAR) | (1<<MUX3) | (1<<MUX2) | (1<<MUX1) | (0<<MUX0);

	ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1);										// Enable ADC and set ADC prescaler
	for(int i = 0; i < BATTERY_NUM_MESS_ADC + BATTERY_DUMMY_NUM_MESS_ADC; i++) {
		ADCSRA |= (1 << ADSC);															// start conversion
		while (ADCSRA & (1 << ADSC)) {}													// wait for conversion complete

		if (i >= BATTERY_DUMMY_NUM_MESS_ADC) {											// we discard the first dummy measurements
			adcValue += ADCW;
		}
	}
	ADCSRA &= ~(1 << ADEN);																// ADC disable

	adcValue = adcValue / BATTERY_NUM_MESS_ADC;
	return (bandgapVoltage * 1023) / adcValue;											// calculate battery voltage in mV
}
