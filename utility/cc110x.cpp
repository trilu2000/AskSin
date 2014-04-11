//- -----------------------------------------------------------------------------------------------------------------------
// AskSin driver implementation
// 2013-08-03 <horst@diebittners.de> Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
//- CC1101 communication functions ----------------------------------------------------------------------------------------
//- with respect to http://culfw.de/culfw.html
//- -----------------------------------------------------------------------------------------------------------------------

#include "cc110x.h"

void    CC110x::config(uint8_t csP, uint8_t mosiP, uint8_t misoP, uint8_t sckP, uint8_t gdo0P, uint8_t int_gdo0) {															// initialize CC1101
	csPin = csP;
	mosiPin = mosiP;
	misoPin = misoP;
	sckPin = sckP;
	gdo0Pin = gdo0P;
	int_gdo0 = gdo0Int;
}

void    CC110x::init(void) {															// initialize CC1101
	
	#if defined(CC_DBG)
	Serial << F("CC1101_init: ");
	#endif

	pinMode(csPin, OUTPUT);																// set pins for SPI communication
	pinMode(mosiPin, OUTPUT);
	pinMode(misoPin, INPUT);
	pinMode(sckPin, OUTPUT);
	pinMode(gdo0Pin, INPUT);															// config GDO0 as input

	digitalWrite(csPin, HIGH);															// SPI init
	digitalWrite(sckPin, HIGH);
	digitalWrite(mosiPin, LOW);

	SPCR = _BV(SPE) | _BV(MSTR);														// SPI speed = CLK/4

	cc1101_Deselect();																	// some deselect and selects to init the TRX868modul
	_delay_us(5);

	cc1101_Select();
	_delay_us(10);

	cc1101_Deselect();
	_delay_us(41);

	cmdStrobe(CC1101_SRES);																// send reset
	_delay_ms(10);

	#if defined(CC_DBG)
		Serial << '1';
	#endif

	static prog_uint8_t initVal[] PROGMEM = {											// define init settings for TRX868
		0x00, 0x2E,			// IOCFG2: tristate											// non inverted GDO2, high impedance tri state
		0x01, 0x2E,			// IOCFG1: tristate											// low output drive strength, non inverted GD=1, high impedance tri state
		0x02, 0x06,			// IOCFG0: packet CRC ok									// disable temperature sensor, non inverted GDO0, asserts when a sync word has been sent/received, and de-asserts at the end of the packet. in RX, the pin will also de-assert when a package is discarded due to address or maximum length filtering
		0x03, 0x0D,			// FIFOTHR: TX:9 / RX:56									// 0 ADC retention, 0 close in RX, TX FIFO = 9 / RX FIFO = 56 byte
		0x04, 0xE9,			// SYNC1													// Sync word
		0x05, 0xCA,			// SYNC0
		0x06, 0x3D,			// PKTLEN(x): 61											// packet length 61
		0x07, 0x0C,			// PKTCTRL1:												// PQT = 0, CRC auto flush = 1, append status = 1, no address check
		0x0B, 0x06,			// FSCTRL1:													// frequency synthesizer control
		0x0D, 0x21,			// FREQ2
		0x0E, 0x65,			// FREQ1
		0x0F, 0x6A,			// FREQ0
		0x10, 0xC8,			// MDMCFG4
		0x11, 0x93,			// MDMCFG3
		0x12, 0x03,			// MDMCFG2
		0x15, 0x34,			// DEVIATN
		0x16, 0x01,         // MCSM2
		0x17, 0x30,			// MCSM1: always go into IDLE
		0x18, 0x18,			// MCSM0
		0x19, 0x16,			// FOCCFG
		0x1B, 0x43,			// AGCTRL2
		//0x1E, 0x28,       // ..WOREVT1: tEVENT0 = 50 ms, RX timeout = 390 us
		//0x1F, 0xA0,		// ..WOREVT0:
		//0x20, 0xFB,		// ..WORCTRL: EVENT1 = 3, WOR_RES = 0
		0x21, 0x56,			// FREND1
		0x25, 0x00,
		0x26, 0x11,			// FSCAL0
		0x2D, 0x35,			// TEST1
		0x3E, 0xC3,			// ?
	};

	for (uint8_t i=0; i<sizeof(initVal); i++) {											// write init value to TRX868
		writeReg(pgm_read_byte(&initVal[i++]), pgm_read_byte(&initVal[i]));
	}

	#if defined(CC_DBG)
		Serial << '2';
	#endif

	cmdStrobe(CC1101_SCAL);																// calibrate frequency synthesizer and turn it off
	while (readReg(CC1101_MARCSTATE, CC1101_STATUS) != 1) {								// waits until module gets ready
		_delay_us(1);

		#if defined(CC_DBG)
			Serial << '.';
		#endif
	}

	#if defined(CC_DBG)
		Serial << '3';
	#endif

	writeReg(CC1101_PATABLE, PA_MaxPower);												// configure PATABLE
	cmdStrobe(CC1101_SRX);																// flush the RX buffer
	cmdStrobe(CC1101_SWORRST);															// reset real time clock

	#if defined(CC_DBG)
	Serial << F(" - ready\n");
	#endif
}

