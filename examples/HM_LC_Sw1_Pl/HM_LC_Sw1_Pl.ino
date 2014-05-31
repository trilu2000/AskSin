#define SER_DBG

//- load library's --------------------------------------------------------------------------------------------------------
#include "Register.h"																	// configuration sheet
//#include <Buttons.h>																	// remote buttons library
#include <Relay.h>																		// relay library

//- serial communication --------------------------------------------------------------------------------------------------
const uint8_t helptext1[] PROGMEM = {													// help text for serial console
	"\n"
	"Available commands:" "\n"
	"  p                - start pairing with master" "\n"
	"  r                - reset device" "\n"
	"  b[0]  b[n]  s    - send a string, b[0] is length (50 bytes max)" "\n"
	"\n"
	"  c                - print configuration" "\n"
	"\n"
	"  $nn for HEX input (e.g. $AB,$AC ); b[] = byte, i[]. = integer " "\n"
};
#ifdef SER_DBG
const InputParser::Commands cmdTab[] PROGMEM = {
	{ 'p', 0, sendPairing },
	{ 'r', 0, resetDevice },
	{ 's', 1, sendCmdStr },
	{ 'b', 1, buttonSend },
	{ 'c', 0, printConfig },
	{ 0 }
};
InputParser parser (50, cmdTab);
#endif


//- homematic communication -----------------------------------------------------------------------------------------------
HM::s_jumptable jTbl[] = {																// jump table for HM communication
	// byte3, byte10, byte11, function to call											// 0xff means - any byte
//	{ 0x01, 0xff, 0x0e, HM_Status_Request },
	{ 0x11, 0x04, 0x00, HM_Reset_Cmd },
//	{ 0x01, 0xff, 0x06, HM_Config_Changed },
//	{ 0x40, 0xff, 0xff, HM_Remote_Event },
	{ 0x00 }
};
//Buttons button[1];																		// declare remote button object
Relay   relay[1];																		// declare the relay object

//- main functions --------------------------------------------------------------------------------------------------------
void setup() {
#ifdef SER_DBG
	Serial.begin(57600);																// serial setup
	Serial << F("Starting sketch...\n");												// ...and some information
	Serial << pCharPGM(helptext1) << '\n';
	Serial << F("freeMem: ") << freeMem() << F(" byte") <<'\n';
#endif
	
	hm.cc.config(10,11,12,13,2,0);														// CS, MOSI, MISO, SCK, GDO0, Interrupt

	hm.statusLed.config(4, 5);															// configure the status led pin
	hm.statusLed.set(STATUSLED_BOTH, STATUSLED_MODE_BLINKFAST, 3);

	//hm.battery.config(0,0,1000);														// set battery measurement
	//hm.battery.setVoltage(31);															// voltage to 3.1 volt

	hm.setPowerMode(0);																	// power mode for HM device
	hm.init();																			// initialize the hm module

	//button[0].regInHM(0,&hm);															// register buttons in HM per channel, handover HM class pointer
	//button[0].config(A2, NULL);															// configure button on specific pin and handover a function pointer to the main sketch

	relay[0].regInHM(1,&hm);															// register relay class in HM to respective channel	
	relay[0].config(&initRelay,&switchRelay,2,2);										// configure the relay to monostable, therefore only one HW pin needed
	
	Serial << "\npair: " << pHex(regs.ch0.l0.pairCentral,3) << '\n';	
}

void loop() {
#ifdef SER_DBG
	parser.poll();																		// handle serial input from console
#endif

	hm.poll();																			// poll the HM communication
}

void initRelay() {
	digitalWrite(6,0);
	digitalWrite(7,0);
	digitalWrite(8,0);
	digitalWrite(9,0);

	pinMode(6,OUTPUT);
	pinMode(7,OUTPUT);
	pinMode(8,OUTPUT);
	pinMode(9,OUTPUT);
}
void switchRelay(uint8_t on) {
	if (on) {
		PORTB |= bit(0) | bit(1);
		delay(20);
		PORTB &= ~ (bit(0) | bit(1));

	} else {
		PORTD |= bit(6) | bit(7);
		delay(20);
		PORTD &= ~ (bit(6) | bit(7));
		
	}
}


//- HM functions ----------------------------------------------------------------------------------------------------------
void HM_Status_Request(uint8_t *data, uint8_t len) {
//	Serial << F("status request, data: ") << pHex(data,len) << '\n';
}
void HM_Reset_Cmd(uint8_t *data, uint8_t len) {
//	Serial << F("reset, data: ") << pHex(data,len) << '\n';
	hm.send_ACK();																		// send an ACK
	if (data[0] == 0) hm.reset();														// do a reset only if channel is 0
}
void HM_Config_Changed(uint8_t *data, uint8_t len) {
//	Serial << F("config changed, data: ") << pHex(data,len) << '\n';
}
void HM_Remote_Event(uint8_t *data, uint8_t len) {
//	Serial << F("remote event, data: ") << pHex(data,len) << '\n';
}


//- config functions ------------------------------------------------------------------------------------------------------
#ifdef SER_DBG
void sendPairing() {																	// send the first pairing request
	hm.startPairing();
}
void resetDevice() {
	Serial << F("reset device, clear eeprom...\n");
	hm.reset();
	Serial << F("reset done\n");
}
void sendCmdStr() {																		// reads a sndStr from console and put it in the send queue
	memcpy(hm.send.data,parser.buffer,parser.count());									// take over the parsed byte data
	Serial << F("s: ") << pHexL(hm.send.data, hm.send.data[0]+1) << '\n';				// some debug string
	hm.send_out();																		// fire to send routine
}
void buttonSend() {
	uint8_t cnl, lpr;
	parser >> cnl >> lpr;
	
	Serial << "button press, cnl: " << cnl << ", long press: " << lpr << '\n';			// some debug message
	hm.sendPeerREMOTE(cnl,lpr,0);														// parameter: button/channel, long press, battery
}
void printConfig() {
	hm.printConfig();
}
#endif





