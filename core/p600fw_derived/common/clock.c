////////////////////////////////////////////////////////////////////////////////
// Common clock for sequencer and arpeggiator
////////////////////////////////////////////////////////////////////////////////

#include "clock.h"

#include "storage.h"

struct
{
	uint16_t counter,speed;
} clock;

void clock_setSpeed /* P8 patch 0003: was `inline` (MSVC exports no symbol) */(uint16_t speed)
{

	if(speed<1024)
		clock.speed=UINT16_MAX;
	else if(settings.syncMode==smInternal)
		clock.speed=exponentialCourse(speed,22000.0f,500.0f);
	else
        clock.speed=extClockDividers[(((uint32_t)speed)*16)>>16];
}

uint16_t clock_getSpeed /* P8 patch 0003: was `inline` (MSVC exports no symbol) */(void)
{
	return clock.speed;
}

uint16_t clock_getCounter /* P8 patch 0003: was `inline` (MSVC exports no symbol) */(void)
{
	return clock.counter;
}

void clock_reset /* P8 patch 0003: was `inline` (MSVC exports no symbol) */(void)
{
	clock.counter = INT16_MAX; // not UINT16_MAX, to avoid overflow

}

int8_t clock_update /* P8 patch 0003: was `inline` (MSVC exports no symbol) */(void)
{
	// speed management

	if(clock.speed==UINT16_MAX)
		return 0;

	++clock.counter;

	if(clock.counter<clock.speed)
		return 0;

	clock.counter=0;

	return 1;
}
