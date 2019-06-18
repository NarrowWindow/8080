// 8080EmulatorVS.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <stdio.h>
#include <SDL.h>
#include "SDLHelper.h"
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
	bool interrupt_enabled;
	uint16_t sp = 0xf000; // Stack pointer
	uint16_t pc; // Program counter
	uint8_t *memory;
	struct ConditionCodes cc;
	uint8_t int_enable;
};

void Emulate(State8080* state);
void SetFlags(int answer, State8080* state);
void Init(State8080* state, char* romFile);
void UpdateDisplay(State8080* state, SDLHelper sdlHelper);

SDLHelper sdlHelper;

int main(int argc, char** argv)
{
	// Create the registers and memory of the 8080 processor
	State8080* state = new State8080();

	// Initialize the state of the processor
	Init(state, argv[1]);
	
	// Initialize SDL
	sdlHelper.init();	

	// Emulate a fixed number of instructions
	for (int i = 0; i <= 43000; i++)
	{
		printf("Instruction %d: ", i);		

		Emulate(state);

		if (i % 120 == 0)
		{
			UpdateDisplay(state, sdlHelper);
		}
	}
	
	delete(state);
	getchar();

	return 0;
}

void Init(State8080* state, char* romName)
{
	// Open ROM file
	FILE* romFile;
	if (romName == nullptr)
	{
		printf("Error: File name not entered as command line argument");
		exit(1);
	}
	int errorcode = fopen_s(&romFile, romName, "rb");
	if (errorcode != 0)
	{
		printf("Error: Could not open file %s\n", romName);
		exit(1);
	}

	fseek(romFile, 0L, SEEK_END);
	int fsize = ftell(romFile);
	fseek(romFile, 0L, SEEK_SET);

	// Allocate memory to store the ROM
	state->memory = (uint8_t*)malloc(0xffff);

	// Store the rom in allocated memory
	fread(state->memory, fsize, 1, romFile);
	fclose(romFile);

	// Clear the RAM
	for (int i = 0x2400; i <= 0x3FFF; i++)
	{
		state->memory[i] = 0b11111111;
	}
}

