/**
 * SHT10, BMP085, TSL2561 Sensor class
 *
 * (c) 2014 Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
 * Author: Horst Bittner <horst@diebittners.de>
 *         Dirk Hoffmann dirk@fhem.forum
 */
#include "Sensor_SHT10_BMP085_TSL2561.h"
extern "C" {
	#include <Wire.h>
	#include <twi.h>
}

//- user code here --------------------------------------------------------------------------------------------------------
void SHT10_BMP085_TSL2561::config(uint8_t data, uint8_t sck, uint16_t timing, Sensirion *tPtr, BMP085 *pPtr, TSL2561 *lPtr) {
	tTiming = timing;
	nTime = millis() + 1000;													// set the first time we like to measure
	nAction = 'm';

	sht10 = tPtr;
	sht10->config(data,sck);													// configure the sensor
	sht10->writeSR(LOW_RES);													// low resolution is enough

	if (pPtr != NULL) {															// only if there is a valid module defined
		bm180 = pPtr;
	}

	if (lPtr != NULL) {															// only if there is a valid module defined
		tsl2561 = lPtr;
	}

	gain = 0;
	tsl2561->setPowerUp();
	tsl2561->setTiming(gain, INTEGATION_TIME_14);
}

void SHT10_BMP085_TSL2561::poll_measure(void) {
	nTime += measureTime;														// add the 500ms measurement takes to transmit in time
	nAction = 't';

	// Disable I2C
	TWCR = 0;

	// measurement code
	// http://www.sensirion.com/fileadmin/user_upload/customers/sensirion/Dokumente/Humidity/Sensirion_Humidity_SHT1x_Datasheet_V5.pdf

	uint16_t rawData;
	uint8_t shtError = sht10->measTemp(&rawData);
	// Measure temperature and humidity from Sensor only if no error
	if (!shtError) {
		if (rawData < 990) tTemp = (rawData*0.4f-396)*-1+0x4000;
		else tTemp = rawData*0.4f-396;

		sht10->measHumi(&rawData);
		tHum = -2.0468f + 0.5872f * rawData + -4.0845E-4f * rawData * rawData;
		tHum = ((float)tTemp/10 - 25) * (float)(0.01f + 0.00128f * rawData) + tHum;
		//	Serial << "raw: " << rawData << "  mH: " << tHum << '\n';
	}
	
	if (bm180 != NULL) {														// only if we have a valid module
		bm180->begin(0);
		tPres = (uint16_t)(bm180->readPressure() / 100);

		if (tPres > 300) {
			// get temperature from bmp180 if sht10 no present
			if (shtError) {
				// temp from BMP180
				tTemp = bm180->readTemperature() * 10;
			}
		} else {
			tPres = 0;
		}
	}

	unsigned int data0, data1;
	tLux = tsl2561->readBrightness(data0, data1) * 100;
	tLux = tsl2561->getError() ? 10000100 : tLux;								// send 100.001 Lux if no sensor available
}


void SHT10_BMP085_TSL2561::poll_transmit(void) {
	unsigned long mils = millis();

	if (tTiming) {
		nTime = mils + tTiming;													// there is a given timing
	}
	else {
		nTime = mils + (calcSendSlot() * 250) - measureTime;					// calculate the next send slot by multiplying with 250ms to get the time in millis
	}
	
	// hoffmann
//	nTime = mils + 5000;

	nAction = 'm';																// next time we want to measure again

	hm->sendPeerWEATHER(regCnl, tTemp, tHum, tPres, tLux);						// send out the weather event
}

uint32_t SHT10_BMP085_TSL2561::calcSendSlot(void) {
	uint32_t result = (((hm->getHMID() << 8) | hm->getMsgCnt()) * 1103515245 + 12345) >> 16;
	return (result & 0xFF) + 480;
}

