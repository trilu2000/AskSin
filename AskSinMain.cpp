//- -----------------------------------------------------------------------------------------------------------------------
// AskSin driver implementation
// 2013-08-03 <horst@diebittners.de> Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
//- AskSin protocol functions ---------------------------------------------------------------------------------------------
//- with a lot of support from martin876 at FHEM forum
//- -----------------------------------------------------------------------------------------------------------------------
#include "AskSinMain.h"

//- some macros and definitions -------------------------------------------------------------------------------------------
#define _pgmB pgm_read_byte																// short hand for PROGMEM read
#define _pgmW pgm_read_word
#define maxPayload 16																	// defines the length of send_conf_poll and send_peer_poll
//- -----------------------------------------------------------------------------------------------------------------------

//- -----------------------------------------------------------------------------------------------------------------------
//- Homematic communication functions -------------------------------------------------------------------------------------
//- -----------------------------------------------------------------------------------------------------------------------
s_intGDO0 intGDO0;																		// structure to store interrupt number and pointer to active AskSin class instance
uint8_t bCast[] = {0,0,0,0};															// broad cast address

//  public://--------------------------------------------------------------------------------------------------------------
//- homematic public protocol functions
void     HM::init(void) {
	// register handling setup
	prepEEprom();																		// check the eeprom for first time boot, prepares the eeprom and loads the defaults
	loadRegs();

	dParm.cnlCnt = (uint8_t*)malloc(dDef.cnlNbr+1);										// message counter sizing
	memset(dParm.cnlCnt,0,dDef.cnlNbr+1);												// clear counter

	// size the array for module registering
	modTbl = (s_modtable*)malloc((dDef.cnlNbr+1)*sizeof(s_modtable));					// size the table object to the amount of channels
	memset(modTbl,0,(dDef.cnlNbr+1)*sizeof(s_modtable));								// clear structure
	
	// communication setup
	cc.init();																			// init the TRX868 module
	intGDO0.nbr = cc.gdo0Int;															// make the gdo0 interrupt public
	intGDO0.ptr = this;																	// make the address of this instance public
	attachInterrupt(intGDO0.nbr,isrGDO0,FALLING);										// attach the interrupt

	statusLed.setHandle(this);															// make the main class visible for status led
	hm.stayAwake(1000);
}
void     HM::recvInterrupt(void) {
	if (hm.cc.receiveData(hm.recv.data)) {												// is something in the receive string
		hm.hm_dec(hm.recv.data);														// decode the content
	}

}
void     HM::poll(void) {																// task scheduler
	if (recv.data[0] > 0) recv_poll();													// trace the received string and decide what to do further
	if (send.counter > 0) send_poll();													// something to send in the buffer?
	if (conf.act > 0) send_conf_poll();													// some config to be send out
	if (pevt.act > 0) send_peer_poll();													// send peer events
	power_poll();
	module_poll();																		// poll the registered channel modules
	statusLed.poll();																	// poll the status leds
	battery.poll();																		// poll the battery check
}

void     HM::send_out(void) {
	if (bitRead(send.data[2],5)) send.retries = dParm.maxRetr;							// check for ACK request and set max retries counter
	else send.retries = 1;																// otherwise send only one time

	send.burst = bitRead(send.data[2],4);												// burst necessary?

	if (memcmp(&send.data[7], dParm.HMID, 3) == 0) {									// if the message is addressed to us,
		memcpy(recv.data,send.data,send.data[0]+1);										// then copy in receive buffer. could be the case while sending from serial console
		send.counter = 0;																// no need to fire
	} else {																			// it's not for us, so encode and put in send queue

		send.counter = 1;																// and fire
		send.timer = 0;
	}
}
void     HM::reset(void) {
	setEeWo(ee[0].magicNr,0);															// clear magic byte in eeprom and step in initRegisters
	prepEEprom();																		// check the eeprom for first time boot, prepares the eeprom and loads the defaults
	loadRegs();

	statusLed.set(STATUSLED_2, STATUSLED_MODE_BLINKSFAST, 5);							// blink LED2 5 times short
}
void     HM::setPowerMode(uint8_t mode) {
	// there are 3 power modes for the TRX868 module
	// TX mode will switched on while something is in the send queue
	// 0 - RX mode enabled by default, take approx 17ma
	// 1 - RX is in burst mode, RX will be switched on every 250ms to check if there is a carrier signal
	//     if yes - RX will stay enabled until timeout is reached, prolongation of timeout via receive function seems not necessary
	//				to be able to receive an ACK, RX mode should be switched on by send function
	//     if no  - RX will go in idle mode and wait for the next carrier sense check
	// 2 - RX is off by default, TX mode is enabled while sending something
	//     configuration mode is required in this setup to be able to receive at least pairing and config request strings
	//     should be realized by a 30 sec timeout function for RX mode
	// as output we need a status indication if TRX868 module is in receive, send or idle mode
	// idle mode is then the indication for power down mode of AVR

	if        (mode == 1) {																// no power savings, RX is in receiving mode
		set_sleep_mode(SLEEP_MODE_IDLE);												// normal power saving

	} else if (mode == 2) {																// some power savings, RX is in burst mode
		powr.parTO = 15000;																// pairing timeout
		powr.minTO = 2000;																// stay awake for 2 seconds after sending
		powr.nxtTO = millis() + 250;													// check in 250ms for a burst signal

		MCUSR &= ~(1<<WDRF);															// clear the reset flag
		WDTCSR |= (1<<WDCE) | (1<<WDE);													// set control register to change enabled and enable the watch dog
		WDTCSR = 1<<WDP2;																// 250 ms
		powr.wdTme = 256;																// store the watch dog time for adding in the poll function
		set_sleep_mode(SLEEP_MODE_PWR_DOWN);											// max power saving

	} else if (mode == 3) {																// most power savings, RX is off beside a special function where RX stay in receive for 30 sec
		MCUSR &= ~(1<<WDRF);															// clear the reset flag
		WDTCSR |= (1<<WDCE) | (1<<WDE);													// set control register to change enabled and enable the watch dog
		//WDTCSR = 1<<WDP2;																// 250 ms
		//WDTCSR = 1<<WDP1 | 1<<WDP2;													// 1000 ms
		//WDTCSR = 1<<WDP0 | 1<<WDP1 | 1<<WDP2;											// 2000 ms
		WDTCSR = 1<<WDP0 | 1<<WDP3;														// 8000 ms
		powr.wdTme = 8190;																// store the watch dog time for adding in the poll function
	}
	
	if ((mode == 3) || (mode == 4))	{													// most power savings, RX is off beside a special function where RX stay in receive for 30 sec
		powr.parTO = 15000;																// pairing timeout
		powr.minTO = 1000;																// stay awake for 1 seconds after sending
		powr.nxtTO = millis() + 4000;													// stay 4 seconds awake to finish boot time
		set_sleep_mode(SLEEP_MODE_PWR_DOWN);											// max power saving
	}

	powr.mode = mode;																	// set power mode
	powr.state = 1;																		// after init of the TRX module it is in RX mode
	//Serial << "pwr.mode:" << powr.mode << '\n';
}
void     HM::stayAwake(uint32_t xMillis) {
	if (powr.state == 0) cc.detectBurst();												// if TRX is in sleep, switch it on
	powr.state = 1;																		// remember TRX state
	if (powr.nxtTO > (millis() + xMillis)) return;										// set the new timeout only if necessary
	powr.nxtTO = millis() + xMillis;													// stay awake for some time by setting next check time
}