uint8_t CC110x::sendData(uint8_t *buf, uint8_t burst) {									// send data packet via RF

	// Going from RX to TX does not work if there was a reception less than 0.5
	// sec ago. Due to CCA? Using IDLE helps to shorten this period(?)
	//ccStrobe(CC1100_SIDLE);
	//uint8_t cnt = 0xff;
	//while(cnt-- && (ccStrobe( CC1100_STX ) & 0x70) != 2)
	//my_delay_us(10);
 	cmdStrobe(CC1101_SIDLE);															// go to idle mode
	cmdStrobe(CC1101_SFRX );															// flush RX buffer
	cmdStrobe(CC1101_SFTX );															// flush TX buffer

	//Serial << "tx\n";

	if (burst) {																		// BURST-bit set?
		cmdStrobe(CC1101_STX  );														// send a burst
		_delay_ms(360);																	// according to ELV, devices get activated every 300ms, so send burst for 360ms
		//Serial << "send burst\n";

	} else {
		_delay_ms(1);																	// wait a short time to set TX mode
	}

	writeBurst(CC1101_TXFIFO, buf, buf[0]+1);											// write in TX FIFO

	cmdStrobe(CC1101_SFRX);																// flush the RX buffer
	cmdStrobe(CC1101_STX);																// send a burst

	for(uint8_t i=0; i< 200;++i) {														// after sending out all bytes the chip should go automatically in RX mode
		if( readReg(CC1101_MARCSTATE, CC1101_STATUS) == MARCSTATE_RX)
			break;																		//now in RX mode, good
		if( readReg(CC1101_MARCSTATE, CC1101_STATUS) != MARCSTATE_TX) {
			break;																		//neither in RX nor TX, probably some error
		}
		_delay_us(10);
	}

	//uint8_t cnt = 0xff;
	//while(cnt-- && (sendSPI(CC1101_SRX) & 0x70) != 1)
	//delayMicroseconds(10);

	#if defined(CC_DBG)																	// some debug message
		Serial << F("<- ") << pHexL(&buf[0], buf[0]+1) << pTime();
	#endif

	//Serial << "rx\n";
	return true;
}

uint8_t CC110x::receiveData(uint8_t *buf) {												// read data packet from RX FIFO
	uint8_t rxBytes = readReg(CC1101_RXBYTES, CC1101_STATUS);							// how many bytes are in the buffer
	
	//Serial << rxBytes << ' ';

	if ((rxBytes & 0x7F) && !(rxBytes & 0x80)) {										// any byte waiting to be read and no overflow?
		buf[0] = readReg(CC1101_RXFIFO, CC1101_CONFIG);									// read data length

		if (buf[0] > CC1101_DATA_LEN)													// if packet is too long
			buf[0] = 0;																	// discard packet
		else {
			readBurst(&buf[1], CC1101_RXFIFO, buf[0]);									// read data packet
			readReg(CC1101_RXFIFO, CC1101_CONFIG);										// read RSSI

			uint8_t val = readReg(CC1101_RXFIFO, CC1101_CONFIG);						// read LQI and CRC_OK
			lqi = val & 0x7F;
			crc_ok = bitRead(val, 7);
		}
	} else {
		buf[0] = 0;																		// nothing to do, or overflow
	}

	cmdStrobe(CC1101_SFRX);																// flush Rx FIFO
	cmdStrobe(CC1101_SIDLE);															// enter IDLE state
	cmdStrobe(CC1101_SRX);																// back to RX state
	cmdStrobe(CC1101_SWORRST);															// reset real time clock
//	trx868.rfState = RFSTATE_RX;														// declare to be in Rx state

	#if defined(CC_DBG)																	// debug message, string should be short, otherwise program stops
	if (buf[0] > 0) Serial << pHexL(&buf[1], buf[0]) << pTime();
	#endif

	return buf[0];																		// return the data buffer
}

