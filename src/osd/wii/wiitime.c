//============================================================
//
//  Copyright (c) 1996-2009, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//  MAME Wii by Toad King
//
//============================================================

#include "osdepend.h"
#include <ogc/lwp_watchdog.h>
#include <unistd.h>


//============================================================
//  osd_ticks
//============================================================

osd_ticks_t osd_ticks(void)
{
	return gettime();
}


//============================================================
//  osd_ticks_per_second
//============================================================

osd_ticks_t osd_ticks_per_second(void)
{
	return secs_to_ticks(1);
}


//============================================================
//  osd_sleep
//============================================================

void osd_sleep(osd_ticks_t duration)
{
	UINT32 msec;

	// convert to milliseconds, rounding down
	msec = ticks_to_millisecs(duration);

	// only sleep if at least 2 full milliseconds
	if (msec >= 2)
	{
			// take a couple of msecs off the top for good measure
			msec -= 2;
			usleep(msec*1000);
	}
}