//- some functions for checking the config, preparing eeprom and load defaults to eeprom or in regs structure
void     HM::printConfig(void) {
	// print content of devDef incl slice string table
	Serial << F("\ncontent of dDef for ") << dDef.lstNbr << F(" elements\n");
	for (uint8_t i = 0; i < dDef.lstNbr; i++) {
		const s_cnlDefType *t = &dDef.chPtr[i];											// pointer for better handling

		Serial << F("cnl:") << _pgmB(&t->cnl)  << F(", lst:") << _pgmB(&t->lst) << F(", pMax:") << _pgmB(&t->pMax) << '\n';
		Serial << F("idx:") << _pgmB(&t->sIdx) << F(", len:") << _pgmB(&t->sLen) << F(", addr:") << _pgmB(&t->pAddr) << '\n';
		
		Serial << F("regs: ") << pHex(dDef.slPtr + _pgmB(&t->sIdx), _pgmB(&t->sLen)) << '\n';
		Serial << F("data: ") << pHexEE(_pgmB(&t->pAddr)+ee[0].regsDB,_pgmB(&t->sLen)*(_pgmB(&t->pMax)?_pgmB(&t->pMax):1)) << '\n';

		if (!_pgmB(&t->pMax)) {
			Serial << '\n';
			continue;
		}
		
		for (uint8_t j = 0; j < _pgmB(&t->pMax); j++) {
			Serial << pHexEE(_pgmB(&t->pPeer)+ee[0].peerDB+(j*4),4) << F("  ");
		}
		Serial << F("\n\n");
	}
	Serial << '\n';

}
void     HM::prepEEprom(void) {
	uint16_t crc = 0;																	// define variable for storing crc
	uint8_t  *p = (uint8_t*)dDef.chPtr;													// cast devDef to char
	
	for (uint8_t i = 0; i < dDef.lstNbr; i++) {											// step through all lines of chDefType table
		for (uint8_t j = 0; j < 9; j++) {												// step through the first 9 bytes
			crc = crc16(crc, _pgmB(&p[(i*11)+j]));										// calculate the 16bit checksum for the table
		}
	}
	
	#ifdef RPDB_DBG																		// some debug message
	Serial << F("prepEEprom t_crc: ") << crc << F(", e_crc: ") << getEeWo(ee[0].magicNr) << '\n';
	#endif

	if (crc != getEeWo(ee[0].magicNr)) {												// compare against eeprom's magic number
		Serial << "prep eeprom\n";
		#ifdef RPDB_DBG
		Serial << F("format eeprom for:\n");
		Serial << F("peerDB, addr:") << ee[0].peerDB << F(", len:") << ee[1].peerDB << '\n';
		Serial << F("regsDB, addr:") << ee[0].regsDB << F(", len:") << ee[1].regsDB << '\n';
		#endif
		
		clrEeBl(ee[0].peerDB, ee[1].peerDB);											// format the eeprom area
		delay(50);																		// give eeprom some time
		clrEeBl(ee[0].regsDB, ee[1].regsDB);											// format the eeprom area
		delay(50);																		// give eeprom some time
		
		loadDefaults();																	// do we have some default settings
		delay(50);																		// give eeprom some time
	}
	setEeWo(ee[0].magicNr,crc);															// write magic number to it's position
}
void     HM::loadDefaults(void) {
	if (dtRegs.nbr) {
		#ifdef RPDB_DBG
		Serial << F("set defaults:\n");
		#endif
		
		for (uint8_t i = 0; i < dtRegs.nbr; i++) {										// step through the table
			s_defaultRegsTbl *t = &dtRegs.ptr[i];										// pointer for better handling
			
			// check if we search for regs or peer
			uint16_t eeAddr = 0;
			if (t->prIn == 0) {															// we are going for peer
				eeAddr = cnlDefPeerAddr(t->cnl ,t->pIdx);								// get the eeprom address
			} else if (t->prIn == 1) {													// we are going for regs
				eeAddr = cnlDefTypeAddr(t->cnl, t->lst ,t->pIdx);						// get the eeprom address
			}
			
			// find the respective address and write to eeprom
			for (uint8_t j = 0; j < t->len; j++) {										// step through the bytes
				setEeBy(eeAddr++,_pgmB(&t->b[j]));										// read from PROGMEM and write to eeprom
			}
			
			#ifdef RPDB_DBG																// some debug message
			Serial << F("cnl:") << t->cnl << F(", lst:") << t->lst << F(", idx:") << t->pIdx << ", addr:" << eeAddr << ", data: " << pHexPGM(t->b, t->len) << '\n';
			#endif
		}
	}
}
void     HM::loadRegs(void) {
	// default regs filled regarding the cnlDefTable
	for (uint8_t i = 0; i < dDef.lstNbr; i++) {											// step through the list
		const s_cnlDefType *t = &dDef.chPtr[i];											// pointer for easier handling
		
		getEeBl(ee[0].regsDB + _pgmW(&t->pAddr), _pgmB(&t->sLen), (void*)_pgmW(&t->pRegs)); // load content from eeprom to user structure
	}

	// fill the master id
	uint8_t ret = getRegAddr(0,0,0,0x0a,3,dParm.MAID);									// get regs for 0x0a
	//Serial << "MAID:" << pHex(dParm.MAID,3) << '\n'; 	
}
void     HM::regCnlModule(uint8_t cnl, s_mod_dlgt Delegate, uint16_t *mainList, uint16_t *peerList) {
	modTbl[cnl].mDlgt = Delegate;
	modTbl[cnl].use = 1;
	
	// register the call back address for the list pointers
	for (uint8_t i = 0; i < dDef.lstNbr; i++) {											// step through the list
		const s_cnlDefType *t = &dDef.chPtr[i];											// pointer for easier handling
		if (_pgmB(&t->cnl) != cnl) continue;

		if (_pgmB(&t->lst) >= 3) *peerList = _pgmW(&t->pRegs);
		if (_pgmB(&t->lst) <= 1) *mainList = _pgmW(&t->pRegs);
	}

}
uint32_t HM::getHMID(void) {
	return (uint32_t)dParm.HMID[0] << 16 | (uint16_t)dParm.HMID[1] << 8 | (uint8_t)dParm.HMID[2];
}
uint8_t  HM::getMsgCnt(void) {
	return send.mCnt - 1;
}

//- external functions for pairing and communicating with the module
void     HM::startPairing(void) {														// send a pairing request to master
	//                               01 02    03                            04 05 06 07
	// 1A 00 A2 00 3F A6 5C 00 00 00 10 80 02 50 53 30 30 30 30 30 30 30 31 9F 04 01 01
	statusLed.set(STATUSLED_2, STATUSLED_MODE_BLINKFAST);								// led blink in config mode

	//if (powr.mode > 1) stayAwake(powr.parTO);											// stay awake for the next 30 seconds
	memcpy_P(send_payLoad, dParm.p, 17);												// copy details out of register.h
	send_prep(send.mCnt++,0xA2,0x00,dParm.MAID ,send_payLoad,17);
}
void     HM::sendInfoActuatorStatus(uint8_t cnl, uint8_t status, uint8_t flag) {
	if (memcmp(dParm.MAID,bCast,3) == 0) return;										// not paired, nothing to send

	//	"10;p01=06"   => { txt => "INFO_ACTUATOR_STATUS", params => {
	//		CHANNEL => "2,2",
	//		STATUS  => '4,2',
	//		UNKNOWN => "6,2",
	//		RSSI    => '08,02,$val=(-1)*(hex($val))' } },
	send_payLoad[0] = 0x06;																// INFO_ACTUATOR_STATUS
	send_payLoad[1] = cnl;																// channel
	send_payLoad[2] = status;															// status
	send_payLoad[3] = flag;																// unknown
	send_payLoad[4] = cc.rssi;															// RSSI
	
	// if it is an answer to a CONFIG_STATUS_REQUEST we have to use the same message id as the request
	uint8_t tCnt;
	if ((recv.data[3] == 0x01) && (recv.data[11] == 0x0E)) tCnt = recv_rCnt;
	else tCnt = send.mCnt++;
	send_prep(tCnt,0xA4,0x10,dParm.MAID,send_payLoad,5);								// prepare the message
}
void     HM::sendACKStatus(uint8_t cnl, uint8_t status, uint8_t douolo) {
	//if (memcmp(regDev.pairCentral,broadCast,3) == 0) return;							// not paired, nothing to send

	//	"02;p01=01"   => { txt => "ACK_STATUS",  params => {
	//		CHANNEL        => "02,2",
	//		STATUS         => "04,2",
	//		DOWN           => '06,02,$val=(hex($val)&0x20)?1:0',
	//		UP             => '06,02,$val=(hex($val)&0x10)?1:0',
	//		LOWBAT         => '06,02,$val=(hex($val)&0x80)?1:0',
	//		RSSI           => '08,02,$val=(-1)*(hex($val))', }},
	send_payLoad[0] = 0x01;																// ACK Status
	send_payLoad[1] = cnl;																// channel
	send_payLoad[2] = status;															// status
	send_payLoad[3] = douolo | (battery.state << 7);									// down, up, low battery
	send_payLoad[4] = cc.rssi;															// RSSI
	
	// l> 0E EA 80 02 1F B7 4A 63 19 63 01 01 C8 00 4B
	//send_prep(recv_rCnt,0x80,0x02,regDev.pairCentral,send_payLoad,5);	// prepare the message
	send_prep(recv_rCnt,0x80,0x02,recv_reID,send_payLoad,5);							// prepare the message
}
void     HM::sendPeerREMOTE(uint8_t cnl, uint8_t longPress, uint8_t lowBat) {
	// no data needed, because it is a (40)REMOTE EVENT
	// "40"          => { txt => "REMOTE"      , params => {
	//	   BUTTON   => '00,2,$val=(hex($val)&0x3F)',
	//	   LONG     => '00,2,$val=(hex($val)&0x40)?1:0',
	//	   LOWBAT   => '00,2,$val=(hex($val)&0x80)?1:0',
	//	   COUNTER  => "02,2", } },

	//              type  source     target     msg   cnt
	// l> 0B 0A B4  40    1F A6 5C   1F B7 4A   01    01 (l:12)(160188)
	// l> 0B 0B B4 40 1F A6 5C 1F B7 4A 41 02 (l:12)(169121)

	if (cnl >  dDef.cnlNbr) return;														// channel out of range, do nothing

	//pevt.cdpIdx = cnlDefPeerIdx(cnl);													// get the index number in cnlDefType
	//if (pevt.cdpIdx == 0xff) return;
	pevt.t = cnlDefbyPeer(cnl);															// get the index number in cnlDefType
	if (pevt.t == NULL) return;
	pevt.msgCnt = dParm.cnlCnt[cnl];

	if (longPress == 1) pevt.reqACK = 0;												// repeated messages do not need an ACK
	else pevt.reqACK = 0x20;

	pevt.type = 0x40;																	// we want to send an remote event

	if (battery.state) lowBat = 1;
	pevt.data[0] = cnl | ((longPress)?1:0) << 6 | lowBat << 7;							// construct message

	pevt.data[1] = dParm.cnlCnt[cnl]++;													// increase event counter, important for switch event
	pevt.len = 2;																		// 2 bytes payload

	pevt.act = 1;																		// active, 1 = yes, 0 = no
	//Serial << "remote; cdpIdx:" << pevt.cdpIdx << ", type:" << pHexB(pevt.type) << ", rACK:" << pHexB(pevt.reqACK) << ", msgCnt:" << pevt.msgCnt << ", data:" << pHex(pevt.data,pevt.len) << '\n';
}
void     HM::sendPeerWEATHER(uint8_t cnl, uint16_t temp, uint8_t hum) {
	// "70"          => { txt => "WeatherEvent", params => {
	//	TEMP     => '00,4,$val=((hex($val)&0x3FFF)/10)*((hex($val)&0x4000)?-1:1)',
	//	HUM      => '04,2,$val=(hex($val))', } },

	//              type  source     target     temp    hum
	// l> 0C 0A B4  70    1F A6 5C   1F B7 4A   01 01   01 (l:13)(160188)
	// l> 0B 0B B4  40    1F A6 5C   1F B7 4A 41 02 (l:12)(169121)

	if (cnl >  dDef.cnlNbr) return;														// channel out of range, do nothing

	pevt.t = cnlDefbyPeer(cnl);															// get the index number in cnlDefType
	if (pevt.t == NULL) return;
	//Serial << "se\n";
	
	pevt.type = 0x70;																	// we want to send a weather event
	pevt.reqACK = 0x20;																	// we like to get an ACK

	pevt.data[0] = (temp >> 8) & 0xFF;
	pevt.data[1] = temp & 0xFF;
	pevt.data[2] = hum;
	pevt.len = 3;																		// 2 bytes payload
	pevt.msgCnt++;
	
	pevt.act = 1;																		// active, 1 = yes, 0 = no
	//Serial << "remote; cdpIdx:" << pevt.cdpIdx << ", type:" << pHexB(pevt.type) << ", rACK:" << pHexB(pevt.reqACK) << ", msgCnt:" << pevt.msgCnt << ", data:" << pHex(pevt.data,pevt.len) << '\n';
}
void     HM::sendPeerRAW(uint8_t cnl, uint8_t type, uint8_t *data, uint8_t len) {
	// validate the input, and fill the respective variables in the struct
	// message handling is taken from send_peer_poll
	if (cnl >  dDef.cnlNbr) return;														// channel out of range, do nothing
//	if (pevt.act) return;																// already sending an event, leave
//	if (doesListExist(cnl,4) == 0) {													// check if a list4 exist, otherwise leave
		//Serial << "sendPeerREMOTE failed\n";
//		return;
//	}
	
	// set variables in struct and make send_peer_poll active
//	pevt.cnl = cnl;																		// peer database channel
//	pevt.type = type;																	// message type
//	pevt.mFlg = 0xA2;
	
//	if (len > 0) {																		// copy data if there are some
//		memcpy(pevt.data, data, len);													// data to send
//		pevt.len = len;																	// len of data to send
//	}
//	pevt.act = 1;																		// active, 1 = yes, 0 = no
	//statusLed.setset(STATUSLED_BOTH, STATUSLED_MODE_BLINKSFAST, 1);						// blink led1 one time
}
void     HM::send_ACK(void) {
	uint8_t payLoad[] = {0x00};	 														// ACK
	send_prep(recv_rCnt,0x80,0x02,recv_reID,payLoad,1);
}
void     HM::send_NACK(void) {
	uint8_t payLoad[] = {0x80};															// NACK
	send_prep(recv_rCnt,0x80,0x02,recv_reID,payLoad,1);
}

