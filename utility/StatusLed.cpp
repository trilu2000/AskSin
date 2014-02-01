//- -----------------------------------------------------------------------------------------------------------------------
// AskSin driver implementation
// 2013-08-03 <horst@diebittners.de> Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
//- Status led driver -----------------------------------------------------------------------------------------------------
//- with a lot of contribution from Dirk at FHEM forum
//- -----------------------------------------------------------------------------------------------------------------------

#include "StatusLed.h"

#include <AskSinMain.h>																	// declaration for getting access to stay awake function
HM *hmPtr;


void StatusLed::config(uint8_t pin1, uint8_t pin2) {

	pin[0] = pin1;
	pinMode(pin1, OUTPUT);																// setting the pin1 to output mode
	off(0);

	pin[1] = pin2;
	pinMode(pin2, OUTPUT);																// setting the pin2 to output mode
	off(1);
}

void StatusLed::setHandle(void *ptr) {
	hmPtr = (HM*)ptr;
}

void StatusLed::poll() {
	for (uint8_t i = 0; i < 2; i++){

		if ((nTime[i] > 0) && (nTime[i] <= millis())) {
			//			Serial << "LED:" <<i << ", mode:" << mode[i] << ", bCnt:" << bCnt[i];

			if (mode[i] == STATUSLED_MODE_OFF) {
				off(i);

			} else if (mode[i] == STATUSLED_MODE_ON) {
				on(i);

			} else if (mode[i] == STATUSLED_MODE_BLINKSLOW) {
				toggle(i);
				nTime[i] = millis() + STATUSLED_BLINKRATE_SLOW;

			} else if (mode[i] == STATUSLED_MODE_BLINKFAST) {
				toggle(i);
				nTime[i] = millis() + STATUSLED_BLINKRATE_FAST;

			} else if (mode[i] == STATUSLED_MODE_BLINKSFAST) {
				toggle(i);
				nTime[i] = millis() + STATUSLED_BLINKRATE_SFAST;

			} else if (mode[i] == STATUSLED_MODE_HEARTBEAT) {
				if (bCnt[i] > 3) bCnt[i] = 0;
				toggle(i);
				nTime[i] = millis() + heartBeat[bCnt[i]++];
			}

			if ((mode[i] == STATUSLED_MODE_BLINKSLOW || mode[i] == STATUSLED_MODE_BLINKFAST || mode[i] == STATUSLED_MODE_BLINKSFAST) && bCnt[i] > 0) {
				bCnt[i]--;

				if (bCnt[i] == 0) {
						off(i);															// stop blinking next time
				}

				// while blink counting don't go sleep
				uint32_t xMillis = STATUSLED_BLINKRATE_SLOW;
				if (mode[i] == STATUSLED_MODE_BLINKFAST) xMillis = STATUSLED_BLINKRATE_FAST;
				if (mode[i] == STATUSLED_MODE_BLINKSFAST) xMillis = STATUSLED_BLINKRATE_SFAST;
				
				hmPtr->stayAwake(xMillis + 100);
			}

		}
	}
}

void StatusLed::set(uint8_t leds, uint8_t tMode, uint8_t blinkCount) {
	blinkCount = blinkCount * 2;

	if (leds & 0b01){
		mode[0] = tMode;
		bCnt[0] = blinkCount;
		nTime[0] = millis();
	}

	if (leds & 0b10){
		mode[1] = tMode;
		bCnt[1] = blinkCount;
		nTime[1] = millis();
	}
}

void StatusLed::stop(uint8_t leds) {
	if (leds & 0b01) off(0);
	if (leds & 0b01) off(1);

	for (uint8_t i = 0; i < 2; i++){
		mode[i] = STATUSLED_MODE_OFF;
		bCnt[i] = 0;
		state[i] = 0;
	}
}

void StatusLed::on(uint8_t ledNum) {
	digitalWrite(pin[ledNum], 1);														// switch led on
	state[ledNum] = STATUSLED_MODE_ON;
	nTime[ledNum] = 0;
}

void StatusLed::off(uint8_t ledNum) {
	digitalWrite(pin[ledNum], 0);														// switch led off
	state[ledNum] = STATUSLED_MODE_OFF;
	nTime[ledNum] = 0;
}

void StatusLed::toggle(uint8_t ledNum) {
	if (state[ledNum]) off(ledNum);
	else on(ledNum);
}
