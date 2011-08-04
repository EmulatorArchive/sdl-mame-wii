//============================================================
//
//  Copyright (c) 1996-2009, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//  MAME Wii by Toad King
//
//============================================================

#include "osdepend.h"
#include "wiiaudio.h"
#include <gccore.h>
#include "mame.h"

#define AUDIO_BUFFER 2048

static u8 audiobuffer[2][AUDIO_BUFFER] __attribute__((__aligned__(32)));
static u8 whichab = 0;

void osd_update_audio_stream(running_machine *machine, INT16 *buffer, int samples_this_frame)
{
	int buf = whichab;
	int bytes_to_copy = samples_this_frame * sizeof(INT16) * 2;
	
	if (bytes_to_copy > AUDIO_BUFFER)
		bytes_to_copy = AUDIO_BUFFER;

	memcpy(audiobuffer[buf], buffer, bytes_to_copy);
	DCFlushRange(audiobuffer[buf], AUDIO_BUFFER);
}

void osd_set_mastervolume(int attenuation)
{
	// TODO
}

static void wii_audio_mix()
{
	whichab ^= 1;
	AUDIO_InitDMA((u32) audiobuffer[whichab], AUDIO_BUFFER);
}

void wii_init_audio(running_machine *machine)
{
	u32 sample_rate = (machine->sample_rate == 32000) ? AI_SAMPLERATE_32KHZ : AI_SAMPLERATE_48KHZ;

	AUDIO_Init(NULL);
	AUDIO_SetDSPSampleRate(sample_rate);

	memset(audiobuffer[0], 0, AUDIO_BUFFER);
	memset(audiobuffer[1], 0, AUDIO_BUFFER);

	AUDIO_RegisterDMACallback(wii_audio_mix);
	AUDIO_StartDMA();
}

void wii_shutdown_audio()
{
	AUDIO_StopDMA();
}