//  protected://-----------------------------------------------------------------------------------------------------------
//- hm communication functions
void     HM::recv_poll(void) {															// handles the receive objects
	// do some checkups
	if (memcmp(&recv.data[7], dParm.HMID, 3) == 0) recv.forUs = 1;						// for us
	else recv.forUs = 0;
	if (memcmp(&recv.data[7], bCast, 3) == 0)      recv.bCast = 1;						// or a broadcast
	else recv.bCast = 0;																// otherwise only a log message
	
	// show debug message
	#ifdef AS_DBG																		// some debug message
	if(recv.forUs) Serial << F("-> ");
	else if(recv.bCast) Serial << F("b> ");
	else Serial << F("l> ");
	Serial << pHexL(recv.data, recv.data[0]+1) << pTime();
	exMsg(recv.data);																	// explain message
	#endif

	// if it's not for us or a broadcast message we could skip
	if ((recv.forUs == 0) && (recv.bCast == 0)) {
		recv.data[0] = 0;																// clear receive string
		return;
	}

	// is the message from a valid sender (pair or peer), if not then exit - takes ~2ms
	if ((isPairKnown(recv_reID) == 0) && (isPeerKnown(recv_reID) == 0)) {				// check against peers
		#if defined(AS_DBG)																// some debug message
		Serial << "pair/peer did not fit, exit\n";
		#endif
		recv.data[0] = 0;																// clear receive string
		return;
	}

	// check if it was a repeated message, delete while already received
	if (recv_isRpt) {																	// check repeated flag
		#if defined(AS_DBG)																// some debug message
		Serial << F("   repeated message; ");
		if (recv.mCnt == recv_rCnt) Serial << F("already received - skip\n");
		else Serial << F("not received before...\n");
		#endif
		
		if (recv.mCnt == recv_rCnt) {													// already received
			recv.data[0] = 0;															// therefore ignore
			return;																		// and skip
		}
	}

	// if the message comes from pair, we should remember the message id
	if (isPairKnown(recv_reID)) {
		recv.mCnt = recv_rCnt;
	}

	// configuration message handling
	if ((recv.forUs) && (recv_isMsg) && (recv_msgTp == 0x01)) {							// message is a config message
		if      (recv_by11 == 0x01) recv_ConfigPeerAdd();								// 01, 01
		else if (recv_by11 == 0x02) recv_ConfigPeerRemove();							// 01, 02
		else if (recv_by11 == 0x03) recv_ConfigPeerListReq();							// 01, 03
		else if (recv_by11 == 0x04) recv_ConfigParamReq();								// 01, 04
		else if (recv_by11 == 0x05) recv_ConfigStart();									// 01, 05
		else if (recv_by11 == 0x06) recv_ConfigEnd();									// 01, 06
		else if (recv_by11 == 0x08) recv_ConfigWriteIndex();							// 01, 08
		else if (recv_by11 == 0x09) recv_ConfigSerialReq();								// 01, 09
		else if (recv_by11 == 0x0A) recv_Pair_Serial();									// 01, 0A
		else if (recv_by11 == 0x0E) recv_ConfigStatusReq();								// 01, 0E

		#if defined(AS_DBG)																// some debug message
		else Serial << F("\nUNKNOWN MESSAGE, PLEASE REPORT!\n\n");
		#endif
	}
	
	// l> 0A 73 80 02 63 19 63 2F B7 4A 00
	if ((recv.forUs) && (recv_isMsg) && (recv_msgTp == 0x02)) {							// message seems to be an ACK
		send.counter = 0;

		if (pevt.act == 1) {
			// we got an ACK after key press?
			statusLed.stop(STATUSLED_BOTH);
			statusLed.set(STATUSLED_1, STATUSLED_MODE_BLINKFAST, 1);					// blink led 1 once
		}
	}
		
	// pair event handling
	if ((recv.forUs) && (recv_isMsg) && (recv_msgTp == 0x11)) {
		recv_PairEvent();
	}

	// peer event handling
	if ((recv.forUs) && (recv_isMsg) && (recv_msgTp >= 0x12)) {
		recv_PeerEvent();
	}
	
	if (recv.forUs) main_Jump();														// does main sketch want's to be informed?

	//to do: if it is a broadcast message, do something with
	recv.data[0] = 0;																	// otherwise ignore
}
void     HM::send_poll(void) {															// handles the send queue
	if((send.counter <= send.retries) && (send.timer <= millis())) {					// not all sends done and timing is OK
		
		// here we encode and send the string
		hm_enc(send.data);																// encode the string
		detachInterrupt(intGDO0.nbr);													// disable interrupt otherwise we could get some new content while we copy the buffer
		cc.sendData(send.data,send.burst);												// and send
		attachInterrupt(intGDO0.nbr,isrGDO0,FALLING);									// enable the interrupt again
		hm_dec(send.data);																// decode the string
		
		// setting some variables
		send.counter++;																	// increase send counter
		send.timer = millis() + dParm.timeOut;											// set the timer for next action
		powr.state = 1;																	// remember TRX module status, after sending it is always in RX mode
		if ((powr.mode > 0) && (powr.nxtTO < (millis() + powr.minTO))) stayAwake(powr.minTO); // stay awake for some time

		#if defined(AS_DBG)																// some debug messages
		Serial << F("<- ") << pHexL(send.data, send.data[0]+1) << pTime();
		#endif

		if (pevt.act == 1) {
			statusLed.set(STATUSLED_BOTH, STATUSLED_MODE_BLINKFAST, 1);					// blink led 1 and led 2 once after key press
		}
	}
	
	if((send.counter > send.retries) && (send.counter < dParm.maxRetr)) {				// all send but don't wait for an ACK
		send.counter = 0; send.timer = 0;												// clear send flag
	}
	
	if((send.counter > send.retries) && (send.timer <= millis())) {						// max retries achieved, but seems to have no answer
		send.counter = 0; send.timer = 0;												// cleanup of send buffer
		// todo: error handling, here we could jump some were to blink a led or whatever
		
		if (pevt.act == 1) {
			statusLed.stop(STATUSLED_BOTH);
			statusLed.set(STATUSLED_2, STATUSLED_MODE_BLINKSLOW, 1);					// blink the led 2 once if key press before
		}

		#if defined(AS_DBG)
		Serial << F("-> NA ") << pTime();
		#endif
	}
}																															// ready, should work
void     HM::send_conf_poll(void) {
	if (send.counter > 0) return;														// send queue is busy, let's wait
	uint8_t len;
	
	statusLed.set(STATUSLED_1, STATUSLED_MODE_BLINKSFAST, 1);							// blink the led 1 once

	//Serial << "send conf poll\n";
	if (conf.type == 0x01) {
		// answer                            Cnl  Peer         Peer         Peer         Peer
		// l> 1A 05 A0 10 1E 7A AD 63 19 63  01   1F A6 5C 02  1F A6 5C 01  11 22 33 02  11 22 33 01
		//                                   Cnl  Termination
		// l> 0E 06 A0 10 1E 7A AD 63 19 63  01   00 00 00 00
		len = getPeerForMsg(conf.channel, send_payLoad+1);								// get peer list
		if (len == 0x00) {																// check if all done
			memset(&conf, 0, sizeof(conf));												// clear the channel struct
			return;																		// exit
		} else if (len == 0xff) {														// failure, out of range
			memset(&conf, 0, sizeof(conf));												// clear the channel struct
			send_NACK();
		} else {																		// seems to be ok, answer
			send_payLoad[0] = 0x01;														// INFO_PEER_LIST
			send_prep(conf.mCnt++,0xA0,0x10,conf.reID,send_payLoad,len+1);				// prepare the message
		//send_prep(send.mCnt++,0xA0,0x10,conf.reID,send_payLoad,len+1);				// prepare the message
		}
	} else if (conf.type == 0x02) {
		// INFO_PARAM_RESPONSE_PAIRS message
		//                               RegL_01:  30:06 32:50 34:4B 35:50 56:00 57:24 58:01 59:01 00:00
		// l> 1A 04 A0 10  1E 7A AD  63 19 63  02  30 06 32 50 34 4B 35 50 56 00 57 24 58 01 59 01 (l:27)(131405)
		
		len = getListForMsg2(conf.channel, conf.list, conf.peer, send_payLoad+1);		// get the message
		if (len == 0) {																	// check if all done
			memset(&conf, 0, sizeof(conf));												// clear the channel struct
			return;																		// and exit
		} else if (len == 0xff) {														// failure, out of range
			memset(&conf, 0, sizeof(conf));												// clear the channel struct
			send_NACK();
		} else {																		// seems to be ok, answer
			send_payLoad[0] = 0x02;			 											// INFO_PARAM_RESPONSE_PAIRS
			send_prep(conf.mCnt++,0xA0,0x10,conf.reID,send_payLoad,len+1);				// prepare the message
			//send_prep(send.mCnt++,0xA0,0x10,conf.reID,send_payLoad,len+1);			// prepare the message
		}
	} else if (conf.type == 0x03) {
		// INFO_PARAM_RESPONSE_SEQ message
		//                               RegL_01:  30:06 32:50 34:4B 35:50 56:00 57:24 58:01 59:01 00:00
		// l> 1A 04 A0 10  1E 7A AD  63 19 63  02  30 06 32 50 34 4B 35 50 56 00 57 24 58 01 59 01 (l:27)(131405)
		
	}
	
}
void     HM::send_peer_poll(void) {
	// step through the peers and send the message
	// if we didn't found a peer to send to, then send status to master

	static uint8_t pPtr = 0, msgCnt_L, statSend;
	static s_cnlDefType *t, *tL;
	
	if (send.counter > 0) return;														// something is in the send queue, lets wait for the next free slot

	// first time run detection
	if ((tL == pevt.t) && (msgCnt_L == pevt.msgCnt)) {									// nothing changed
		if ((statSend == 1) && (pPtr >= _pgmB(&t->pMax))) {								// check if we are through
			pevt.act = 0;																// no need to jump in again
			return;
		}
	} else {																			// start again
		tL = pevt.t;																	// remember for next time
		msgCnt_L = pevt.msgCnt;
		pPtr = 0;																		// set peer pointer to start
		statSend = 0;																	// if loop is on end and statSend is 0 then we have to send to master
		t  = pevt.t;																	// some shorthand
	}
	
	// check if peer pointer is through and status to master must be send - return if done
	if ((statSend == 0) && ( pPtr >= _pgmB(&t->pMax))) {
		send_prep(send.mCnt++,(0x82|pevt.reqACK),pevt.type,dParm.MAID,pevt.data,pevt.len); // prepare the message
		statSend = 1;
		return;
	}
	
	// step through the peers, get the respective list4 and send the message
	uint32_t tPeer;
	uint8_t ret = getPeerByIdx(_pgmB(&t->cnl),pPtr,(uint8_t*)&tPeer);									
	//Serial << "pP:" << pPtr << ", tp:" << pHex((uint8_t*)&tPeer,4) << '\n';
	if (tPeer == 0) {
		pPtr++;
		return;
	}
	
	// if we are here we have a valid peer address to send to
	statSend = 1;																		// first peer address found, no need to send status to the master
	
	// now we need the respective list4 to know if we have to send a burst message
	// in regLstByte there are two information. peer needs AES and burst needed
	// AES will be ignored at the moment, but burst needed will be translated into the message flag - bit 0 in regLstByte, translated to bit 4 = burst transmission in msgFlag
	uint8_t lB[1];
	getRegAddr(_pgmB(&t->cnl),_pgmB(&t->lst),pPtr++,0x01,1,lB);							// get regs for 0x01
	//Serial << "rB:" << pHexB(lB[0]) << '\n';
	send_prep(send.mCnt++,(0x82|pevt.reqACK|bitRead(lB[0],0)<<4),pevt.type,(uint8_t*)&tPeer,pevt.data,pevt.len); // prepare the message
}
void     HM::power_poll(void) {
	// there are 3 power modes for the TRX868 module
	// TX mode will switched on while something is in the send queue
	// 1 - RX mode enabled by default, take approx 17ma
	// 2 - RX is in burst mode, RX will be switched on every 250ms to check if there is a carrier signal
	//     if yes - RX will stay enabled until timeout is reached, prolongation of timeout via receive function seems not necessary
	//				to be able to receive an ACK, RX mode should be switched on by send function
	//     if no  - RX will go in idle mode and wait for the next carrier sense check
	// 3 - RX is off by default, TX mode is enabled while sending something
	//     configuration mode is required in this setup to be able to receive at least pairing and config request strings
	//     should be realized by a 15 sec timeout function for RX mode
	//     system time in millis will be hold by a regular wakeup from the watchdog timer
	// 4 - Same as power mode 3 but without watchdog
	
	if (powr.mode == 0) return;															// in mode 0 there is nothing to do
	if (powr.nxtTO > millis()) return;													// no need to do anything
	if (send.counter > 0) return;														// send queue not empty
	
	// power mode 2, module is in sleep and next check is reached
	if ((powr.mode == 2) && (powr.state == 0)) {
		if (cc.detectBurst()) {															// check for a burst signal, if we have one, we should stay awake
			powr.nxtTO = millis() + powr.minTO;											// schedule next timeout with some delay
		} else {																		// no burst was detected, go to sleep in next cycle
			powr.nxtTO = millis();														// set timer accordingly
		}
		powr.state = 1;																	// set status to awake
		return;
	}

	// power mode 2, module is active and next check is reached
	if ((powr.mode == 2) && (powr.state == 1)) {
		cc.setPowerDownState();															// go to sleep
		powr.state = 0;
		powr.nxtTO = millis() + 250;													// schedule next check in 250 ms
	}

	// 	power mode 3, check RX mode against timer. typically RX is off beside a special command to switch RX on for at least 30 seconds
	if ((powr.mode >= 3) && (powr.state == 1)) {
		cc.setPowerDownState();															// go to sleep
		powr.state = 0;
	}

	// sleep for mode 2, 3 and 4
	if ((powr.mode > 1) && (powr.state == 0)) {											// TRX module is off, so lets sleep for a while
		statusLed.stop(STATUSLED_BOTH);													// stop blinking, because we are going to sleep
		if ((powr.mode == 2) || (powr.mode == 3)) WDTCSR |= (1<<WDIE);					// enable watch dog if power mode 2 or 3

		//Serial << ":";
		//delay(100);

		ADCSRA = 0;																		// disable ADC
		uint8_t xPrr = PRR;																// turn off various modules
		PRR = 0xFF;

		sleep_enable();																	// enable the sleep mode

		MCUCR = (1<<BODS)|(1<<BODSE);													// turn off brown-out enable in software
		MCUCR = (1<<BODS);																// must be done right before sleep

		sleep_cpu();																	// goto sleep

		/* wake up here */
		
		sleep_disable();																// disable sleep
		if ((powr.mode == 2) || (powr.mode == 3)) WDTCSR &= ~(1<<WDIE);					// disable watch dog
		PRR = xPrr;																		// restore modules
		
		if (wd_flag == 1) {																// add the watchdog time to millis()
			wd_flag = 0;																// to detect the next watch dog timeout
			timer0_millis += powr.wdTme;												// add watchdog time to millis() function
		} else {
			stayAwake(powr.minTO);														// stay awake for some time, if the wakeup where not raised from watchdog
		}
		//Serial << ".";

	}
}
void     HM::module_poll(void) {
	for (uint8_t i = 0; i <= dDef.cnlNbr; i++) {
		if (modTbl[i].use) modTbl[i].mDlgt(0,0,0,NULL,0);
	}
}

