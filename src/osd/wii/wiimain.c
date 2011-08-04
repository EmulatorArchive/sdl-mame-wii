//============================================================
//
//  Copyright (c) 1996-2009, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//  MAME Wii by Toad King
//
//============================================================

#include "osdepend.h"
#include <ogcsys.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <unistd.h>
#include "wiiaudio.h"
#include "wiiinput.h"
#include "wiimisc.h"
#include "wiivideo.h"
#include "render.h"
#include "clifront.h"
#include "mame.h"
#include "emuopts.h"
#include "options.h"


//============================================================
//  CONSTANTS
//============================================================

// we fake a keyboard with the following keys
enum
{
	KEY_ESCAPE,
	KEY_P1_START,
	KEY_BUTTON_1,
	KEY_BUTTON_2,
	KEY_BUTTON_3,
	KEY_JOYSTICK_U,
	KEY_JOYSTICK_D,
	KEY_JOYSTICK_L,
	KEY_JOYSTICK_R,
	KEY_TOTAL
};


//============================================================
//  GLOBALS
//============================================================

// a single rendering target
static render_target *our_target;

// a single input device
static input_device *keyboard_device;

// the state of each key
static UINT8 keyboard_state[KEY_TOTAL];

static running_machine *current_machine = NULL;
static BOOL shutting_down = FALSE;

static const options_entry wii_mame_options[] =
{
	{ "initpath", ".;/mame", 0, "path to ini files" },
	{ NULL, NULL, OPTION_HEADER, "WII OPTIONS" },
	{ "safearea(0.01-1)", "1.0", 0, "Adjust video for safe areas on older TVs (.9 or .85 are usually good values)" },
	{ NULL }
};

//============================================================
//  FUNCTION PROTOTYPES
//============================================================

static INT32 keyboard_get_state(void *device_internal, void *item_internal);


static void wii_shutdown()
{
	if (current_machine == NULL)
		return;

	shutting_down = TRUE;

	// mame_schedule_exit only returns to the game select screen if done in
	// game, so fake being out of agame before doing it
	options_set_string(mame_options(), OPTION_GAMENAME, "", OPTION_PRIORITY_CMDLINE);
	mame_schedule_exit(current_machine);
}

static void wii_reset()
{
	if (current_machine != NULL)
		mame_schedule_exit(current_machine);
}

//============================================================
//  main
//============================================================

int main(int argc, char *argv[])
{
	const char *nargv[] = {
		""
	};
	int nargc = 1;
	int ret;
	L2Enhance();
	WPAD_Init();
	PAD_Init();

	WPAD_SetPowerButtonCallback((WPADShutdownCallback) wii_shutdown);
	SYS_SetPowerCallback(wii_shutdown);
	SYS_SetResetCallback(wii_reset);

	fatInitDefault();
	if (chdir("usb:/mame/") == 0 || chdir("sd:/mame/") == 0);

	wii_init_width(); 
	wii_init_video();

	// cli_execute does the heavy lifting; if we have osd-specific options, we
	// would pass them as the third parameter here
	ret = cli_execute(nargc, (char **) nargv, wii_mame_options);

	wii_shutdown_video();
	
	if (shutting_down)
		SYS_ResetSystem(SYS_POWEROFF, 0, 0);

	return ret;
}


//============================================================
//  osd_init
//============================================================

void osd_init(running_machine *machine)
{
	current_machine = machine;
	our_target = render_target_alloc(current_machine, NULL, 0);

	if (our_target == NULL)
		fatalerror("Error creating render target");

	wii_init_dimensions();
	wii_init_input(current_machine);
	wii_init_audio(current_machine);

	return;
}


//============================================================
//  osd_wait_for_debugger
//============================================================

void osd_wait_for_debugger(const device_config *device, int firststop)
{
	// we don't have a debugger, so we just return here
}


//============================================================
//  osd_update
//============================================================

void osd_update(running_machine *machine, int skip_redraw)
{
	const render_primitive_list *primlist = render_target_get_primitives(our_target);

	if (!skip_redraw)
	{
		// make that the size of our target
		render_target_set_bounds(our_target, wii_render_width(), wii_render_height(), 0);

		// lock them, and then render them
		wii_video_render(primlist);
	}

	wii_poll_input();
}


//============================================================
//  osd_customize_input_type_list
//============================================================

void osd_customize_input_type_list(input_type_desc *typelist)
{
	// This function is called on startup, before reading the
	// configuration from disk. Scan the list, and change the
	// default control mappings you want. It is quite possible
	// you won't need to change a thing.
}


//============================================================
//  keyboard_get_state
//============================================================

static INT32 keyboard_get_state(void *device_internal, void *item_internal)
{
	// this function is called by the input system to get the current key
	// state; it is specified as a callback when adding items to input
	// devices
	UINT8 *keystate = (UINT8 *)item_internal;
	return *keystate;
}
