// 8080EmulatorVS.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <stdio.h>
#include <SDL.h>
#undef main // SDL.h for whatever reason defines main as something else and breaks int main()

using namespace std;

typedef struct ConditionCodes {
	uint8_t z : 1;
	uint8_t s : 1;
	uint8_t p : 1;
	uint8_t cy : 1;
	uint8_t ac : 1;
	uint8_t pad : 3;
};

typedef struct State8080 {
	uint8_t a;
	uint8_t b;
	uint8_t c;
	uint8_t d;
	uint8_t e;
	uint8_t h;
	uint8_t l;
	uint16_t sp = 0xf000;
	uint16_t pc;
	uint8_t *memory;
	struct ConditionCodes cc;
	uint8_t int_enable;
};

void Emulate(State8080* state);
void SetFlags(int answer, State8080* state);

int main(int argc, char** argv)
{
	// Open ROM file
	FILE *f;
	int errorcode = fopen_s(&f, argv[1], "rb");
	if (errorcode != 0)
	{
		printf("Error: Could not open file %s\n", argv[1]);
		exit(1);
	}

	fseek(f, 0L, SEEK_END);
	int fsize = ftell(f);
	fseek(f, 0L, SEEK_SET);

	// Allocate space in memory for the ROM
	//unsigned char *buffer = (unsigned char*)malloc(0xffff);

	// Create the registers and memory of the 8080
	State8080* state = new State8080();

	// Allocate memory to store the ROM
	state->memory = (uint8_t*)malloc(0xffff);
	
	// Store the rom in allocated memory
	fread(state->memory, fsize, 1, f);
	fclose(f);

	// Emulate 100 instructions
	for (int i = 0; i < 100000; i++)//gf
	{
		printf("Instruction %d: ", i);
		Emulate(state);
	}
	
	free(state->memory);

	return 0;
}

void Emulate(State8080* state)
{
	unsigned char *opcode = &state->memory[state->pc];
	printf("Currently running instruction 0x%x at address 0x%x\n", *opcode, state->pc);

	switch (*opcode)
	{
	case 0x00: 
		break;
	case 0x01: 
		state->c = opcode[2];
		state->b = opcode[1];
		state->pc += 2;
		break;
	case 0x05:
		state->b -= 1;
		SetFlags(state->b, state);
		break;
	case 0x06:
		state->b = opcode[1];
		state->pc += 1;
		break;
	case 0xe:
		state->c = opcode[1];
		state->pc++;
		break;
	case 0x11: // LXI DE
		state->d = opcode[2];
		state->e = opcode[1];
		state->pc += 2;
		break;
	case 0x13:
	{
		uint16_t address = (state->d << 8) | state->e;
		address += 1;
		state->d = (address >> 8) & 0xff;
		state->e = address & 0xff;
		break;
	}
	case 0x14:
		state->d++;
		SetFlags(state->d, state);
		break;
	case 0x19:
	{
		uint16_t hl = (state->h << 8) | state->l;
		uint16_t de = (state->d << 8) | state->e;
		uint32_t answer = hl + de;
		state->h = (answer >> 8) & 0xff;
		state->l = answer & 0xff;
		state->cc.cy = (answer > 0xff);
		break;
	}
	case 0x1a:
	{
		uint16_t address = (state->d << 8) | state->e;
		state->a = state->memory[address];
		break;
	}
	case 0x21: // LXI HL
		state->h = opcode[2];
		state->l = opcode[1];		
		state->pc += 2;
		break;
	case 0x23:
	{
		uint16_t address = (state->h << 8) | state->l;
		address += 1;
		state->h = (address >> 8) & 0xff;
		state->l = address & 0xff;
		break;
	}
	case 0x26:
		state->h = opcode[1];
		state->pc++;
		break;
	case 0x29:
	{
		uint16_t hl = (state->h << 8) | state->l;
		uint32_t answer = hl * 2;
		state->h = (answer >> 8) & 0xff;
		state->l = answer & 0xff;
		state->cc.cy = (answer > 0xff);
		break;
	}
	case 0x31:
		state->sp = opcode[2] * 256 + opcode[1];
		state->pc += 2;
		break;
	case 0x36:
	{
		uint16_t address = (state->h << 8) | state->l;
		state->memory[address] = opcode[1];
		state->pc++;
		break;
	}
	case 0x39:
	{
		uint16_t hl = (state->h << 8) | state->l;
		uint32_t answer = hl + state->sp;
		state->h = (answer >> 8) & 0xff;
		state->l = answer & 0xff;
		state->cc.cy = (answer > 0xff);
		break;
	}
	case 0x41:
		state->b = state->c;
		break;
	case 0x42:
		state->b = state->d;
		break;
	case 0x43:
		state->b = state->e;
		break;
	case 0x6f:
		state->l = state->a;
		break;
	case 0x77:
	{
		uint16_t address = (state->h << 8) | state->l;
		state->memory[address] = state->a;
		break;
	}
	case 0x7c:
		state->a = state->h;
		break;
	case 0x80:
	{
		uint16_t answer = (uint16_t)state->a + (uint16_t)state->b;
		SetFlags(answer, state);
		break;
	}		
	case 0x81:
	{
		uint16_t answer = (uint16_t)state->a + (uint16_t)state->c;
		SetFlags(answer, state);
		break;
	}		
	case 0x86:
	{
		// TODO: Fix this. I probably accidentally delted code
		uint16_t offset = state->h * 256 + state->l;
		uint16_t answer = 0;
		SetFlags(answer, state);
		break;
	}		
	case 0xc2:
		if (state->cc.z == 0)
		{
			state->pc = opcode[2] * 256 + opcode[1] - 1;
		}
		else
		{
			state->pc += 2;
		}
		break;
	case 0xc3: // JUMP
		state->pc = ((opcode[2] << 8) | opcode[1]) - 1;
		break;
	case 0xc6:
	{
		uint16_t answer = (uint16_t)state->a + (uint16_t)opcode[1];
		SetFlags(answer, state);
		break;
	}		
	case 0xc9: // RETURN
		// Get the return address from the stack
		state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
		state->sp += 2;
		break;
	case 0xcd: // CALL
	{
		uint16_t returnAddress = state->pc + 2;
		state->memory[state->sp - 1] = (returnAddress >> 8) & 0xff;
		state->memory[state->sp - 2] = returnAddress & 0xff;
		state->sp -= 2;
		state->pc = (opcode[2] << 8) | opcode[1] - 1;
		break;
	}
	case 0xd3:
		break;
	case 0xd5:
		state->memory[state->sp - 2] = state->e;
		state->memory[state->sp - 1] = state->d;
		state->sp -= 2;
		break;
	case 0xe5:
		state->memory[state->sp - 2] = state->l;
		state->memory[state->sp - 1] = state->h;
		state->sp -= 2;
		break;
	case 0xeb:
	{
		uint8_t temp = state->d;
		state->d = state->h;
		state->h = temp;
		temp = state->e;
		state->e = state->l;
		state->l = temp;
	}
	case 0xfe:
	{
		uint8_t x = state->a - opcode[1];
		state->cc.z = (x == 0);
		state->cc.s = (0x80 == (x & 0x80));
		state->cc.cy = (state->a < opcode[1]);
		state->pc++;
		break;
	}		
	default: printf("Error: Instruction 0x%x not implemented.", *opcode); exit(1);
	}
	state->pc += 1;
}

void SetFlags(int answer, State8080* state)
{
	state->cc.z = ((answer & 0xff) == 0);
	state->cc.s = ((answer & 0x80) != 0);
	state->cc.cy = (answer > 0xff);
	state->a = answer & 0xff;
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