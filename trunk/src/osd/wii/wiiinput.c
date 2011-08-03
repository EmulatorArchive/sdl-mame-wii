//============================================================
//
//  Copyright (c) 1996-2009, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//  MAME Wii by Toad King
//
//============================================================

#include "osdepend.h"
#include "wiiinput.h"
#include "wiimisc.h"
#include <wiiuse/wpad.h>
#include <ogc/pad.h>

static int gamecube_buttons[4][8];
static int gamecube_hats[4][4];
static int gamecube_axis[4][6];
static int wiimote_buttoms[4][16];
static int wiimote_hats[4][1];
static int wiimote_axis[4][4];

static int joypad_get_state(void *device_internal, void *item_internal)
{
        return *(int *) item_internal;
}

void wii_init_input(running_machine *machine)
{
	int i;
	input_device_class_enable(machine, DEVICE_CLASS_LIGHTGUN, TRUE);
	input_device_class_enable(machine, DEVICE_CLASS_JOYSTICK, TRUE);

	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(WPAD_CHAN_ALL, wii_screen_width(), 480);

	for (i = 0; i < 4; i++)
	{
		static const char* const buttons[8] = { "A", "B", "X", "Y", "Z", "R", "L", "Start" };
		char name[11];
		int j;
		input_device *devinfo;

		snprintf(name, 11, "GameCube %d", i + 1);
		devinfo = input_device_add(machine, DEVICE_CLASS_JOYSTICK, name, NULL);

		for (j = 0; j < 8; j++)
			input_device_item_add(devinfo, buttons[j], &gamecube_buttons[i][j], (input_item_id)(ITEM_ID_BUTTON1 + j), joypad_get_state);

		input_device_item_add(devinfo, "D-Pad Up", &gamecube_hats[i][0], ITEM_ID_HAT1UP, joypad_get_state);
		input_device_item_add(devinfo, "D-Pad Down", &gamecube_hats[i][1], ITEM_ID_HAT1DOWN, joypad_get_state);
		input_device_item_add(devinfo, "D-Pad Left", &gamecube_hats[i][2], ITEM_ID_HAT1LEFT, joypad_get_state);
		input_device_item_add(devinfo, "D-Pad Right", &gamecube_hats[i][3], ITEM_ID_HAT1RIGHT, joypad_get_state);
		input_device_item_add(devinfo, "Main X Axis", &gamecube_axis[i][0], ITEM_ID_XAXIS, joypad_get_state);
		input_device_item_add(devinfo, "Main Y Axis", &gamecube_axis[i][1], ITEM_ID_YAXIS, joypad_get_state);
		input_device_item_add(devinfo, "C X Axis", &gamecube_axis[i][2], ITEM_ID_RXAXIS, joypad_get_state);
		input_device_item_add(devinfo, "C Y Axis", &gamecube_axis[i][3], ITEM_ID_RYAXIS, joypad_get_state);
		input_device_item_add(devinfo, "L Analog", &gamecube_axis[i][4], ITEM_ID_ZAXIS, joypad_get_state);
		input_device_item_add(devinfo, "Y Analog", &gamecube_axis[i][5], ITEM_ID_RZAXIS, joypad_get_state);
	}
}

void wii_poll_input()
{
	int i;

	PAD_ScanPads();

	for (i = 0; i < 4; i++)
	{
		static const int const buttonmap[8] = { PAD_BUTTON_A, PAD_BUTTON_B, PAD_BUTTON_X, PAD_BUTTON_Y, PAD_TRIGGER_Z, PAD_TRIGGER_R, PAD_TRIGGER_L, PAD_BUTTON_START };
		int j;

		UINT16 buttons = PAD_ButtonsHeld(i);

		for (j = 0; j < 8; j++)
			gamecube_buttons[i][j] = ((buttons & buttonmap[j]) != 0) ? 0x80 : 0;

		gamecube_hats[i][0] = ((buttons & PAD_BUTTON_UP) != 0) ? 0x80 : 0;
		gamecube_hats[i][1] = ((buttons & PAD_BUTTON_DOWN) != 0) ? 0x80 : 0;
		gamecube_hats[i][2] = ((buttons & PAD_BUTTON_LEFT) != 0) ? 0x80 : 0;
		gamecube_hats[i][3] = ((buttons & PAD_BUTTON_RIGHT) != 0) ? 0x80 : 0;
		
		gamecube_axis[i][0] = PAD_StickX(i) * 512;
		gamecube_axis[i][1] = PAD_StickY(i) * -512;
		gamecube_axis[i][2] = PAD_SubStickX(i) * 512;
		gamecube_axis[i][3] = PAD_SubStickY(i) * -512;
		gamecube_axis[i][4] = PAD_TriggerL(i) * 512;
		gamecube_axis[i][5] = PAD_TriggerR(i) * 512;
	}
}
