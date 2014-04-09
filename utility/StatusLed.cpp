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
	for (uint8_t i = 0; i < 2; i++) {

		unsigned long mils = millis();
		if ((nTime[i] > 0) && (nTime[i] <= mils )) {
			//	Serial << "LED:" <<i << ", mode:" << mode[i] << ", bCnt:" << bCnt[i];

			short toggle_nTime = 0;
			if (mode[i] == STATUSLED_MODE_OFF) {
				off(i);

			} else if (mode[i] == STATUSLED_MODE_ON) {
				on(i);

			} else if (mode[i] == STATUSLED_MODE_BLINKSLOW) {
				toggle_nTime = STATUSLED_BLINKRATE_SLOW;

			} else if (mode[i] == STATUSLED_MODE_BLINKFAST) {
				toggle_nTime = STATUSLED_BLINKRATE_FAST;

			} else if (mode[i] == STATUSLED_MODE_BLINKSFAST) {
				toggle_nTime = STATUSLED_BLINKRATE_SFAST;

			} else if (mode[i] == STATUSLED_MODE_HEARTBEAT) {
				if (bCnt[i] > 3) {
					bCnt[i] = 0;
				}
				toggle_nTime = heartBeat[bCnt[i]++];
			}

			if (mode[i] > STATUSLED_MODE_ON) {
				toggle(i);
				nTime[i] = millis() + toggle_nTime;

				if (bCnt[i] > 0) {
					bCnt[i]--;

					if (bCnt[i] == 0) {
						off(i);													// stop blinking next time
					}

					hmPtr->stayAwake(toggle_nTime + 100);
				}
			}
		}
	}
}

void StatusLed::set(uint8_t leds, uint8_t tMode, uint8_t blinkCount) {
	unsigned long mils = millis();

	blinkCount = blinkCount * 2;

	if (leds & 0b01){
		mode[0] = tMode;
		bCnt[0] = blinkCount;
		nTime[0] = mils;
	}

	if (leds & 0b10){
		mode[1] = tMode;
		bCnt[1] = blinkCount;
		nTime[1] = mils;
	}
}

void StatusLed::stop(uint8_t leds) {
	if (leds & 0b01) off(0);
	if (leds & 0b10) off(1);

	for (uint8_t i = 0; i < 2; i++){
		mode[i] = STATUSLED_MODE_OFF;
		bCnt[i] = 0;
		state[i] = 0;
	}
}

void StatusLed::on(uint8_t ledNum) {
	onOff(STATUSLED_MODE_ON, ledNum);											// switch led on
}

void StatusLed::off(uint8_t ledNum) {
	onOff(STATUSLED_MODE_OFF, ledNum);											// switch led off
}

void StatusLed::onOff(uint8_t mode, uint8_t ledNum) {
	digitalWrite(pin[ledNum], mode);
	state[ledNum] = mode;
	nTime[ledNum] = 0;
}

void StatusLed::toggle(uint8_t ledNum) {
	if (state[ledNum]) off(ledNum);
	else on(ledNum);
}
