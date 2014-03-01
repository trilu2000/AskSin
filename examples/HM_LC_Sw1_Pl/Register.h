#include <AskSinMain.h>
//1A 2B 84 00 1F B7 4A 00 00 00 15 00 6C 4B 45 51 30 32 33 37 33 39 36 10 41 01 00
//- ----------------------------------------------------------------------------------------------------------------------
//- settings of HM device for HM class -----------------------------------------------------------------------------------
const uint8_t devParam[] PROGMEM = {
/* Firmware version 1 byte */  0x15,									// don't know for what it is good for
/* Model ID	        2 byte */  0x00, 0x11,								// model ID, describes HM hardware. we should use high values due to HM starts from 0
/* Serial ID       10 byte */  'T','L','U','0','0','0','2','0','0','2', // serial ID, needed for pairing
/* Sub Type ID      1 byte */  0x10,									// not needed for FHEM, it's something like a group ID
/* Device Info      3 byte */  0x01, 0x01, 0x00							// describes device, not completely clear yet. includes amount of channels
};

HM::s_devParm dParm = {
/* device HM Id     3 byte */ {0xF4, 0xB6, 0x5C},						// very important, must be unique. identifier for the device in the network
/* send retries		1 byte */  3,										// how often a string should be send out until we get an answer
/* send timeout     2 byte */  700,										// time out for ACK handling
/* pointer to serial       */  devParam,
};


HM::s_modtable modTbl[] = {
	{0,0,(s_mod_dlgt)NULL},
	{0,0,(s_mod_dlgt)NULL},
}; // 24 byte

//- ----------------------------------------------------------------------------------------------------------------------
//- channel slice definition ---------------------------------------------------------------------------------------------
uint8_t sliceStr[] = {
	0x02,0x05,0x0a,0x0b,0x0c,0x12,
	0x08,
	0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,
}; // 29 byte


//- ----------------------------------------------------------------------------------------------------------------------
//- Channel device config ------------------------------------------------------------------------------------------------
struct s_regDevL0 {
	// 0x02,0x05,0x0a,0x0b,0x0c,0x12,
	uint8_t                      :7;     //       l:0, s:7
	uint8_t  intKeyVisib         :1;     // 0x02, s:7, e:8
	uint8_t                      :6;     //       l:0, s:6
	uint8_t  ledMode             :2;     // 0x05, s:6, e:8
	uint8_t  pairCentral[3];             // 0x0a, s:0, e:0
	uint8_t  lowBatLimitBA;              // 0x12, s:0, e:0
};

struct s_regChanL1 {
	// 0x08,
	uint8_t  sign                :1;     // 0x08, s:0, e:1
	uint8_t                      :7;     //
};

struct s_regChanL3 {
	// 0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,
	uint8_t  shCtDlyOn           :4;     // 0x02, s:0, e:4
	uint8_t  shCtDlyOff          :4;     // 0x02, s:4, e:8
	uint8_t  shCtOn              :4;     // 0x03, s:0, e:4
	uint8_t  shCtOff             :4;     // 0x03, s:4, e:8
	uint8_t  shCtValLo;                  // 0x04, s:0, e:0
	uint8_t  shCtValHi;                  // 0x05, s:0, e:0
	uint8_t  shOnDly;                    // 0x06, s:0, e:0
	uint8_t  shOnTime;                   // 0x07, s:0, e:0
	uint8_t  shOffDly;                   // 0x08, s:0, e:0
	uint8_t  shOffTime;                  // 0x09, s:0, e:0
	uint8_t  shActionType        :2;     // 0x0a, s:0, e:2
	uint8_t                      :4;     //
	uint8_t  shOffTimeMode       :1;     // 0x0a, s:6, e:7
	uint8_t  shOnTimeMode        :1;     // 0x0a, s:7, e:8
	uint8_t  shSwJtOn            :4;     // 0x0b, s:0, e:4
	uint8_t  shSwJtOff           :4;     // 0x0b, s:4, e:8
	uint8_t  shSwJtDlyOn         :4;     // 0x0c, s:0, e:4
	uint8_t  shSwJtDlyOff        :4;     // 0x0c, s:4, e:8
	uint8_t  lgCtDlyOn           :4;     // 0x82, s:0, e:4
	uint8_t  lgCtDlyOff          :4;     // 0x82, s:4, e:8
	uint8_t  lgCtOn              :4;     // 0x83, s:0, e:4
	uint8_t  lgCtOff             :4;     // 0x83, s:4, e:8
	uint8_t  lgCtValLo;                  // 0x84, s:0, e:0
	uint8_t  lgCtValHi;                  // 0x85, s:0, e:0
	uint8_t  lgOnDly;                    // 0x86, s:0, e:0
	uint8_t  lgOnTime;                   // 0x87, s:0, e:0
	uint8_t  lgOffDly;                   // 0x88, s:0, e:0
	uint8_t  lgOffTime;                  // 0x89, s:0, e:0
	uint8_t  lgActionType        :2;     // 0x8a, s:0, e:2
	uint8_t                      :3;     //
	uint8_t  lgMultiExec         :1;     // 0x8a, s:5, e:6
	uint8_t  lgOffTimeMode       :1;     // 0x8a, s:6, e:7
	uint8_t  lgOnTimeMode        :1;     // 0x8a, s:7, e:8
	uint8_t  lgSwJtOn            :4;     // 0x8b, s:0, e:4
	uint8_t  lgSwJtOff           :4;     // 0x8b, s:4, e:8
	uint8_t  lgSwJtDlyOn         :4;     // 0x8c, s:0, e:4
	uint8_t  lgSwJtDlyOff        :4;     // 0x8c, s:4, e:8
};

