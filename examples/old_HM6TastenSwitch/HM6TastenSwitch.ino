//- load library's --------------------------------------------------------------------------------------------------------
#include "Register.h"																	// configuration sheet
#include <Buttons.h>																	// remote buttons library
#include <Dummy.h>

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
const InputParser::Commands cmdTab[] PROGMEM = {
	{ 'p', 0, sendPairing },
	{ 'r', 0, resetDevice },
	{ 's', 1, sendCmdStr },
	{ 'b', 1, buttonSend },
	{ 'c', 0, printConfig },
	{ 0 }
};
InputParser parser (50, cmdTab);


//- homematic communication -----------------------------------------------------------------------------------------------
HM::s_jumptable jTbl[] = {																// jump table for HM communication
	// byte3, byte10, byte11, function to call											// 0xff means - any byte
	{ 0x01, 0xff, 0x0e, HM_Status_Request },
	{ 0x11, 0x04, 0x00, HM_Reset_Cmd },
	{ 0x01, 0xff, 0x06, HM_Config_Changed },
	{ 0x00 }
};
Buttons button[7];																		// declare remote button object
Dummy dummy;

//- main functions --------------------------------------------------------------------------------------------------------
void setup() {
	Serial.begin(57600);																// serial setup
	Serial << F("Starting sketch...\n");												// ...and some information
	Serial << pCharPGM(helptext1) << '\n';
	Serial << F("freeMem: ") << freeMem() << F(" byte") <<'\n';
	
	hm.cc.config(10,11,12,13,2,0);														// CS, MOSI, MISO, SCK, GDO0, Interrupt

	hm.statusLed.config(4, 6);															// configure the status led pin
	hm.statusLed.set(STATUSLED_BOTH, STATUSLED_MODE_BLINKFAST, 3);

	hm.battery.config(1,0,1000);														// set battery measurement
	hm.battery.setVoltage(31);															// voltage to 3.1 volt

	hm.setPowerMode(0);																	// power mode for HM device
	hm.init();																			// initialize the hm module

	buttonSetup();																		// jump in the button setup function

	dummy.regInHM(1,&hm);
	Serial << "\npair: " << pHex(regs.ch0.l0.pairCentral,3) << '\n';	

}

void loop() {
	parser.poll();																		// handle serial input from console
	hm.poll();																			// poll the HM communication
	button[0].poll();																	// poll the buttons
	button[1].poll();
	button[2].poll();
	button[3].poll();
	button[4].poll();
	button[5].poll();
	button[6].poll();
}


//- HM functions ----------------------------------------------------------------------------------------------------------
void HM_Status_Request(uint8_t *data, uint8_t len) {
	// message from master to client while requesting the channel specific status
	// client has to send an INFO_ACTUATOR_MESSAGE with the current status of the requested channel
	// there is no payload; data[0] could be ignored
	// Serial << F("\nStatus_Request; cnl: ") << pHexB(cnl) << F(", data: ") << pHex(data,len) << "\n\n";

	// user code here...
	//hm.sendInfoActuatorStatus(cnl, 0, 0);
}
void HM_Reset_Cmd(uint8_t *data, uint8_t len) {
	//Serial << F("\nReset_Cmd; cnl: ") << pHexB(cnl) << F(", data: ") << pHex(data,len) << "\n\n";
	hm.send_ACK();																		// send an ACK
	if (data[0] == 0) hm.reset();															// do a reset only if channel is 0
}
void HM_Config_Changed(uint8_t *data, uint8_t len) {
	//Serial << "ch1: long:" << 300+(regMC.ch1.l1.longPress*100) << ", dbl:" << (regMC.ch1.l1.dblPress*100) << '\n';

	buttonSetup();
	Serial << F("config changed, data: ") << pHex(data,len) << '\n';
}

void buttonSetup() {
	// button 0 for channel 0 for send pairing string, and double press for reseting device config
	button[0].config(0, 8, 0, 5000, 5000, &buttonEvent);
	button[1].config(1, A0, (regs.ch1.l1.dblPress * 100), 300 + (regs.ch1.l1.longPress * 100), 1000, &buttonEvent);
	button[2].config(2, A1, (regs.ch2.l1.dblPress * 100), 300 + (regs.ch2.l1.longPress * 100), 1000, &buttonEvent);
	button[3].config(3, A2, (regs.ch3.l1.dblPress * 100), 300 + (regs.ch3.l1.longPress * 100), 1000, &buttonEvent);
	button[4].config(4, A3, (regs.ch4.l1.dblPress * 100), 300 + (regs.ch4.l1.longPress * 100), 1000, &buttonEvent);
	button[5].config(5, A4, (regs.ch5.l1.dblPress * 100), 300 + (regs.ch5.l1.longPress * 100), 1000, &buttonEvent);
	button[6].config(6, A5, (regs.ch6.l1.dblPress * 100), 300 + (regs.ch6.l1.longPress * 100), 1000, &buttonEvent);
}
void buttonEvent(uint8_t idx, uint8_t state) {
	// possible events of this function:
	//   0 - short key press
	//   1 - double short key press
	//   2 - long key press
	//   3 - repeated long key press
	//   4 - end of long key press
	//   5 - double long key press
	//   6 - time out for a double long

	hm.stayAwake(2000);																	// overcome the problem of not getting a long repeated key press
	if (state == 255) return;															// was only a wake up message
	Serial << "i:" << idx << ", s:" << state << '\n';									// some debug message

	// channel device
	if (idx == 0) {
		if (state == 0) hm.startPairing();												// short key press, send pairing string
		if (state == 2) hm.statusLed.set(STATUSLED_2, STATUSLED_MODE_BLINKSFAST);		// long key press could mean, you like to go for reseting the device
		if (state == 6) hm.statusLed.set(STATUSLED_2, STATUSLED_MODE_OFF);				// time out for double long, stop slow blinking
		if (state == 5) hm.reset();														// double long key press, reset the device
	}

	// channel 1 - 6
	if ((idx >= 1) && (idx <= 6)) {

		if ((state == 0) || (state == 1)) hm.sendPeerREMOTE(idx, 0, 0);					// short key or double short key press detected
		if ((state == 2) || (state == 3)) hm.sendPeerREMOTE(idx, 1, 0);					// long or repeated long key press detected
		if (state == 4) hm.sendPeerREMOTE(idx, 2, 0);									// end of long or repeated long key press detected
	}
}


//- config functions ------------------------------------------------------------------------------------------------------
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






