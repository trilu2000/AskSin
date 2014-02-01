#include <AskSinMain.h>

//- ----------------------------------------------------------------------------------------------------------------------
//- settings of HM device for HM class -----------------------------------------------------------------------------------
const uint8_t devParam[] PROGMEM = {
/* Firmware version 1 byte */  0x11,									// don't know for what it is good for
/* Model ID	        2 byte */  0x00, 0xA9,								// model ID, describes HM hardware. we should use high values due to HM starts from 0
/* Serial ID       10 byte */  'P','S','0','0','0','0','0','0','0','2', // serial ID, needed for pairing
/* Sub Type ID      1 byte */  0x40,									// not needed for FHEM, it's something like a group ID
/* Device Info      3 byte */  0x06, 0x00, 0x00							// describes device, not completely clear yet. includes amount of channels
};

HM::s_devParm dParm = {
/* device HM Id     3 byte */ {0x4F, 0xB7, 0x4A},						// very important, must be unique. identifier for the device in the network
/* send retries		1 byte */  3,										// how often a string should be send out until we get an answer
/* send timeout     2 byte */  700,										// time out for ACK handling
/* pointer to serial       */  devParam,
};


//- ----------------------------------------------------------------------------------------------------------------------
//- channel slice definition ---------------------------------------------------------------------------------------------
uint8_t sliceStr[] = {
	0x01,0x02,0x0a,0x0b,0x0c,0x18,
	0x04,0x08,0x09,
	0x01,
}; // 10 byte


//- ----------------------------------------------------------------------------------------------------------------------
//- Channel device config ------------------------------------------------------------------------------------------------
struct s_regDevL0 {
	// 0x01,0x02,0x0a,0x0b,0x0c,0x18,
	uint8_t  burstRx;                    // 0x01, s:0, e:0
	uint8_t                      :7;     //       l:0, s:7
	uint8_t  intKeyVisib         :1;     // 0x02, s:7, e:8
	uint8_t  pairCentral[3];             // 0x0a, s:0, e:0
	uint8_t  localResDis;                // 0x18, s:0, e:0
};

struct s_regChanL1 {
	// 0x04,0x08,0x09,
	uint8_t                      :4;     //       l:0, s:4
	uint8_t  longPress           :4;     // 0x04, s:4, e:8
	uint8_t  sign                :1;     // 0x08, s:0, e:1
	uint8_t                      :7;     //       l:7, s:0
	uint8_t  dblPress            :4;     // 0x09, s:0, e:4
	uint8_t                      :4;     //
};

struct s_regChanL4 {
	// 0x01,
	uint8_t  peerNeedsBurst      :1;     // 0x01, s:0, e:1
	uint8_t                      :6;     //
	uint8_t  expectAES           :1;     // 0x01, s:7, e:8
};

struct s_regDev {
	s_regDevL0 l0;
};

struct s_regChan {
	s_regChanL1 l1;
	s_regChanL4 l4;
};

struct s_regs {
	s_regDev ch0;
	s_regChan ch1;
	s_regChan ch2;
	s_regChan ch3;
	s_regChan ch4;
	s_regChan ch5;
	s_regChan ch6;
} regs; // 60 byte


//- ----------------------------------------------------------------------------------------------------------------------
//- channel device list table --------------------------------------------------------------------------------------------
const s_cnlDefType cnlDefType[] PROGMEM = {
	// cnl, lst, pMax, sIdx, sLen, pAddr, pPeer, *pRegs;																	// pointer to regs structure

	{0, 0, 0, 0x00, 6, 0x0000, 0x0000, (void*)&regs.ch0.l0},
	{1, 1, 0, 0x06, 3, 0x0006, 0x0000, (void*)&regs.ch1.l1},
	{1, 4, 6, 0x09, 1, 0x0009, 0x0000, (void*)&regs.ch1.l4},
	{2, 1, 0, 0x06, 3, 0x000f, 0x0000, (void*)&regs.ch2.l1},
	{2, 4, 6, 0x09, 1, 0x0012, 0x0018, (void*)&regs.ch2.l4},
	{3, 1, 0, 0x06, 3, 0x0018, 0x0000, (void*)&regs.ch3.l1},
	{3, 4, 6, 0x09, 1, 0x001b, 0x0030, (void*)&regs.ch3.l4},
	{4, 1, 0, 0x06, 3, 0x0021, 0x0000, (void*)&regs.ch4.l1},
	{4, 4, 6, 0x09, 1, 0x0024, 0x0048, (void*)&regs.ch4.l4},
	{5, 1, 0, 0x06, 3, 0x002a, 0x0000, (void*)&regs.ch5.l1},
	{5, 4, 6, 0x09, 1, 0x002d, 0x0060, (void*)&regs.ch5.l4},
	{6, 1, 0, 0x06, 3, 0x0033, 0x0000, (void*)&regs.ch6.l1},
	{6, 4, 6, 0x09, 1, 0x0036, 0x0078, (void*)&regs.ch6.l4},
}; // 143 byte


