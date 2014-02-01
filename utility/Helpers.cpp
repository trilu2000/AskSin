//- -----------------------------------------------------------------------------------------------------------------------
// AskSin driver implementation
// 2013-08-03 <horst@diebittners.de> Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
//- helper functions ------------------------------------------------------------------------------------------------------
//- -----------------------------------------------------------------------------------------------------------------------

#include "Helpers.h"

extern uint16_t __bss_end, _pHeap_start;
extern void *__brkval;
uint16_t freeMem() {																	// shows free memory
	uint16_t free_memory;

	if((uint16_t)__brkval == 0)
	free_memory = ((uint16_t)&free_memory) - ((uint16_t)&__bss_end);
	else
	free_memory = ((uint16_t)&free_memory) - ((uint16_t)__brkval);

	return free_memory;
}

uint16_t crc16(uint16_t crc, uint8_t a) {
	uint16_t i;

	crc ^= a;
	for (i = 0; i < 8; ++i) {
		if (crc & 1)
		crc = (crc >> 1) ^ 0xA001;
	else
		crc = (crc >> 1);
	}
   return crc;
}