struct s_regDev {
	s_regDevL0 l0;
};

struct s_regChan {
	s_regChanL1 l1;
	s_regChanL3 l3;
};

struct s_regs {
	s_regDev ch0;
	s_regChan ch1;
} regs; // 139 byte


//- ----------------------------------------------------------------------------------------------------------------------
//- channel device list table --------------------------------------------------------------------------------------------
s_cnlDefType cnlDefType[] PROGMEM = {
	// cnl, lst, pMax, sIdx, sLen, pAddr, pPeer, *pRegs;

	{0, 0, 0, 0x00, 6, 0x0000, 0x0000, (void*)&regs.ch0.l0},
	{1, 1, 0, 0x06, 1, 0x0006, 0x0000, (void*)&regs.ch1.l1},
	{1, 3, 6, 0x07, 22, 0x0007, 0x0000, (void*)&regs.ch1.l3},
}; // 33 byte


//- ----------------------------------------------------------------------------------------------------------------------
//- handover to AskSin lib -----------------------------------------------------------------------------------------------
HM::s_devDef dDef = {
	1, 3, sliceStr, cnlDefType,
}; // 6 byte


//- ----------------------------------------------------------------------------------------------------------------------
//- eeprom definition ----------------------------------------------------------------------------------------------------
// define start address  and size in eeprom for magicNumber, peerDB, regsDB, userSpace
HM::s_eeprom ee[] = {
	{0x0000, 0x0002, 0x001a, 0x00a5,},
	{0x0002, 0x0018, 0x008b, 0x0000,},
}; // 16 byte


//- -----------------------------------------------------------------------------------------------------------------------
// - defaults definitions -------------------------------------------------------------------------------------------------
const uint8_t regs01[] PROGMEM = {0x00,0x00,0x63,0x19,0x63};
//const uint8_t regs02[] PROGMEM = {0xff,0xff,0xff};

const uint8_t regs03[] PROGMEM = {0x1f,0xa6,0x5c,0x06};
const uint8_t regs04[] PROGMEM = {0x1f,0xa6,0x5c,0x05};
//const uint8_t regs05[] PROGMEM = {0x12,0x02,0x03,0x04};
//const uint8_t regs06[] PROGMEM = {0x13,0x02,0x03,0x04};
//const uint8_t regs07[] PROGMEM = {0x13,0x02,0x03,0x05};
//const uint8_t regs08[] PROGMEM = {0x15,0x02,0x03,0x04};

//const uint8_t regs09[] PROGMEM = {0x20,0x02,0x03,0x04,0x00,0x00,0x00,0x00,0x22,0x02,0x03,0x04,0x00,0x00,0x00,0x00,0x24,0x02,0x03,0x04,0x25,0x02,0x03,0x04};
//const uint8_t regs10[] PROGMEM = {0x30,0x02,0x03,0x04,0x31,0x02,0x03,0x04,0x32,0x02,0x03,0x04,0x33,0x02,0x03,0x04,0x34,0x02,0x03,0x04,0x35,0x02,0x03,0x04};
//const uint8_t regs11[] PROGMEM = {0x40,0x02,0x03,0x04,0x41,0x02,0x03,0x04,0x42,0x02,0x03,0x04,0x43,0x02,0x03,0x04,0x44,0x02,0x03,0x04,0x45,0x02,0x03,0x04};
//const uint8_t regs12[] PROGMEM = {0x50,0x02,0x03,0x04,0x51,0x02,0x03,0x04,0x52,0x02,0x03,0x04,0x53,0x02,0x03,0x04,0x54,0x02,0x03,0x04,0x55,0x02,0x03,0x04};
//const uint8_t regs13[] PROGMEM = {0x60,0x02,0x03,0x04,0x61,0x02,0x03,0x04,0x62,0x02,0x03,0x04,0x63,0x02,0x03,0x04,0x64,0x02,0x03,0x04,0x65,0x02,0x03,0x04};

s_defaultRegsTbl defaultRegsTbl[] = {
// peer(0) or regs(1), channel, list, peer index, len, pointer to payload
	{1, 0, 0, 0, 5, regs01},
//	{1, 1, 1, 0, 3, regs02},

	{0, 1, 4, 0, 4, regs03},
	{0, 1, 4, 1, 4, regs04},
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
	0, defaultRegsTbl
};