void     HM::hm_enc(uint8_t *buf) {

	buf[1] = (~buf[1]) ^ 0x89;
	uint8_t buf2 = buf[2];
	uint8_t prev = buf[1];

	uint8_t i;
	for (i=2; i<buf[0]; i++) {
		prev = (prev + 0xdc) ^ buf[i];
		buf[i] = prev;
	}

	buf[i] ^= buf2;
}
void     HM::hm_dec(uint8_t *buf) {

	uint8_t prev = buf[1];
	buf[1] = (~buf[1]) ^ 0x89;

	uint8_t i, t;
	for (i=2; i<buf[0]; i++) {
		t = buf[i];
		buf[i] = (prev + 0xdc) ^ buf[i];
		prev = t;
	}

	buf[i] ^= buf[2];
}
void     HM::exMsg(uint8_t *buf) {
	#ifdef AS_DBG_Explain

	#define b_len			buf[0]
	#define b_msgTp			buf[3]
	#define b_by10			buf[10]
	#define b_by11			buf[11]

	Serial << F("   ");																	// save some byte and send 3 blanks once, instead of having it in every if
	
	if        ((b_msgTp == 0x00)) {
		Serial << F("DEVICE_INFO; fw: ") << pHex(&buf[10],1) << F(", type: ") << pHex(&buf[11],2) << F(", serial: ") << pHex(&buf[13],10) << '\n';
		Serial << F("              , class: ") << pHex(&buf[23],1) << F(", pCnlA: ") << pHex(&buf[24],1) << F(", pCnlB: ") << pHex(&buf[25],1) << F(", na: ") << pHex(&buf[26],1);

		} else if ((b_msgTp == 0x01) && (b_by11 == 0x01)) {
		Serial << F("CONFIG_PEER_ADD; cnl: ") << pHex(&buf[10],1) << F(", peer: ") << pHex(&buf[12],3) << F(", pCnlA: ") << pHex(&buf[15],1) << F(", pCnlB: ") << pHex(&buf[16],1);

		} else if ((b_msgTp == 0x01) && (b_by11 == 0x02)) {
		Serial << F("CONFIG_PEER_REMOVE; cnl: ") << pHex(&buf[10],1) << F(", peer: ") << pHex(&buf[12],3) << F(", pCnlA: ") << pHex(&buf[15],1) << F(", pCnlB: ") << pHex(&buf[16],1);

		} else if ((b_msgTp == 0x01) && (b_by11 == 0x03)) {
		Serial << F("CONFIG_PEER_LIST_REQ; cnl: ") << pHex(&buf[10],1);

		} else if ((b_msgTp == 0x01) && (b_by11 == 0x04)) {
		Serial << F("CONFIG_PARAM_REQ; cnl: ") << pHex(&buf[10],1) << F(", peer: ") << pHex(&buf[12],3) << F(", pCnl: ") << pHex(&buf[15],1) << F(", lst: ") << pHex(&buf[16],1);

		} else if ((b_msgTp == 0x01) && (b_by11 == 0x05)) {
		Serial << F("CONFIG_START; cnl: ") << pHex(&buf[10],1) << F(", peer: ") << pHex(&buf[12],3) << F(", pCnl: ") << pHex(&buf[15],1) << F(", lst: ") << pHex(&buf[16],1);

		} else if ((b_msgTp == 0x01) && (b_by11 == 0x06)) {
		Serial << F("CONFIG_END; cnl: ") << pHex(&buf[10],1);

		} else if ((b_msgTp == 0x01) && (b_by11 == 0x08)) {
		Serial << F("CONFIG_WRITE_INDEX; cnl: ") << pHex(&buf[10],1) << F(", data: ") << pHex(&buf[12],(buf[0]-11));

		} else if ((b_msgTp == 0x01) && (b_by11 == 0x09)) {
		Serial << F("CONFIG_SERIAL_REQ");
		
		} else if ((b_msgTp == 0x01) && (b_by11 == 0x0A)) {
		Serial << F("PAIR_SERIAL, serial: ") << pHex(&buf[12],10);

		} else if ((b_msgTp == 0x01) && (b_by11 == 0x0E)) {
		Serial << F("CONFIG_STATUS_REQUEST, cnl: ") << pHex(&buf[10],1);

		} else if ((b_msgTp == 0x02) && (b_by10 == 0x00)) {
		if (b_len == 0x0A) Serial << F("ACK");
		else Serial << F("ACK; data: ") << pHex(&buf[11],b_len-10);

		} else if ((b_msgTp == 0x02) && (b_by10 == 0x01)) {
		Serial << F("ACK_STATUS; cnl: ") << pHex(&buf[11],1) << F(", status: ") << pHex(&buf[12],1) << F(", down/up/loBat: ") << pHex(&buf[13],1);
		if (b_len > 13) Serial << F(", rssi: ") << pHex(&buf[14],1);

		} else if ((b_msgTp == 0x02) && (b_by10 == 0x02)) {
		Serial << F("ACK2");
		
		} else if ((b_msgTp == 0x02) && (b_by10 == 0x04)) {
		Serial << F("ACK_PROC; para1: ") << pHex(&buf[11],2) << F(", para2: ") << pHex(&buf[13],2) << F(", para3: ") << pHex(&buf[15],2) << F(", para4: ") << pHex(&buf[17],1);

		} else if ((b_msgTp == 0x02) && (b_by10 == 0x80)) {
		Serial << F("NACK");

		} else if ((b_msgTp == 0x02) && (b_by10 == 0x84)) {
		Serial << F("NACK_TARGET_INVALID");
		
		} else if ((b_msgTp == 0x03)) {
		Serial << F("AES_REPLY; data: ") << pHex(&buf[10],b_len-9);
		
		} else if ((b_msgTp == 0x04) && (b_by10 == 0x01)) {
		Serial << F("TOpHMLAN:SEND_AES_CODE; cnl: ") << pHex(&buf[11],1);

		} else if ((b_msgTp == 0x04)) {
		Serial << F("TO_ACTOR:SEND_AES_CODE; code: ") << pHex(&buf[11],1);
		
		} else if ((b_msgTp == 0x10) && (b_by10 == 0x00)) {
		Serial << F("INFO_SERIAL; serial: ") << pHex(&buf[11],10);

		} else if ((b_msgTp == 0x10) && (b_by10 == 0x01)) {
		Serial << F("INFO_PEER_LIST; peer1: ") << pHex(&buf[11],4);
		if (b_len >= 19) Serial << F(", peer2: ") << pHex(&buf[15],4);
		if (b_len >= 23) Serial << F(", peer3: ") << pHex(&buf[19],4);
		if (b_len >= 27) Serial << F(", peer4: ") << pHex(&buf[23],4);

		} else if ((b_msgTp == 0x10) && (b_by10 == 0x02)) {
		Serial << F("INFO_PARAM_RESPONSE_PAIRS; data: ") << pHex(&buf[11],b_len-10);

		} else if ((b_msgTp == 0x10) && (b_by10 == 0x03)) {
		Serial << F("INFO_PARAM_RESPONSE_SEQ; offset: ") << pHex(&buf[11],1) << F(", data: ") << pHex(&buf[12],b_len-11);

		} else if ((b_msgTp == 0x10) && (b_by10 == 0x04)) {
		Serial << F("INFO_PARAMETER_CHANGE; cnl: ") << pHex(&buf[11],1) << F(", peer: ") << pHex(&buf[12],4) << F(", pLst: ") << pHex(&buf[16],1) << F(", data: ") << pHex(&buf[17],b_len-16);

		} else if ((b_msgTp == 0x10) && (b_by10 == 0x06)) {
		Serial << F("INFO_ACTUATOR_STATUS; cnl: ") << pHex(&buf[11],1) << F(", status: ") << pHex(&buf[12],1) << F(", na: ") << pHex(&buf[13],1);
		if (b_len > 13) Serial << F(", rssi: ") << pHex(&buf[14],1);
		
		} else if ((b_msgTp == 0x11) && (b_by10 == 0x02)) {
		Serial << F("SET; cnl: ") << pHex(&buf[11],1) << F(", value: ") << pHex(&buf[12],1) << F(", rampTime: ") << pHex(&buf[13],2) << F(", duration: ") << pHex(&buf[15],2);

		} else if ((b_msgTp == 0x11) && (b_by10 == 0x03)) {
		Serial << F("STOP_CHANGE; cnl: ") << pHex(&buf[11],1);

		} else if ((b_msgTp == 0x11) && (b_by10 == 0x04) && (b_by11 == 0x00)) {
		Serial << F("RESET");

		} else if ((b_msgTp == 0x11) && (b_by10 == 0x80)) {
		Serial << F("LED; cnl: ") << pHex(&buf[11],1) << F(", color: ") << pHex(&buf[12],1);

		} else if ((b_msgTp == 0x11) && (b_by10 == 0x81) && (b_by11 == 0x00)) {
		Serial << F("LED_ALL; Led1To16: ") << pHex(&buf[12],4);
		
		} else if ((b_msgTp == 0x11) && (b_by10 == 0x81)) {
		Serial << F("LED; cnl: ") << pHex(&buf[11],1) << F(", time: ") << pHex(&buf[12],1) << F(", speed: ") << pHex(&buf[13],1);
		
		} else if ((b_msgTp == 0x11) && (b_by10 == 0x82)) {
		Serial << F("SLEEPMODE; cnl: ") << pHex(&buf[11],1) << F(", mode: ") << pHex(&buf[12],1);
		
		} else if ((b_msgTp == 0x12)) {
		Serial << F("HAVE_DATA");
		
		} else if ((b_msgTp == 0x3E)) {
		Serial << F("SWITCH; dst: ") << pHex(&buf[10],3) << F(", na: ") << pHex(&buf[13],1) << F(", cnl: ") << pHex(&buf[14],1) << F(", counter: ") << pHex(&buf[15],1);
		
		} else if ((b_msgTp == 0x3F)) {
		Serial << F("TIMESTAMP; na: ") << pHex(&buf[10],2) << F(", time: ") << pHex(&buf[12],2);
		
		} else if ((b_msgTp == 0x40)) {
		Serial << F("REMOTE; button: ") << pHexB(buf[10] & 0x3F) << F(", long: ") << (buf[10] & 0x40 ? 1:0) << F(", lowBatt: ") << (buf[10] & 0x80 ? 1:0) << F(", counter: ") << pHexB(buf[11]);
		
		} else if ((b_msgTp == 0x41)) {
		Serial << F("SENSOR_EVENT; button: ") <<pHexB(buf[10] & 0x3F) << F(", long: ") << (buf[10] & 0x40 ? 1:0) << F(", lowBatt: ") << (buf[10] & 0x80 ? 1:0) << F(", value: ") << pHex(&buf[11],1) << F(", next: ") << pHex(&buf[12],1);
		
		} else if ((b_msgTp == 0x53)) {
		Serial << F("SENSOR_DATA; cmd: ") << pHex(&buf[10],1) << F(", fld1: ") << pHex(&buf[11],1) << F(", val1: ") << pHex(&buf[12],2) << F(", fld2: ") << pHex(&buf[14],1) << F(", val2: ") << pHex(&buf[15],2) << F(", fld3: ") << pHex(&buf[17],1) << F(", val3: ") << pHex(&buf[18],2) << F(", fld4: ") << pHex(&buf[20],1) << F(", val4: ") << pHex(&buf[21],2);
		
		} else if ((b_msgTp == 0x58)) {
		Serial << F("CLIMATE_EVENT; cmd: ") << pHex(&buf[10],1) << F(", valvePos: ") << pHex(&buf[11],1);
		
		} else if ((b_msgTp == 0x70)) {
		Serial << F("WEATHER_EVENT; temp: ") << pHex(&buf[10],2) << F(", hum: ") << pHex(&buf[12],1);

		} else {
		Serial << F("Unknown Message, please report!");
	}
	Serial << F("\n\n");
	#endif
}

