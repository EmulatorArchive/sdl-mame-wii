To save memory, SDL MAME Wii uses a custom build of SDL-Wii. Simply replace
SDL_wiivideo.c found in SDL\src\video\wii with the one found here and
compile/install it. SDL MAME Wii might be able to run with an unedited SDL,
but a chunk of memory will be used up, which can cause some larger games to
fail to load.