void Emulate(State8080* state)
{
	unsigned char *opcode = &state->memory[state->pc];
	printf("Currently running instruction 0x%x at address 0x%x with state: %02x %02x%02x %02x%02x %02x%02x %04x %04x\n", *opcode, state->pc, state->a, state->b, state->c, state->d, state->e, state->h, state->l, state->pc, state->sp);
	//printf("Stack is: %x %x %x %x %x\n", state->memory[state->sp], state->memory[state->sp + 1], state->memory[state->sp + 2], state->memory[state->sp + 3], state->memory[state->sp + 4]);

	switch (*opcode)
	{
	case 0x00: 
		break;
	case 0x01: 
		state->c = opcode[1];
		state->b = opcode[2];
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
	case 0x09:
	{
		uint16_t hl = (state->h << 8) | state->l;
		uint16_t bc = (state->b << 8) | state->c;
		uint32_t answer = hl + bc;
		state->h = (answer >> 8) & 0xff;
		state->l = answer & 0xff;
		state->cc.cy = (answer > 0xff);
		break;
	}
	case 0xd: // DCR C
		state->c -= 1;
		SetFlags(state->c, state);
		break;
	case 0xe:
		state->c = opcode[1];
		state->pc++;
		break;
	case 0xf:
	{
		bool lastBit = state->a & 0b1;
		state->a = state->a << 7 | lastBit;
		state->cc.cy = lastBit;
		break;
	}
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
	case 0x32:
	{
		uint16_t address = (opcode[2] << 8) | opcode[1];
		state->memory[address] = state->a;
		state->pc += 2;
		break;
	}		
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
	case 0x3a:
	{
		uint16_t address = (opcode[2] << 8) | opcode[1];
		state->a = state->memory[address];
		state->pc += 2;
		break;
	}
	case 0x3e:
		state->a = opcode[1];
		state->pc++;
		break;
	case 0x41:
		state->b = state->c;
		break;
	case 0x42:
		state->b = state->d;
		break;
	case 0x43:
		state->b = state->e;
		break;
	case 0x56:
	{
		uint16_t address = (state->h << 8) | state->l;
		state->d = state->memory[address];
		break;
	}		
	case 0x5e:
	{
		uint16_t address = (state->h << 8) | state->l;
		state->e = state->memory[address];
		break;
	}
	case 0x5f: // MOV E, A
		state->e = state->a;
		break;
	case 0x66:
	{
		uint16_t address = (state->h << 8) | state->l;
		state->h = state->memory[address];
		break;
	}
	case 0x6f:
		state->l = state->a;
		break;
	case 0x77:
	{
		uint16_t address = (state->h << 8) | state->l;
		state->memory[address] = state->a;
		break;
	}
	case 0x7a: // MOV A, D
		state->a = state->d;
		break;
	case 0x7b: // MOV A, E
		state->a = state->e;
	case 0x7c:
		state->a = state->h;
		break;
	case 0x7e:
	{
		uint16_t address = (state->h << 8) | state->l;
		state->a = state->memory[address];
		break;
	}		
	case 0x80:
	{
		uint16_t answer = (uint16_t)state->a + (uint16_t)state->b;
		state->a = answer & 0xff;
		SetFlags(answer, state);
		break;
	}		
	case 0x81:
	{
		uint16_t answer = (uint16_t)state->a + (uint16_t)state->c;
		state->a = answer & 0xff;
		SetFlags(answer, state);
		break;
	}		
	case 0x86:
	{
		// TODO: Fix this. I probably accidentally delted code
		uint16_t address = state->h * 256 + state->l;
		uint16_t answer = (uint16_t)state->a + (uint16_t)state->memory[address];
		state->a = answer & 0xff;
		SetFlags(answer, state);
		break;
	}
	case 0xa7:
		SetFlags(state->a, state);
		break;
	case 0xaf:
		state->a = state->a ^ state->a;
		break;
	case 0xc1:
		state->c = state->memory[state->sp];
		state->b = state->memory[state->sp + 1];
		state->sp += 2;
		break;
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
	case 0xc5:
		state->memory[state->sp - 2] = state->c;
		state->memory[state->sp - 1] = state->b;
		state->sp -= 2;
		break;
	case 0xc6:
	{
		uint16_t answer = (uint16_t)state->a + (uint16_t)opcode[1];
		SetFlags(answer, state);
		state->pc++;
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
	case 0xd1: // POP D
		state->e = state->memory[state->sp];
		state->d = state->memory[state->sp + 1];
		state->sp += 2;
		break;
	case 0xd3:
		state->pc++;
		break;
	case 0xd5:
		state->memory[state->sp - 2] = state->e;
		state->memory[state->sp - 1] = state->d;
		state->sp -= 2;
		break;
	case 0xe1:
		state->l = state->memory[state->sp];
		state->h = state->memory[state->sp + 1];
		state->sp += 2;
		break;
	case 0xe5:
		state->memory[state->sp - 2] = state->l;
		state->memory[state->sp - 1] = state->h;
		state->sp -= 2;
		break;
	case 0xe6:
		state->a = state->a & opcode[1];
		SetFlags(state->a, state);
		state->pc++;
		break;
	case 0xeb:
	{
		uint8_t temp = state->d;
		state->d = state->h;
		state->h = temp;
		temp = state->e;
		state->e = state->l;
		state->l = temp;
		break;
	}
	case 0xf1:
		state->cc.z = state->memory[state->sp] & 0b1;
		state->cc.s = (state->memory[state->sp] >> 1) & 0b1;
		state->cc.p = (state->memory[state->sp] >> 2) & 0b1;
		state->cc.cy = (state->memory[state->sp] >> 3) & 0b1;
		state->cc.ac = (state->memory[state->sp] >> 4) & 0b1;
		state->a = state->memory[state->sp + 1];
		state->sp += 2;
		break;
	case 0xf3: // DI
		state->interrupt_enabled = false;
		break;
	case 0xf5:
		state->memory[state->sp - 1] = state->a;
		state->memory[state->sp - 2] = state->cc.z | (state->cc.s << 1) | (state->cc.p << 2) | (state->cc.cy << 3) | (state->cc.ac << 4);
		state->sp -= 2;
		break;
	case 0xfb: // EI
		state->interrupt_enabled = true;
		break;
	case 0xfe:
	{
		SetFlags(state->a - opcode[1], state);
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
	state->cc.p = answer % 2 == 0;
	state->cc.cy = (answer > 0xff);
}

void UpdateDisplay(State8080* state, SDLHelper sdlHelper)
{
	int videoPointer = 0x2400;
	for (int w = 0; w < 224; w++)
	{
		for (int h = 31; h >= 0; h--)
		{
			int byte = state->memory[videoPointer];
			for (int b = 0; b < 8; b++)
			{
				if (byte % 2 == 0)
				{
					SDL_SetRenderDrawColor(sdlHelper.renderer, 0x0, 0x0, 0x0, 0xFF);
				}
				else
				{
					SDL_SetRenderDrawColor(sdlHelper.renderer, 0xFF, 0xFF, 0xFF, 0xFF);
				}
				SDL_RenderDrawPoint(sdlHelper.renderer, w, (h) * 8 + b);
				int nh = (h + 1) * 8 - b - 1;
				int nw = w;
				byte /= 2;
			}
			videoPointer++;
		}
	}
	SDL_RenderPresent(sdlHelper.renderer);
}