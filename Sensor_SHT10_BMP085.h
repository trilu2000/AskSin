//- -----------------------------------------------------------------------------------------------------------------------
// AskSin driver implementation
// 2013-08-03 <horst@diebittners.de> Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
//- SHT10 and BMP085 Sensor class -----------------------------------------------------------------------------------------
//- -----------------------------------------------------------------------------------------------------------------------
#ifndef SENSOR_SHT10_BMP085_H
#define SENSOR_SHT10_BMP085_H

#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif

#include "AskSinMain.h"
#include "utility/Serial.h"
#include "utility/Fastdelegate.h"

#include <Wire.h>
#include <../Sensirion/Sensirion.h>
#include <../BMP085/BMP085.h>

#define MyClassName Sensor_SHT10_BMP085													// module name as macro to overcome the problem of renaming functions all the time
//#define DM_DBG																			// debug message flag

const uint8_t peerOdd[] =    {};														// default settings for list3 or list4
const uint8_t peerEven[] =   {};
const uint8_t peerSingle[] = {};

class MyClassName {
  //- user code here ------------------------------------------------------------------------------------------------------
  public://----------------------------------------------------------------------------------------------------------------
	void config(uint8_t data, uint8_t sck, uint16_t timing, Sensirion *tPtr, BMP085 *pPtr, uint16_t Altidude);

	
  protected://-------------------------------------------------------------------------------------------------------------
  private://---------------------------------------------------------------------------------------------------------------
	#define  measureTime   1000
	
	Sensirion *sh;
	BMP085 *bm;
	
	uint8_t  nAction;
	uint32_t nTime;
	uint16_t tTiming;
	uint16_t tAlt;
	
	uint16_t tTemp;
	uint8_t  tHum;
	uint16_t tPres;
	
	uint32_t calcSendSlot(void);
	void     poll_measure(void);
	void     poll_transmit(void);
  
  //- mandatory functions for every new module to communicate within HM protocol stack ------------------------------------ 
  public://----------------------------------------------------------------------------------------------------------------
	uint8_t  modStat;																	// module status byte, needed for list3 modules to answer status requests
	uint8_t  regCnl;																	// holds the channel for the module

	HM      *hm;																		// pointer to HM class instance
	uint8_t *ptrPeerList;																// pointer to list3/4 in regs struct
	uint8_t *ptrMainList;																// pointer to list0/1 in regs struct

	void    configCngEvent(void);														// list1 on registered channel had changed
	void    pairSetEvent(uint8_t *data, uint8_t len);									// pair message to specific channel, handover information for value, ramp time and so on
	void    pairStatusReq(void);														// event on status request
	void    peerMsgEvent(uint8_t type, uint8_t *data, uint8_t len);						// peer message was received on the registered channel, handover the message bytes and length

	void    poll(void);																	// poll function, driven by HM loop

	//- predefined, no reason to touch ------------------------------------------------------------------------------------
	void    regInHM(uint8_t cnl, HM *instPtr);											// register this module in HM on the specific channel
	void    hmEventCol(uint8_t by3, uint8_t by10, uint8_t by11, uint8_t *data, uint8_t len); // call back address for HM for informing on events
	void    peerAddEvent(uint8_t *data, uint8_t len);									// peer was added to the specific channel, 1st and 2nd byte shows peer channel, third and fourth byte shows peer index
};


#endif
