//============================================================
//
//  Copyright (c) 1996-2009, Nicola Salmoria and the MAME Team.
//  Visit http://mamedev.org for licensing and usage restrictions.
//
//  MAME Wii by Toad King
//
//============================================================

#ifndef _WIIVIDEO_H_
#define _WIIVIDEO_H_

#include "render.h"

void wii_init_video();
void wii_init_dimensions();
void wii_video_render(const render_primitive_list *primlist);
int wii_render_width();
int wii_render_height();
void wii_shutdown_video();

#endif