// receive message handling
void     HM::recv_ConfigPeerAdd(void) {
	// description --------------------------------------------------------
	//                                  Cnl      PeerID    PeerCnl_A  PeerCnl_B
	// l> 10 55 A0 01 63 19 63 1E 7A AD 03   01  1F A6 5C  06         05

	// do something with the information ----------------------------------
	addPeerFromMsg(recv_payLoad[0], recv_payLoad+2);

	// send appropriate answer ---------------------------------------------
	// l> 0A 55 80 02 1E 7A AD 63 19 63 00
	if (recv_ackRq) send_ACK();															// send ACK if requested
}
void     HM::recv_ConfigPeerRemove(void) {
	// description --------------------------------------------------------
	//                                  Cnl      PeerID    PeerCnl_A  PeerCnl_B
	// l> 10 55 A0 01 63 19 63 1E 7A AD 03   02  1F A6 5C  06         05

	// do something with the information ----------------------------------
	uint8_t ret = remPeerFromMsg(recv_payLoad[0], recv_payLoad+2);

	// send appropriate answer ---------------------------------------------
	// l> 0A 55 80 02 1E 7A AD 63 19 63 00
	if (recv_ackRq) send_ACK();
	//if ((recv_ackRq) && (ret == 1)) send_ACK();										// send ACK if requested
	//else if (recv_ackRq) send_NACK();
}
void     HM::recv_ConfigPeerListReq(void) {
	// description --------------------------------------------------------
	//                                  Cnl
	// l> 0B 05 A0 01 63 19 63 1E 7A AD 01  03

	// do something with the information ----------------------------------
	conf.mCnt = recv_rCnt;
	conf.channel = recv_payLoad[0];
	memcpy(conf.reID, recv_reID, 3);
	conf.type = 0x01;

	// send appropriate answer ---------------------------------------------
	// answer will be generated in config_poll function
	conf.act = 1;
}
void     HM::recv_ConfigParamReq(void) {
	// description --------------------------------------------------------
	//                                  Cnl    PeerID    PeerCnl  ParmLst
	// l> 10 04 A0 01 63 19 63 1E 7A AD 01  04 00 00 00  00       01
	// do something with the information ----------------------------------
	conf.mCnt = recv_rCnt;
	conf.channel = recv_payLoad[0];
	conf.list = recv_payLoad[6];
	memcpy(conf.peer, &recv_payLoad[2], 4);
	memcpy(conf.reID, recv_reID, 3);
	// todo: check when message type 2 or 3 is selected
	conf.type = 0x02;

	// send appropriate answer ---------------------------------------------
	// answer will be generated in config_poll function
	conf.act = 1;
}
void     HM::recv_ConfigStart(void) {
	// description --------------------------------------------------------
	//                                  Cnl    PeerID    PeerCnl  ParmLst
	// l> 10 01 A0 01 63 19 63 1E 7A AD 00  05 00 00 00  00       00

	// do something with the information ----------------------------------
	// todo: check against known master id, if master id is empty, set from everyone is allowed
	conf.channel = recv_payLoad[0];														// set parameter
	memcpy(conf.peer,&recv_payLoad[2],4);
	conf.list = recv_payLoad[6];
	conf.wrEn = 1;																		// and enable write to config
	
	// send appropriate answer ---------------------------------------------
	if (recv_ackRq) send_ACK();															// send ACK if requested
}
void     HM::recv_ConfigEnd(void) {
	// description --------------------------------------------------------
	//                                  Cnl
	// l> 0B 01 A0 01 63 19 63 1E 7A AD 00  06

	// do something with the information ----------------------------------
	conf.wrEn = 0;																		// disable write to config
	loadRegs();																			// probably something changed, reload config
	module_Jump(recv_msgTp, recv_by10, recv_by11, recv_by10, NULL, 0);					// search and jump in the respective channel module
	
	// send appropriate answer ---------------------------------------------
	if (recv_ackRq) send_ACK();															// send ACK if requested
}
void     HM::recv_ConfigWriteIndex(void) {
	// description --------------------------------------------------------
	//                                  Cnl    Data
	// l> 13 02 A0 01 63 19 63 1E 7A AD 00  08 02 01 0A 63 0B 19 0C 63

	// do something with the information ----------------------------------
	if ((!conf.wrEn) || (!(conf.channel == recv_payLoad[0]))) {							// but only if we are in config mode
		#if defined(AS_DBG)
		Serial << F("   write data, but not in config mode\n");
		#endif
		return;
	}
	uint8_t payLen = recv_len - 11;														// calculate len of payload and provide the data
	uint8_t ret = setListFromMsg(conf.channel, conf.list, conf.peer, payLen, &recv_payLoad[2]);
	//Serial << "we: " << conf.wrEn << ", cnl: " << conf.channel << ", lst: " << conf.list << ", peer: " << pHex(conf.peer,4) << '\n';
	//Serial << "pl: " << pHex(&recv_payLoad[2],payLen) << ", ret: " << ret << '\n';

	// send appropriate answer ---------------------------------------------
	if ((recv_ackRq) && (ret == 1))send_ACK();											// send ACK if requested
	else if (recv_ackRq) send_NACK();													// send NACK while something went wrong
}
void     HM::recv_ConfigSerialReq(void) {
	// description --------------------------------------------------------
	// l> 0B 48 A0 01 63 19 63 1E 7A AD 00 09

	// do something with the information ----------------------------------
	// nothing to do, we have only to answer

	// send appropriate answer ---------------------------------------------
	//                                     SerNr
	// l> 14 48 80 10 1E 7A AD 63 19 63 00 4A 45 51 30 37 33 31 39 30 35
	send_payLoad[0] = 0x00; 															// INFO_SERIAL
	memcpy_P(&send_payLoad[1], &dParm.p[3], 11);										// copy details out of register.h
	send_prep(recv_rCnt,0x80,0x10,recv_reID,send_payLoad,11);							// prepare the message
	//send_prep(send.mCnt++,0x80,0x10,recv_reID,send_payLoad,11);						// prepare the message
}
void     HM::recv_Pair_Serial(void) {
	// description --------------------------------------------------------
	//                                         Serial
	// l> 15 48 A0 01 63 19 63 1E 7A AD 00 0A  4A 45 51 30 37 33 31 39 30 35
	
	// do something with the information ----------------------------------
	// compare serial number with own serial number and send pairing string back
	if (memcmp_P(&recv_payLoad[2], &dParm.p[3], 10) != 0) return;

	// send appropriate answer ---------------------------------------------
	// l> 1A 01 A2 00 3F A6 5C 00 00 00 10 80 02 50 53 30 30 30 30 30 30 30 31 9F 04 01 01
	memcpy_P(send_payLoad, dParm.p, 17);												// copy details out of register.h
	send_prep(send.mCnt++,0xA2,0x00,recv_reID,send_payLoad,17);
}
void     HM::recv_ConfigStatusReq(void) {
	// description --------------------------------------------------------
	//                                   Cnl
	// l> 0B 30 A0 01 63 19 63 2F B7 4A  01  0E

	// do something with the information ----------------------------------
	module_Jump(recv_msgTp, recv_by10, recv_by11, recv_by10, NULL, 0);					// jump in the respective channel module

	// send appropriate answer ---------------------------------------------
	// answer will be send from client function
}
void     HM::recv_PeerEvent(void) {
	// description --------------------------------------------------------
	//                 peer                cnl  payload
	// -> 0B 56 A4 40  AA BB CC  3F A6 5C  03   CA

	// do something with the information ----------------------------------
	uint8_t peer[4];																	// bring it in a search able format
	memcpy(peer,recv_reID,4);
	peer[3] = recv_payLoad[0] & 0xF;													// only the low byte is the channel indicator
	
	uint8_t cnl = getCnlByPeer(peer);													// check on peer database
	if (!cnl) return;																	// if peer was not found, the function returns a 0 and we can leave

	getList3ByPeer(cnl, peer);															// load list3
	
	module_Jump(recv_msgTp, recv_by10, recv_by11, cnl, recv_payLoad+1, recv_len-10);
	
	// send appropriate answer ---------------------------------------------
	// answer should be initiated by client function in user area
}
void     HM::recv_PairEvent(void) {
	// description --------------------------------------------------------
	//                 pair                    cnl  payload
	// -> 0E E6 A0 11  63 19 63  2F B7 4A  02  01   C8 00 00
	// answer is an enhanced ACK:
	// <- 0E E7 80 02 1F B7 4A 63 19 63 01 01 C8 00 54
	
	// do something with the information ----------------------------------
	module_Jump(recv_msgTp, recv_by10, recv_by11, recv_by11, recv_payLoad+2, recv_len-11);
	
	// send appropriate answer ---------------------------------------------
	// answer should be initiated by client function in user area
}
uint8_t  HM::main_Jump(void) {
	uint8_t ret = 0;
	for (uint8_t i = 0; ; i++) {														// find the call back function
		s_jumptable *p = &jTbl[i];														// some shorthand
		if (p->by3 == 0) break;															// break if on end of list
		
		if ((p->by3 != recv_msgTp) && (p->by3  != 0xff)) continue;						// message type didn't fit, therefore next
		if ((p->by10 != recv_by10) && (p->by10 != 0xff)) continue;						// byte 10 didn't fit, therefore next
		if ((p->by11 != recv_by11) && (p->by11 != 0xff)) continue;						// byte 11 didn't fit, therefore next
		
		// if we are here, all conditions are checked and ok
		p->fun(recv_payLoad, recv_len - 9);												// and jump into
		ret = 1;																		// remember that we found a valid function to jump into
	}
	return ret;
}
uint8_t  HM::module_Jump(uint8_t by3, uint8_t by10, uint8_t by11, uint8_t cnl, uint8_t *data, uint8_t len) {
	if (modTbl[cnl].use == 0) return 0;													// nothing found in the module table, exit

	modTbl[cnl].mDlgt(by3,by10,by11,data,len);											// find the right entry in the table
	return 1;
}