uint8_t CC110x::detectBurst(void) {														// wake up CC1101 from power down state
	// 10 7/10 5 in front of the received string; 33 after received string
	// 10 - 00001010 - sync word found
	// 7  - 00000111 - GDO0 = 1, GDO2 = 1
	// 5  - 00000101 - GDO0 = 1, GDO2 = 1
	// 33 - 00100001 - GDO0 = 1, preamble quality reached
	// 96 - 01100000 - burst sent
	// 48 - 00110000 - in receive mode
	//
	// Status byte table:
	//	0 current GDO0 value
	//	1 reserved
	//	2 GDO2
	//	3 sync word found
	//	4 channel is clear
	//	5 preamble quality reached
	//	6 carrier sense
	//	7 CRC ok
	//
	// possible solution for finding a burst is to check for bit 6, carrier sense

	// set RXTX module in receive mode
	cc1101_Select();																	// select CC1101
	wait_Miso();																		// wait until MISO goes low
	cc1101_Deselect();																	// deselect CC1101
	cmdStrobe(CC1101_SRX);																// set RX mode again
	_delay_ms(3);																		// wait a short time to set RX mode

	// todo: check carrier sense for 5ms to avoid wakeup due to normal transmition
	//Serial << "rx\n";
//	return bitRead(hm.cc.monitorStatus(),6);											// return the detected signal
	return bitRead(monitorStatus(),6);													// return the detected signal
}

void    CC110x::setPowerDownState() {													// put CC1101 into power-down state
	cmdStrobe(CC1101_SIDLE);															// coming from RX state, we need to enter the IDLE state first
	cmdStrobe(CC1101_SFRX);
	cmdStrobe(CC1101_SPWD);																// enter power down state
	//Serial << "pd\n";
}

uint8_t CC110x::monitorStatus() {
	return readReg(CC1101_PKTSTATUS, CC1101_STATUS);
}

uint8_t CC110x::sendSPI(uint8_t val) {													// send byte via SPI
	SPDR = val;																			// transfer byte via SPI
	while(!(SPSR & _BV(SPIF)));															// wait until SPI operation is terminated
	return SPDR;
}

void    CC110x::cmdStrobe(uint8_t cmd) {												// send command strobe to the CC1101 IC via SPI
	cc1101_Select();																	// select CC1101
	wait_Miso();																		// wait until MISO goes low
	sendSPI(cmd);																		// send strobe command
	cc1101_Deselect();																	// deselect CC1101
}

void    CC110x::writeBurst(uint8_t regAddr, uint8_t *buf, uint8_t len) {				// write multiple registers into the CC1101 IC via SPI
	cc1101_Select();																	// select CC1101
	wait_Miso();																		// wait until MISO goes low
	sendSPI(regAddr | WRITE_BURST);														// send register address
	for(uint8_t i=0 ; i<len ; i++) sendSPI(buf[i]);										// send value
	cc1101_Deselect();																	// deselect CC1101
}

void    CC110x::readBurst(uint8_t *buf, uint8_t regAddr, uint8_t len) {					// read burst data from CC1101 via SPI
	cc1101_Select();																	// select CC1101
	wait_Miso();																		// wait until MISO goes low
	sendSPI(regAddr | READ_BURST);														// send register address
	for(uint8_t i=0 ; i<len ; i++) buf[i] = sendSPI(0x00);								// read result byte by byte
	cc1101_Deselect();																	// deselect CC1101
}

uint8_t CC110x::readReg(uint8_t regAddr, uint8_t regType) {								// read CC1101 register via SPI
	cc1101_Select();																	// select CC1101
	wait_Miso();																		// wait until MISO goes low
	sendSPI(regAddr | regType);															// send register address
	uint8_t val = sendSPI(0x00);														// read result
	cc1101_Deselect();																	// deselect CC1101
	return val;
}

void    CC110x::writeReg(uint8_t regAddr, uint8_t val) {								// write single register into the CC1101 IC via SPI
	cc1101_Select();																	// select CC1101
	wait_Miso();																		// wait until MISO goes low
	sendSPI(regAddr);																	// send register address
	sendSPI(val);																		// send value
	cc1101_Deselect();																	// deselect CC1101
}
