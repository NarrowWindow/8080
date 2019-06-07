#pragma once

#include <SDL.h>
#include <stdio.h>

class SDLHelper
{
public:
	bool init();

	SDL_Window* window;
	SDL_Renderer* renderer;
};