//- internal send functions
void     HM::send_prep(uint8_t msgCnt, uint8_t comBits, uint8_t msgType, uint8_t *targetID, uint8_t *payLoad, uint8_t payLen) {
	send.data[0]  = 9 + payLen;															// message length
	send.data[1]  = msgCnt;																// message counter

	// set the message flags
	//    #RPTEN    0x80: set in every message. Meaning?
	//    #RPTED    0x40: repeated (repeater operation)
	//    #BIDI     0x20: response is expected						- should be in comBits
	//    #Burst    0x10: set if burst is required by device        - will be set in peer send string if necessary
	//    #Bit3     0x08:
	//    #CFG      0x04: Device in Config mode						- peer seems to be always in config mode, message to master only if an write mode enable string was received
	//    #WAKEMEUP 0x02: awake - hurry up to send messages			- only for a master, peer don't need
	//    #WAKEUP   0x01: send initially to keep the device awake	- should be only necessary while receiving

	send.data[2]  = comBits;															// take the communication bits
	send.data[3]  = msgType;															// message type
	memcpy(&send.data[4], dParm.HMID, 3);												// source id
	memcpy(&send.data[7], targetID, 3);													// target id

	if ((uint16_t)payLoad != (uint16_t)send_payLoad)
	memcpy(send_payLoad, payLoad, payLen);												// payload
	
	#if defined(AS_DBG)																	// some debug messages
	exMsg(send.data);																	// explain message
	#endif

	send_out();
}

