// 8080EmulatorVS.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <stdio.h>
#include <SDL.h>
#undef main

using namespace std;

int main()
{
	cout << "Hello World" << endl;
	return 0;
}

/*

void PutPixel32_nolock(SDL_Surface*, int, int, Uint32);

int main()
{
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		cout << "SDL_Init Error: " << SDL_GetError() << endl;
		return 1;
	}
	
	SDL_Window *win = SDL_CreateWindow("Hello World!", 100, 100, 640, 480, SDL_WINDOW_SHOWN);
	if (win == nullptr)
	{
		std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return 1;
	}
	
	SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (ren == nullptr)
	{
		SDL_DestroyWindow(win);
		cout << "SDL_CreateRenderer Error: " << SDL_GetError() << endl;
		SDL_Quit();
		return 1;
	}

	SDL_Surface *surface = SDL_CreateRGBSurface(0, 640, 480, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	if (surface == nullptr)
	{
		SDL_DestroyWindow(win);
		cout << "Error: " << SDL_GetError() << endl;
		SDL_Quit();
		return 1;
	}

	for (int i = 0; i < 640; i++)
	{
		for (int j = 0; j < 480; j++)
		{
			PutPixel32_nolock(surface, i, j, 0x00000000);
		}		
	}
	

    return 0;
}

void PutPixel32_nolock(SDL_Surface * surface, int x, int y, Uint32 color)
{
	Uint8 * pixel = (Uint8*)surface->pixels;
	pixel += (y * surface->pitch) + (x * sizeof(Uint32));
	*((Uint32*)pixel) = color;
}

*/