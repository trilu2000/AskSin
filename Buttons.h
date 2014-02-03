//- -----------------------------------------------------------------------------------------------------------------------
// AskSin driver implementation
// 2013-08-03 <horst@diebittners.de> Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
//- Remote Button ---------------------------------------------------------------------------------------------------------
//- with a lot of contribution from Dirk at FHEM forum
//- -----------------------------------------------------------------------------------------------------------------------
#ifndef BUTTONS_H
#define BUTTONS_H

#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif

#include "AskSinMain.h"
#include "utility/Serial.h"
#include "utility/Fastdelegate.h"
#include "utility/PinChangeIntHandler.h"

#define MyClassName Buttons																// module name as macro to overcome the problem of renaming functions all the time
//#define DM_DBG																		// debug message flag


class MyClassName {
  //- user code here ------------------------------------------------------------------------------------------------------
  public://----------------------------------------------------------------------------------------------------------------
	void     config(uint8_t Pin, void tCallBack(uint8_t, uint8_t));
	void     interrupt(uint8_t c);

  private://---------------------------------------------------------------------------------------------------------------
	struct s_regChanL1 {
		// 0x04,0x08,0x09,
		uint8_t                      :4;     //       l:0, s:4
		uint8_t  longPress           :4;     // 0x04, s:4, e:8
		uint8_t  sign                :1;     // 0x08, s:0, e:1
		uint8_t                      :7;     //       l:7, s:0
		uint8_t  dblPress            :4;     // 0x09, s:0, e:4
		uint8_t                      :4;     //
	} *list1;																			// pointer to list1 values

	uint16_t toShDbl;																	// minimum time to be recognized as a short key press
	uint16_t lngKeyTme;																	// time key should be pressed to be recognized as a long key press
	uint16_t toLoDbl;																	// maximum time between a double key press
	void (*callBack)(uint8_t, uint8_t);													// call back address for key display

	uint8_t  cFlag :1;																	// was there a change happened in key state
	uint8_t  cStat :1;																	// current status of the button

	uint8_t  lStat :1;																	// last key state
	uint8_t  dblSh :1;																	// remember last short key press to indicate a double key press
	uint8_t  dblLo :1;																	// remember last long key press to indicate a double key press
	uint8_t  rptLo :1;																	// remember long key press to indicate repeated long, or when long was released

	uint32_t cTime;																		// stores the next time to check, otherwise we would check every cycle
	uint32_t kTime;																		// stores time when the key was pressed or released

	void     buttonAction(uint8_t bEvent);												// function to call from inside of the poll function

  //- mandatory functions for every new module to communicate within HM protocol stack ------------------------------------ 
  public://----------------------------------------------------------------------------------------------------------------
	uint8_t  modStat;																	// module status byte, needed for list3 modules to answer status requests
	uint8_t  regCnl;																	// holds the channel for the module

	HM       *hm;																		// pointer to HM class instance
	uint8_t  *ptrPeerList;																// pointer to list3/4 in regs struct
	uint8_t  *ptrMainList;																// pointer to list0/1 in regs struct

	void     configCngEvent(void);														// list1 on registered channel had changed
	void     pairSetEvent(uint8_t *data, uint8_t len);									// pair message to specific channel, handover information for value, ramp time and so on
	void     pairStatusReq(void);														// event on status request
	void     peerMsgEvent(uint8_t type, uint8_t *data, uint8_t len);					// peer message was received on the registered channel, handover the message bytes and length

	void     poll(void);																// poll function, driven by HM loop

	//- predefined, no reason to touch ------------------------------------------------------------------------------------
	void    regInHM(uint8_t cnl, HM *instPtr);											// register this module in HM on the specific channel
	void    hmEventCol(uint8_t by3, uint8_t by10, uint8_t by11, uint8_t *data, uint8_t len); // call back address for HM for informing on events
	void    peerAddEvent(uint8_t *data, uint8_t len);									// peer was added to the specific channel, 1st and 2nd byte shows peer channel, third and fourth byte shows peer index
};


#endif
