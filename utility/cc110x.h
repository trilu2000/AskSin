//- -----------------------------------------------------------------------------------------------------------------------
// AskSin driver implementation
// 2013-08-03 <horst@diebittners.de> Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
//- CC1101 communication functions ----------------------------------------------------------------------------------------
//- with respect to http://culfw.de/culfw.html
//- -----------------------------------------------------------------------------------------------------------------------
#ifndef CC110X_H
#define CC110X_H

#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif

#include "utility/Serial.h"
#include "utility/Helpers.h"
#include <util/delay.h>

//#define CC_DBG

class CC110x {
  public://----------------------------------------------------------------------------------------------------------------
	uint8_t csPin, mosiPin, misoPin, sckPin, gdo0Pin, gdo0Int;							// pin definition for the module

	uint8_t crc_ok;																		// CRC OK for received message
	uint8_t rssi;																		// signal strength
	uint8_t lqi;																		// link quality

	// TRX868 communication functions
	void    config(uint8_t csP, uint8_t mosiP, uint8_t misoP, uint8_t sckP, uint8_t gdo0P, uint8_t int_gdo0); // set the respective pins
	void    init();																		// initialize CC1101
	uint8_t sendData(uint8_t *buf, uint8_t burst);										// send data packet via RF
	uint8_t receiveData(uint8_t *buf);													// read data packet from RX FIFO
	uint8_t detectBurst(void);															// detect burst signal, sleep while no signal, otherwise stay awake
	void    setPowerDownState(void);													// put CC1101 into power-down state
	uint8_t monitorStatus(void);

  //private://-------------------------------------------------------------------------------------------------------------
	#define CC1101_DATA_LEN			 60

	// some register definitions for TRX868 communication
	#define READ_SINGLE              0x80
	#define READ_BURST               0xC0
	#define WRITE_BURST              0x40												// type of transfers

	#define CC1101_CONFIG            0x80												// type of register
	#define CC1101_STATUS            0xC0

	#define CC1101_PATABLE           0x3E												// PATABLE address
	#define CC1101_TXFIFO            0x3F												// TX FIFO address
	#define CC1101_RXFIFO            0x3F												// RX FIFO address

	#define CC1101_SRES              0x30												// reset CC1101 chip
	#define CC1101_SFSTXON           0x31												// enable and calibrate frequency synthesizer (if MCSM0.FS_AUTOCAL=1). if in RX (with CCA): Go to a wait state where only the synthesizer is running (for quick RX / TX turnaround).
	#define CC1101_SXOFF             0x32												// turn off crystal oscillator
	#define CC1101_SCAL              0x33												// calibrate frequency synthesizer and turn it off. SCAL can be strobed from IDLE mode without setting manual calibration mode (MCSM0.FS_AUTOCAL=0)
	#define CC1101_SRX               0x34												// enable RX. perform calibration first if coming from IDLE and MCSM0.FS_AUTOCAL=1
	#define CC1101_STX               0x35												// in IDLE state: enable TX. perform calibration first if MCSM0.FS_AUTOCAL=1. if in RX state and CCA is enabled: only go to TX if channel is clear
	#define CC1101_SIDLE             0x36												// exit RX / TX, turn off frequency synthesizer and exit Wake-On-Radio mode if applicable
	#define CC1101_SWOR              0x38												// start automatic RX polling sequence (Wake-on-Radio) as described in Section 19.5 if WORCTRL.RC_PD=0
	#define CC1101_SPWD              0x39												// enter power down mode when CSn goes high
	#define CC1101_SFRX              0x3A												// flush the RX FIFO buffer. only issue SFRX in IDLE or RXFIFO_OVERFLOW states
	#define CC1101_SFTX              0x3B												// flush the TX FIFO buffer. only issue SFTX in IDLE or TXFIFO_UNDERFLOW states
	#define CC1101_SWORRST           0x3C												// reset real time clock to Event1 value
	#define CC1101_SNOP              0x3D												// no operation. may be used to get access to the chip status byte

	#define CC1101_PARTNUM           0x30												// status register, chip ID
	#define CC1101_VERSION           0x31												// chip ID
	#define CC1101_FREQEST           0x32												// frequency offset estimate from demodulator
	#define CC1101_LQI               0x33												// demodulator estimate for Link Quality
	#define CC1101_RSSI              0x34												// received signal strength indication
	#define CC1101_MARCSTATE         0x35												// main radio control state machine state
	#define CC1101_WORTIME1          0x36												// high byte of WOR Time
	#define CC1101_WORTIME0          0x37												// low byte of WOR Time
	#define CC1101_PKTSTATUS         0x38												// current GDOx status and packet status
	#define CC1101_VCO_VC_DAC        0x39												// current setting from PLL calibration module
	#define CC1101_TXBYTES           0x3A												// underflow and number of bytes
	#define CC1101_RXBYTES           0x3B												// overflow and number of bytes
	#define CC1101_RCCTRL1_STATUS    0x3C												// last RC oscillator calibration result
	#define CC1101_RCCTRL0_STATUS    0x3D												// last RC oscillator calibration result

	#define MARCSTATE_SLEEP          0x00
	#define MARCSTATE_IDLE           0x01
	#define MARCSTATE_XOFF           0x02
	#define MARCSTATE_VCOON_MC       0x03
	#define MARCSTATE_REGON_MC       0x04
	#define MARCSTATE_MANCAL         0x05
	#define MARCSTATE_VCOON          0x06
	#define MARCSTATE_REGON          0x07
	#define MARCSTATE_STARTCAL       0x08
	#define MARCSTATE_BWBOOST        0x09
	#define MARCSTATE_FS_LOCK        0x0A
	#define MARCSTATE_IFADCON        0x0B
	#define MARCSTATE_ENDCAL         0x0C
	#define MARCSTATE_RX             0x0D
	#define MARCSTATE_RX_END         0x0E
	#define MARCSTATE_RX_RST         0x0F
	#define MARCSTATE_TXRX_SWITCH    0x10
	#define MARCSTATE_RXFIFO_OFLOW   0x11
	#define MARCSTATE_FSTXON         0x12
	#define MARCSTATE_TX             0x13
	#define MARCSTATE_TX_END         0x14
	#define MARCSTATE_RXTX_SWITCH    0x15
	#define MARCSTATE_TXFIFO_UFLOW   0x16

	#define PA_LowPower              0x03												// PATABLE values
	#define PA_Normal                0x50												// PATABLE values
	#define PA_MaxPower			     0xC0

	// some macros for TRX868 communication
	#define wait_Miso()       while(digitalRead(misoPin))								// wait until SPI MISO line goes low
	#define cc1101_Select()   digitalWrite(csPin,LOW)									// select (SPI) CC1101
	#define cc1101_Deselect() digitalWrite(csPin,HIGH)									// deselect (SPI) CC1101

	// TRX868 communication functions
	uint8_t sendSPI(uint8_t val);														// send bytes to SPI interface
	void    cmdStrobe(uint8_t cmd);														// send command strobe to the CC1101 IC via SPI
	void    readBurst(uint8_t * buf, uint8_t regAddr, uint8_t len);						// read burst data from CC1101 via SPI
	void    writeBurst(uint8_t regAddr, uint8_t* buf, uint8_t len);						// write multiple registers into the CC1101 IC via SPI
	uint8_t readReg(uint8_t regAddr, uint8_t regType);									// read CC1101 register via SPI
	void    writeReg(uint8_t regAddr, uint8_t val);										// write single register into the CC1101 IC via SPI
};

#endif
