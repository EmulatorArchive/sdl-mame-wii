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
#include <ogcsys.h>
#include "mame.h"

#define AUDIO_BUFFER 3840 // (48000 / 50) * 2 * 2 = 3840

static u8 audiobuffer[2][AUDIO_BUFFER] __attribute__((__aligned__(32)));
static u8 tmpbuffer[AUDIO_BUFFER];
static u8 whichab = 0;
static int audiotimeout = 0;
#define AUDIOTIMEOUT_LIMIT 8
static lwpq_t audioqueue;
static lwp_t audiothread;
static mutex_t audiomutex;
static BOOL killaudio = FALSE;

void osd_update_audio_stream(running_machine *machine, INT16 *buffer, int samples_this_frame)
{
	int bytes_to_copy = samples_this_frame * sizeof(INT16) * 2;

	if (bytes_to_copy > AUDIO_BUFFER)
		bytes_to_copy = AUDIO_BUFFER;

	LWP_MutexLock(audiomutex);
	audiotimeout = 0;
	memset(tmpbuffer, 0, AUDIO_BUFFER);
	memcpy(tmpbuffer, buffer, bytes_to_copy);
	LWP_MutexUnlock(audiomutex);
}

void *wii_audio_thread()
{
	while (!killaudio)
	{
		memset(audiobuffer[whichab], 0, AUDIO_BUFFER);
		LWP_MutexLock(audiomutex);
		audiotimeout++;
		if (audiotimeout > AUDIOTIMEOUT_LIMIT - 1)
		{
			memset(audiobuffer[whichab], 0, AUDIO_BUFFER);
			audiotimeout = AUDIOTIMEOUT_LIMIT;
		}
		else
		{
			memcpy(audiobuffer[whichab], tmpbuffer, AUDIO_BUFFER);
		}
		DCFlushRange(audiobuffer[whichab], AUDIO_BUFFER);
		LWP_MutexUnlock(audiomutex);
		LWP_ThreadSleep(audioqueue);
	}

	return (void *)0;
}

void osd_set_mastervolume(int attenuation)
{
	// TODO
}

static void wii_audio_mix()
{
	whichab ^= 1;
	AUDIO_InitDMA((u32) audiobuffer[whichab], AUDIO_BUFFER);
	LWP_ThreadSignal(audioqueue);
}

static void wii_audio_cleanup(running_machine *machine)
{
	AUDIO_StopDMA();
	LWP_MutexLock(audiomutex);
	audiotimeout = 0;
	memset(tmpbuffer, 0, AUDIO_BUFFER);
	LWP_MutexUnlock(audiomutex);
}

void wii_setup_audio()
{
	LWP_MutexInit(&audiomutex, 0);
	LWP_InitQueue(&audioqueue);
	LWP_CreateThread(&audiothread, wii_audio_thread, NULL, NULL, 0, 70);

	AUDIO_Init(NULL);
}

void wii_init_audio(running_machine *machine)
{
	u32 sample_rate = (machine->sample_rate == 32000) ? AI_SAMPLERATE_32KHZ : AI_SAMPLERATE_48KHZ;

	memset(audiobuffer[0], 0, AUDIO_BUFFER);
	memset(audiobuffer[1], 0, AUDIO_BUFFER);
	memset(tmpbuffer,      0, AUDIO_BUFFER);
	DCFlushRange(audiobuffer[0], AUDIO_BUFFER);
	DCFlushRange(audiobuffer[1], AUDIO_BUFFER);

	add_exit_callback(machine, wii_audio_cleanup);

	AUDIO_SetDSPSampleRate(sample_rate);
	AUDIO_RegisterDMACallback(wii_audio_mix);
	AUDIO_InitDMA((u32) audiobuffer[whichab], AUDIO_BUFFER);
	AUDIO_StartDMA();
}

void wii_shutdown_audio()
{
	void *status;
	AUDIO_StopDMA();
	AUDIO_RegisterDMACallback(NULL);
	killaudio = TRUE;
	LWP_ThreadSignal(audioqueue);
	LWP_JoinThread(audiothread, &status);
	LWP_CloseQueue(audioqueue);
	LWP_MutexDestroy(audiomutex);
}