//- ----------------------------------------------------------------------------------------------------------------------
//- handover to AskSin lib -----------------------------------------------------------------------------------------------
HM::s_devDef dDef = {
	6, 13, sliceStr, cnlDefType,
}; // 6 byte

//- ----------------------------------------------------------------------------------------------------------------------
//- eeprom definition ----------------------------------------------------------------------------------------------------
// define start address  and size in eeprom for magicNumber, peerDB, regsDB, userSpace
HM::s_eeprom ee[] = {
	{0x0000, 0x0002, 0x0092, 0x00ce,},
	{0x0002, 0x0090, 0x003c, 0x0000,},
}; // 16 byte




//- -----------------------------------------------------------------------------------------------------------------------
// - defaults definitions -------------------------------------------------------------------------------------------------
const uint8_t regs01[] PROGMEM = {0x00,0x00,0x63,0x19,0x63};
const uint8_t regs02[] PROGMEM = {0xff,0xff,0xff};

const uint8_t regs03[] PROGMEM = {0x10,0x02,0x03,0x04};
const uint8_t regs04[] PROGMEM = {0x11,0x02,0x03,0x04};
const uint8_t regs05[] PROGMEM = {0x12,0x02,0x03,0x04};
const uint8_t regs06[] PROGMEM = {0x13,0x02,0x03,0x04};
const uint8_t regs07[] PROGMEM = {0x13,0x02,0x03,0x05};
const uint8_t regs08[] PROGMEM = {0x15,0x02,0x03,0x04};

const uint8_t regs09[] PROGMEM = {0x20,0x02,0x03,0x04,0x00,0x00,0x00,0x00,0x22,0x02,0x03,0x04,0x00,0x00,0x00,0x00,0x24,0x02,0x03,0x04,0x25,0x02,0x03,0x04};
const uint8_t regs10[] PROGMEM = {0x30,0x02,0x03,0x04,0x31,0x02,0x03,0x04,0x32,0x02,0x03,0x04,0x33,0x02,0x03,0x04,0x34,0x02,0x03,0x04,0x35,0x02,0x03,0x04};
const uint8_t regs11[] PROGMEM = {0x40,0x02,0x03,0x04,0x41,0x02,0x03,0x04,0x42,0x02,0x03,0x04,0x43,0x02,0x03,0x04,0x44,0x02,0x03,0x04,0x45,0x02,0x03,0x04};
const uint8_t regs12[] PROGMEM = {0x50,0x02,0x03,0x04,0x51,0x02,0x03,0x04,0x52,0x02,0x03,0x04,0x53,0x02,0x03,0x04,0x54,0x02,0x03,0x04,0x55,0x02,0x03,0x04};
const uint8_t regs13[] PROGMEM = {0x60,0x02,0x03,0x04,0x61,0x02,0x03,0x04,0x62,0x02,0x03,0x04,0x63,0x02,0x03,0x04,0x64,0x02,0x03,0x04,0x65,0x02,0x03,0x04};

s_defaultRegsTbl defaultRegsTbl[] = {
// peer(0) or regs(1), channel, list, peer index, len, pointer to payload
	{1, 0, 0, 0, 5, regs01},
//	{1, 6, 4, 0, 1, regs02},

//	{0, 1, 4, 0, 4, regs03},
//	{0, 1, 4, 1, 4, regs04},
//	{0, 1, 4, 2, 4, regs05},
//	{0, 1, 4, 3, 4, regs06},
//	{0, 1, 4, 4, 4, regs07},
//	{0, 1, 4, 5, 4, regs08},

//	{0, 2, 4, 0, 24, regs09},
//	{0, 3, 4, 0, 24, regs10},
//	{0, 4, 4, 0, 24, regs11},
//	{0, 5, 4, 0, 24, regs12},
//	{0, 6, 4, 0, 24, regs13},
};
HM::s_dtRegs dtRegs = {
// amount of lines in defaultRegsTbl[], pointer to defaultRegsTbl[]
	1, defaultRegsTbl
};