//- mandatory functions for every new module to communicate within HM protocol stack --------------------------------------
void SHT10_BMP085_TSL2561::configCngEvent(void) {
	// it's only for information purpose while something in the channel config was changed (List0/1 or List3/4)
	#ifdef DM_DBG
	Serial << F("configCngEvent\n");
	#endif
}
void SHT10_BMP085_TSL2561::pairSetEvent(uint8_t *data, uint8_t len) {
	// we received a message from master to set a new value, typical you will find three bytes in data
	// 1st byte = value; 2nd byte = ramp time; 3rd byte = duration time;
	// after setting the new value we have to send an enhanced ACK (<- 0E E7 80 02 1F B7 4A 63 19 63 01 01 C8 00 54)
	#ifdef DM_DBG
	Serial << F("pairSetEvent, value:") << pHexB(data[0]);
	if (len > 1) Serial << F(", rampTime: ") << pHexB(data[1]);
	if (len > 2) Serial << F(", duraTime: ") << pHexB(data[2]);
	Serial << '\n';
	#endif
		
	hm->sendACKStatus(regCnl,modStat,0);
}
void SHT10_BMP085_TSL2561::pairStatusReq(void) {
	// we received a status request, appropriate answer is an InfoActuatorStatus message
	#ifdef DM_DBG
	Serial << F("pairStatusReq\n");
	#endif
	
	hm->sendInfoActuatorStatus(regCnl, modStat, 0);
}
void SHT10_BMP085_TSL2561::peerMsgEvent(uint8_t type, uint8_t *data, uint8_t len) {
	// we received a peer event, in type you will find the marker if it was a switch(3E), remote(40) or sensor(41) event
	// appropriate answer is an ACK
	#ifdef DM_DBG
	Serial << F("peerMsgEvent, type: ")  << pHexB(type) << F(", data: ")  << pHex(data,len) << '\n';
	#endif
	
	hm->send_ACK();
}

void SHT10_BMP085_TSL2561::poll(void) {
	unsigned long mils = millis();

	// just polling, as the function name said
	if ((nTime == 0) || (nTime > mils)) {										// check if it is time to jump in
		return;
	}

	if (nAction == 'm') {
		poll_measure();															// measure

	} else if (nAction == 't') {
		poll_transmit();														// transmit
	}
}

void SHT10_BMP085_TSL2561::setResetReason(byte _resetReason) {
	resetReason = _resetReason;
}

//- predefined, no reason to touch ----------------------------------------------------------------------------------------
void SHT10_BMP085_TSL2561::regInHM(uint8_t cnl, HM *instPtr) {
	hm = instPtr;																		// set pointer to the HM module
	hm->regCnlModule(cnl,s_mod_dlgt(this,&SHT10_BMP085_TSL2561::hmEventCol),(uint16_t*)&ptrMainList,(uint16_t*)&ptrPeerList);
	regCnl = cnl;																		// stores the channel we are responsible fore
}
void SHT10_BMP085_TSL2561::hmEventCol(uint8_t by3, uint8_t by10, uint8_t by11, uint8_t *data, uint8_t len) {
	if       (by3 == 0x00)                    poll();
	else if ((by3 == 0x01) && (by11 == 0x06)) configCngEvent();
	else if ((by3 == 0x11) && (by10 == 0x02)) pairSetEvent(data, len);
	else if ((by3 == 0x01) && (by11 == 0x0E)) pairStatusReq();
	else if ((by3 == 0x01) && (by11 == 0x01)) peerAddEvent(data, len);
	else if  (by3 >= 0x3E)                    peerMsgEvent(by3, data, len);
	else return;
}
void SHT10_BMP085_TSL2561::peerAddEvent(uint8_t *data, uint8_t len) {
	// we received an peer add event, which means, there was a peer added in this respective channel
	// 1st byte and 2nd byte shows the peer channel, 3rd and 4th byte gives the peer index
	// no need for sending an answer, but we could set default data to the respective list3/4
	#ifdef DM_DBG
	Serial << F("peerAddEvent: pCnl1: ") << pHexB(data[0]) << F(", pCnl2: ") << pHexB(data[1]) << F(", pIdx1: ") << pHexB(data[2]) << F(", pIdx2: ") << pHexB(data[3]) << '\n';
	#endif
	
	if ((data[0]) && (data[1])) {																		// dual peer add
		if (data[0]%2) {																				// odd
			hm->setListFromModule(regCnl,data[2],(uint8_t*)peerOdd,sizeof(peerOdd));
			hm->setListFromModule(regCnl,data[3],(uint8_t*)peerEven,sizeof(peerEven));
		} else {																						// even
			hm->setListFromModule(regCnl,data[2],(uint8_t*)peerEven,sizeof(peerEven));
			hm->setListFromModule(regCnl,data[3],(uint8_t*)peerOdd,sizeof(peerOdd));
		}
	} else {																							// single peer add
		if (data[0]) hm->setListFromModule(regCnl,data[2],(uint8_t*)peerSingle,sizeof(peerSingle));
		if (data[1]) hm->setListFromModule(regCnl,data[3],(uint8_t*)peerSingle,sizeof(peerSingle));
	}
	
}