//- to check incoming messages if sender is known
uint8_t  HM::isPeerKnown(uint8_t *peer) {
	uint8_t tPeer[3];

	for (uint16_t i = 0; i < ee[1].peerDB; i+=4) {
		getEeBl(ee[0].peerDB+i,3,tPeer);												// copy the respective eeprom block to the pointer
		if (memcmp(peer,tPeer,3) == 0) return 1;
	}
	
	return 0;																			// nothing found
}
uint8_t  HM::isPairKnown(uint8_t *pair) {
	//Serial << "x:" << pHex(dParm.MAID,3) << " y:" << pHex(pair,3) << '\n';
	if (memcmp(dParm.MAID, bCast, 3) == 0) return 1;									// return 1 while not paired
	
	if (memcmp(dParm.MAID, pair, 3) == 0) return 1;										// check against regDev
	else return 0;																		// found nothing, return 0
}

//- peer specific public functions
uint8_t  HM::addPeerFromMsg(uint8_t cnl, uint8_t *peer) {
	// given peer incl the peer address (3 bytes) and probably of 2 peer channels (2 bytes)
	// therefore we have to shift bytes in our own variable
	uint8_t tPeer[4],ret1,ret2;															// size variables

	if (peer[3]) {
		memcpy(tPeer,peer,4);															// copy the peer in a variable
		ret1 = addPeer(cnl, tPeer);														// add the first peer
	}

	if (peer[4]) {
		tPeer[3] = peer[4];																// change the peer channel
		ret2 = addPeer(cnl, tPeer);														// add the second peer
	}

	#ifdef RPDB_DBG																		// some debug message
	Serial << F("addPeerFromMsg, cnl: ") << cnl << F(", ret1: ") << ret1 << F(", ret2: ") << ret2 << '\n';
	#endif
	
	//                                  Cnl      PeerID    PeerCnl_A  PeerCnl_B
	// l> 10 55 A0 01 63 19 63 1E 7A AD 03   01  1F A6 5C  06         05
	recv_payLoad[7] = ret1;
	recv_payLoad[8] = ret2;
	module_Jump(recv_msgTp, recv_by10, recv_by11, recv_by10, recv_payLoad+5, 4);		// jump in the respective channel module

	return 1;																			// every thing went ok
}
uint8_t  HM::remPeerFromMsg(uint8_t cnl, uint8_t *peer) {
	// given peer incl the peer address (3 bytes) and probably of 2 peer channels (2 bytes)
	// therefore we have to shift bytes in our own variable
	uint8_t tPeer[4];																	// size variables

	memcpy(tPeer,peer,4);																// copy the peer in a variable
	uint8_t ret1 = remPeer(cnl, tPeer);													// remove the first peer

	tPeer[3] = peer[4];																	// change the peer channel
	uint8_t ret2 = remPeer(cnl, tPeer);													// remove the second peer

	#ifdef RPDB_DBG																		// some debug message
	Serial << F("remPeerFromMsg, cnl: ") << cnl << F(", ret1: ") << ret1 << F(", ret2: ") << ret2 << '\n';
	#endif
	
	return 1;																			// every thing went ok
}
uint8_t  HM::valPeerFromMsg(uint8_t *peer) {
	uint8_t ret = getCnlByPeer(peer);
	return (ret == 0xff)?0:1;
}
uint8_t  HM::getPeerForMsg(uint8_t cnl, uint8_t *buf) {
	static uint8_t idx_L = 0xff, ptr;
	static s_cnlDefType *tL;

	//uint8_t idx = cnlDefPeerIdx(cnl);													// get the index number in cnlDefType
	s_cnlDefType *t = cnlDefbyPeer(cnl);												// get the index number in cnlDefType

 	if (tL != t) {																		// check for first time
		ptr = 0;																		// set the pointer to 0
		tL = t;																			// remember for next call check
		
	} else {																			// next time detected
		if (ptr == 0xff) {																// we are completely through
			tL = NULL;
			return 0;	
		}
	}
	
	uint8_t cnt = 0;																	// size a counter and a long peer store
	uint32_t peerL;

	while ((ptr < _pgmB(&t->pMax)) && (cnt < maxPayload)) {								// loop until buffer is full or end of table is reached

		peerL = getEeLo(cnlDefPeerAddr(cnl,ptr));										// get the peer
		
		//Serial << "start: " << start << ", len: " << len << ", pos: " << (cnt-start) << '\n';
		if (peerL != 0) {																// if we found an filled block

			//Serial << "i: " << cnt << "pL: " << pHex((uint8_t*)&peerL,4) << '\n';
			memcpy(&buf[cnt], (uint8_t*)&peerL, 4);										// copy the content
			cnt+=4;																		// increase the pointer to buffer for next time
		}
		ptr++;
	}
	
	if (cnt < maxPayload) {																// termination of message needs two zeros
		memcpy(&buf[cnt], bCast, 4);													// copy the content
		cnt+=4;																			// increase the pointer to buffer for next time
		ptr = 0xff;																		// indicate that we are through
	}

	return cnt;																			// return delivered bytes
}

//- regs specific public functions
uint8_t  HM::getListForMsg2(uint8_t cnl, uint8_t lst, uint8_t *peer, uint8_t *buf) {
	static uint8_t pIdx_L = 0xff, ptr = 0;

	uint8_t pIdx = getIdxByPeer(cnl, peer);												// get the index of the respective peer
	if (pIdx == 0xff) return 0xff;														// peer does not exist

	if (pIdx_L != pIdx) {																// check for first time
		ptr = 0;																		// set the pointer to 0
		pIdx_L = pIdx;																	// remember for next call check
		
	} else {																			// next time detected
		if (ptr == 0xff) {
			pIdx_L = 0xff;																// set the pointer to 0
			return 0;																	// we are completely through
		}
	}
	
	s_cnlDefType *t = cnlDefbyList(cnl, lst);
	if (t == NULL) return 0;

	uint8_t  slcAddr = _pgmB(&t->sIdx);													// get the respective addresses
	uint16_t regAddr = cnlDefTypeAddr(cnl, lst, pIdx);

	uint8_t cnt = 0;																	// step through the bytes and generate message
	while ((ptr < _pgmB(&t->sLen)) && (cnt < maxPayload)) {								// loop until buffer is full or end of table is reached
		memcpy(&buf[cnt++], &dDef.slPtr[slcAddr + ptr], 1);								// copy one byte from sliceStr
		getEeBl(regAddr + ptr, 1, &buf[cnt++]);
		ptr++;
	}

	if (cnt < maxPayload) {																// termination of message needs two zeros
		buf[cnt++] = 0;																	// write 0 to buffer
		buf[cnt++] = 0;
		ptr = 0xff;																		// indicate that we are through
	}

	return cnt;																			// return delivered bytes
}
uint8_t  HM::setListFromMsg(uint8_t cnl, uint8_t lst, uint8_t *peer, uint8_t len, uint8_t *buf) {
	// get the address of the respective slcStr
	uint8_t pIdx = getIdxByPeer(cnl, peer);												// get the index of the respective peer
	if (pIdx == 0xff) return 0xff;														// peer does not exist
	
	s_cnlDefType *t = cnlDefbyList(cnl, lst);
	if (t == NULL) return 0;

	
	uint8_t  slcAddr = _pgmB(&t->sIdx);													// get the respective addresses
	uint16_t regAddr = cnlDefTypeAddr(cnl, lst, pIdx);

	// search through the sliceStr for even bytes in buffer
	for (uint8_t i = 0; i < len; i+=2) {
		void *x = memchr(&dDef.slPtr[slcAddr], buf[i], _pgmB(&t->sLen));				// find the byte in slcStr
		if ((uint16_t)x == 0) continue;													// got no result, try next
		pIdx = (uint8_t)((uint16_t)x - (uint16_t)&dDef.slPtr[slcAddr]);					// calculate the relative position
		//Serial << "x: " << pHexB(buf[i])  << ", ad: " << pIdx << '\n';
		setEeBy(regAddr+pIdx,buf[i+1]);													// write the bytes to the respective address
	}
	return 1;
}
uint8_t  HM::getRegAddr(uint8_t cnl, uint8_t lst, uint8_t pIdx, uint8_t addr, uint8_t len, uint8_t *buf) {

	s_cnlDefType *t = cnlDefbyList(cnl, lst);
	if (t == NULL) return 0;

	void *x = memchr(&dDef.slPtr[_pgmB(&t->sIdx)], addr, _pgmB(&t->sLen));				// search for the reg byte in slice string
	if ((uint16_t)x == 0) return 0;														// if not found return 0
	uint8_t xIdx = (uint8_t)((uint16_t)x - (uint16_t)&dDef.slPtr[_pgmB(&t->sIdx)]);				// calculate the base address
		
	uint16_t regAddr = cnlDefTypeAddr(cnl, lst, pIdx);									// get the base address in eeprom
	getEeBl(regAddr+xIdx,len,buf);														// get the respective block from eeprom
	return 1;																			// we are successfully
}
uint8_t  HM::doesListExist(uint8_t cnl, uint8_t lst) {
	if (cnlDefbyList(cnl,lst) == NULL) return 0;
	else return 1;
}
void     HM::getList3ByPeer(uint8_t cnl, uint8_t *peer) {
	// get the peer index
	uint8_t pIdx = getIdxByPeer(cnl, peer);												// get the index of the respective peer
	if (pIdx == 0xff) return;															// peer does not exist

	s_cnlDefType *t = cnlDefbyList(cnl, 3);
	if (t == NULL) return;


	uint16_t addr = cnlDefTypeAddr(cnl, 3, pIdx);										// get the respective eeprom address
	getEeBl(addr, _pgmB(&t->sLen), (void*)_pgmW(&t->pRegs));							// load content from eeprom
	
	#if defined(SM_DBG)																	// some debug message
	Serial << F("Loading list3 for cnl: ") << cnl << F(", peer: ") << pHex(peer,4) << '\n';
	#endif
}
void     HM::setListFromModule(uint8_t cnl, uint8_t peerIdx, uint8_t *data, uint8_t len) {
	s_cnlDefType *t = cnlDefbyPeer(cnl);
	
	uint16_t addr = cnlDefTypeAddr(cnl, _pgmB(&t->lst),peerIdx);
	if (addr == 0x00) return;
	setEeBl(addr, len, data);	
}

//- peer specific private functions
uint8_t  HM::getCnlByPeer(uint8_t *peer) {

	for (uint8_t i = 1; i <= dDef.cnlNbr; i++) {										// step through all channels

		uint8_t ret = getIdxByPeer(i,peer);												// searching inside the channel
		//Serial << "cnl: " << i << ", ret: " << ret << '\n';							// some debug message
		if (ret != 0xff) return i;														// if something found then return
	}
	return 0xff;																		// nothing found
}
uint8_t  HM::getIdxByPeer(uint8_t cnl, uint8_t *peer) {
	if (cnl == 0) return 0;																// channel 0 does not provide any peers

	s_cnlDefType *t = cnlDefbyPeer(cnl);
	if (t == NULL) return 0xff;
	
	for (uint8_t i = 0; i < _pgmB(&t->pMax); i++) {										// step through the peers
		if (getEeLo(cnlDefPeerAddr(cnl,i)) == *(uint32_t*)peer) return i;				// load peer from eeprom and compare
	}
	return 0xff;																		// out of range
}
uint8_t  HM::getPeerByIdx(uint8_t cnl, uint8_t idx, uint8_t *peer) {
	uint16_t eeAddr = cnlDefPeerAddr(cnl ,idx);											// get the address in eeprom
	if (eeAddr == 0) return 0;															// no address found, exit
	getEeBl(eeAddr,4,peer);																// copy the respective eeprom block to the pointer
	return 1;																			// done
}
uint8_t  HM::getFreePeerSlot(uint8_t cnl) {
	s_cnlDefType *t = cnlDefbyPeer(cnl);
	if (t == NULL) return 0xff;
	
	for (uint8_t i = 0; i < _pgmB(&t->pMax); i++) {										// step through all peers, given by pMax
		if (getEeLo(cnlDefPeerAddr(cnl,i)) == 0) return i;								// if we found an empty block then return the number
	}
	return 0xff;																		// no block found
}
uint8_t  HM::cntFreePeerSlot(uint8_t cnl) {
	//Serial << "idx:" << ret << ", pMax:" << _pgmB(&dDef.chPtr[ret].pMax) << '\n';
	s_cnlDefType *t = cnlDefbyPeer(cnl);
	if (t == NULL) return 0xff;
	
	uint8_t cnt = 0;
	for (uint8_t i = 0; i < _pgmB(&t->pMax); i++) {										// step through all peers, given by pMax
		if (getEeLo(cnlDefPeerAddr(cnl,i)) == 0) cnt++;									// increase counter if slot is empty
	}
	return cnt;																			// return the counter
}
uint8_t  HM::addPeer(uint8_t cnl, uint8_t *peer) {
	if (getIdxByPeer(cnl, peer) != 0xff) return 0xfe;									// check if peer already exists
	
	uint8_t idx = getFreePeerSlot(cnl);													// find a free slot
	if (idx == 0xff) return 0xfd;														// no free slot

	uint16_t eeAddr = cnlDefPeerAddr(cnl, idx);											// get the address in eeprom
	if (eeAddr == 0) return 0xff;														// address not found, exit
	
	setEeBl(eeAddr,4,peer);																// write to the slot
	return idx;
}
uint8_t  HM::remPeer(uint8_t cnl, uint8_t *peer) {
	uint8_t idx = getIdxByPeer(cnl, peer);												// get the idx of the peer
	if (idx == 0xff) return 0;															// peer not found, exit
	
	uint16_t eeAddr = cnlDefPeerAddr(cnl, idx);											// get the address in eeprom
	if (eeAddr == 0) return 0;															// address not found, exit
	
	setEeLo(eeAddr,0);																	// write a 0 to the given slot
	return 1;																			// everything is fine
}

//- cnlDefType specific functions
uint16_t HM::cnlDefTypeAddr(uint8_t cnl, uint8_t lst, uint8_t peerIdx) {
	s_cnlDefType *t = cnlDefbyList(cnl, lst);
	if (t == NULL) return 0;
	
	if ((peerIdx) && (peerIdx >= _pgmB(&t->pMax))) return 0;							// return if peer index is out of range
	return ee[0].regsDB + _pgmW(&t->pAddr) + (peerIdx * _pgmB(&t->sLen));				// calculate the starting address
}
uint16_t HM::cnlDefPeerAddr(uint8_t cnl, uint8_t peerIdx) {
	s_cnlDefType *t = cnlDefbyPeer(cnl);
	if (t == NULL) return 0;
	
	if ((peerIdx) && (peerIdx >= _pgmB(&t->pMax))) return 0;							// return if peer index is out of range
	return ee[0].peerDB + _pgmW(&t->pPeer) + (peerIdx * 4);								// calculate the starting address
}
s_cnlDefType* HM::cnlDefbyList(uint8_t cnl, uint8_t lst) {
	for (uint8_t i = 0; i < dDef.lstNbr; i++) {											// step through the list

		if ((cnl == _pgmB(&dDef.chPtr[i].cnl)) && (lst == _pgmB(&dDef.chPtr[i].lst)))
		return (s_cnlDefType*)&dDef.chPtr[i];											 // find the respective channel and list
	}
	return NULL;																		// nothing found
}
s_cnlDefType* HM::cnlDefbyPeer(uint8_t cnl) {
	for (uint8_t i = 0; i < dDef.lstNbr; i++) {											// step through the list

		if ((cnl == _pgmB(&dDef.chPtr[i].cnl)) && (_pgmB(&dDef.chPtr[i].pMax)))
		return (s_cnlDefType*)&dDef.chPtr[i];											// find the respective channel which includes a peer
	}
	return NULL;																		// nothing found
}


//- pure eeprom handling, i2c to be implemented
uint8_t  HM::getEeBy(uint16_t addr) {
	// todo: lock against writing
	// todo: extend range for i2c eeprom
	return eeprom_read_byte((uint8_t*)addr);
}
void     HM::setEeBy(uint16_t addr, uint8_t payload) {
	// todo: lock against reading
	// todo: extend range for i2c eeprom
	eeprom_write_byte((uint8_t*)addr,payload);
}
uint16_t HM::getEeWo(uint16_t addr) {
	// todo: lock against writing
	// todo: extend range for i2c eeprom
	return eeprom_read_word((uint16_t*)addr);
}
void     HM::setEeWo(uint16_t addr, uint16_t payload) {
	// todo: lock against reading
	// todo: extend range for i2c eeprom
	eeprom_write_word((uint16_t*)addr,payload);
}
uint32_t HM::getEeLo(uint16_t addr) {
	// todo: lock against writing
	// todo: extend range for i2c eeprom
	return eeprom_read_dword((uint32_t*)addr);
}
void     HM::setEeLo(uint16_t addr, uint32_t payload) {
	// todo: lock against reading
	// todo: extend range for i2c eeprom
	eeprom_write_dword((uint32_t*)addr,payload);
}
void     HM::getEeBl(uint16_t addr,uint8_t len,void *ptr) {
	// todo: lock against reading
	// todo: extend range for i2c eeprom
	eeprom_read_block((void*)ptr,(const void*)addr,len);
}
void     HM::setEeBl(uint16_t addr,uint8_t len,void *ptr) {
	// todo: lock against reading
	// todo: extend range for i2c eeprom
	eeprom_write_block((const void*)ptr,(void*)addr,len);
}
void     HM::clrEeBl(uint16_t addr, uint16_t len) {
	for (uint16_t l = 0; l < len; l++) {												// step through the bytes of eeprom
		setEeBy(addr+l, 0);																// and write a 0
	}
}

HM hm;
//- -----------------------------------------------------------------------------------------------------------------------

//- interrupt handling for interface communication module to AskSin library
void isrGDO0() {
	detachInterrupt(intGDO0.nbr);														// switch interrupt off
	intGDO0.ptr->recvInterrupt();														// call the interrupt handler of the active AskSin class
	attachInterrupt(intGDO0.nbr,isrGDO0,FALLING);										// switch interrupt on again
}
ISR( WDT_vect ) {
	wd_flag = 1;
}
