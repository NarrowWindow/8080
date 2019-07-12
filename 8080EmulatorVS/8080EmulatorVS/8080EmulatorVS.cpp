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
	bool z : 1;
	bool s : 1;
	bool p : 1;
	bool cy : 1;
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
	int cycles = 16667;
	uint16_t interrupt_pointer = 0x10;
};

void Emulate(State8080* state);
void SetFlags(int answer, bool changeCarry, State8080* state);
void Init(State8080* state, char* romFile);
void UpdateDisplay(State8080* state, SDLHelper sdlHelper);
void ProcessInterrupt(State8080* state, SDLHelper* sdlHelper);
void Return(State8080* state);
void Call(uint16_t newAddress, int instructionSize, State8080* state);

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
	for (int i = 0; i <= 3000000; i++)
	{
		if (i > 2750014 && i < 2750035)
		{
			printf("Instruction %d: Currently running instruction 0x%02x with state: %02x %02x%02x %02x%02x %02x%02x %04x %04x %d ", i, state->memory[state->pc], state->a, state->b, state->c, state->d, state->e, state->h, state->l, state->pc, state->sp, 16667 - state->cycles);
			
			if (state->cc.z)
				printf("z");
			if (state->cc.s)
				printf("s");
			if (state->cc.p)
				printf("p");
			if (state->interrupt_enabled)
				printf("i");			
			if (state->cc.cy)
				printf("c");
			printf("\n");
		}		

		Emulate(state);

		ProcessInterrupt(state, &sdlHelper);
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
	for (int i = 0x2000; i <= 0x23FF; i++)
	{
		state->memory[i] = 0;
	}
	for (int i = 0x2400; i <= 0x3FFF; i++)
	{
		state->memory[i] = 0xFF;
	}
	for (int i = 0x4000; i <= 0xFFFF; i++)
	{
		state->memory[i] = 0;
	}
}

void ProcessInterrupt(State8080* state, SDLHelper* sdlHelper)
{
	if (state->cycles <= 0)
	{
		if (state->interrupt_enabled)
		{
			state->memory[state->sp - 1] = (state->pc >> 8) & 0xff;
			state->memory[state->sp - 2] = state->pc & 0xff;
			state->sp -= 2;
			state->pc = state->interrupt_pointer;
			if (state->interrupt_pointer == 0x10)
			{
				state->interrupt_pointer = 0x08;
			}
			else
			{
				state->interrupt_pointer = 0x10;
			}
			UpdateDisplay(state, *sdlHelper);
		}
		state->cycles += 16667;
	}
}

void Return(State8080* state)
{
	state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
	state->sp += 2;
}

void Call(uint16_t newAddress, int instructionSize, State8080* state)
{
	uint16_t returnAddress = state->pc + instructionSize;
	state->memory[state->sp - 1] = (returnAddress >> 8) & 0xff;
	state->memory[state->sp - 2] = returnAddress & 0xff;
	state->sp -= 2;
	state->pc = newAddress;
}

void Emulate(State8080* state)
{
	unsigned char *opcode = &state->memory[state->pc];
	switch (*opcode)
	{
	case 0x00:
		state->pc++;
		state->cycles -= 4;		
		break;
	case 0x01: 
		state->c = opcode[1];
		state->b = opcode[2];
		state->pc += 3;
		state->cycles -= 10;
		break;
	case 0x03:
	{
		uint16_t bc = (state->b << 8) | state->c;
		uint32_t answer = bc + 1;
		state->b = (answer >> 8) & 0xff;
		state->c = answer & 0xff;
		state->pc++;
		state->cycles -= 6;
		break;
	}
	case 0x04:
	{
		state->b++;
		SetFlags(state->b, false, state);
		state->pc++;
		state->cycles -= 5;
		break;
	}		
	case 0x05:
	{
		state->b -= 1;
		SetFlags(state->b, false, state);
		state->pc++;
		state->cycles -= 5;
		break;
	}		
	case 0x06:
		state->b = opcode[1];
		state->pc += 2;
		state->cycles -= 7;
		break;
	case 0x07:
	{
		uint8_t firstBit = (state->a & 0b10000000);
		state->cc.cy = firstBit;
		firstBit >>= 7;
		state->a <<= 1;
		state->a |= firstBit;
		state->pc++;
		state->cycles -= 4;
		break;
	}
	case 0x09:
	{
		uint16_t hl = (state->h << 8) | state->l;
		uint16_t bc = (state->b << 8) | state->c;
		uint32_t answer = hl + bc;
		state->h = (answer >> 8) & 0xff;
		state->l = answer & 0xff;
		state->cc.cy = (answer > 0xffff);
		state->pc++;
		state->cycles -= 11;
		break;
	}
	case 0xa:
	{
		uint16_t address = (state->b << 8) | state->c;
		state->a = state->memory[address];
		state->pc++;
		state->cycles -= 7;
		break;
	}
	case 0xd: // DCR C
		state->c -= 1;
		SetFlags(state->c, false, state);
		state->pc++;
		state->cycles -= 5;
		break;
	case 0xe:
		state->c = opcode[1];
		state->pc += 2;
		state->cycles -= 7;
		break;
	case 0xf:
	{
		uint8_t lastBit = (state->a & 0b1);
		state->cc.cy = lastBit;
		lastBit <<= 7;
		state->a >>= 1;
		state->a |= lastBit;
		state->pc++;
		state->cycles -= 4;
		break;
	}
	case 0x11: // LXI DE
		state->d = opcode[2];
		state->e = opcode[1];
		state->pc += 3;
		state->cycles -= 10;
		break;
	case 0x13:
	{
		uint16_t address = (state->d << 8) | state->e;
		address += 1;
		state->d = (address >> 8) & 0xff;
		state->e = address & 0xff;
		state->pc++;
		state->cycles -= 6;
		break;
	}
	case 0x14:
		state->d++;
		SetFlags(state->d, false, state);
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x15:
	{
		state->d--;
		SetFlags(state->d, false, state);
		state->pc++;
		state->cycles -= 5;
		break;
	}
	case 0x16:
		state->d = opcode[1];
		state->pc += 2;
		state->cycles -= 7;
		break;
	case 0x19:
	{
		uint16_t hl = (state->h << 8) | state->l;
		uint16_t de = (state->d << 8) | state->e;
		uint32_t answer = hl + de;
		state->h = (answer >> 8) & 0xff;
		state->l = answer & 0xff;
		state->cc.cy = (answer > 0xffff);
		state->pc++;
		state->cycles -= 11;
		break;
	}
	case 0x1a:
	{
		uint16_t address = (state->d << 8) | state->e;
		state->a = state->memory[address];
		state->pc++;
		state->cycles -= 7;
		break;
	}
	case 0x1f:
	{
		uint8_t lastBit = (state->a & 0b1);
		state->a >>= 1;
		state->a |= state->cc.cy << 7;
		state->cc.cy = lastBit;
		state->pc++;
		state->cycles -= 4;
		break;
	}		
	case 0x21: // LXI HL
		state->h = opcode[2];
		state->l = opcode[1];
		state->pc += 3;
		state->cycles -= 10;
		break;
	case 0x22:
	{
		uint16_t address = (opcode[2] << 8) | opcode[1];
		state->memory[address] = state->l;
		state->memory[address + 1] = state->h;
		state->pc += 3;
		state->cycles -= 16;
		break;
	}
	case 0x23:
	{
		uint16_t address = (state->h << 8) | state->l;
		address += 1;
		state->h = (address >> 8) & 0xff;
		state->l = address & 0xff;
		state->pc++;
		state->cycles -= 6;
		break;
	}
	case 0x26:
		state->h = opcode[1];
		state->pc += 2;
		state->cycles -= 7;
		break;
	case 0x29:
	{
		uint16_t hl = (state->h << 8) | state->l;
		uint32_t answer = hl * 2;
		state->h = (answer >> 8) & 0xff;
		state->l = answer & 0xff;
		state->cc.cy = (answer > 0xff);
		state->pc++;
		state->cycles -= 11;
		break;
	}
	case 0x2a:
	{
		uint16_t address = (opcode[2] << 8) | opcode[1];
		state->l = state->memory[address];
		state->h = state->memory[address + 1];
		state->pc += 3;
		state->cycles -= 16;
		break;
	}
	case 0x2b:
	{
		uint16_t hl = (state->h << 8) | state->l - 1;
		state->l = hl & 0xff;
		state->h = (hl & 0xff00) >> 8;
		state->pc++;
		state->cycles -= 6;
		break;
	}
	case 0x2c:
		state->l++;
		SetFlags(state->l, false, state);
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x2e:
		state->l = opcode[1];
		state->pc += 2;
		state->cycles -= 7;
		break;
	case 0x2f:
		state->a = ~state->a;
		state->pc++;
		state->cycles -= 4;
		break;
	case 0x31:
		state->sp = opcode[2] * 256 + opcode[1];
		state->pc += 3;
		state->cycles -= 10;
		break;
	case 0x32:
	{
		uint16_t address = (opcode[2] << 8) | opcode[1];
		state->memory[address] = state->a;
		state->pc += 3;
		state->cycles -= 13;
		break;
	}
	case 0x34:
	{
		uint16_t address = (state->h << 8) | state->l;
		state->memory[address]++;
		SetFlags(state->memory[address], false, state);
		state->pc++;
		state->cycles -= 10;
		break;
	}
	case 0x35:
	{
		uint16_t address = (state->h << 8) | state->l;
		state->memory[address] -= 1;
		SetFlags(state->memory[address], false, state);
		state->pc++;
		state->cycles -= 10;
		break;
	}
	case 0x36:
	{
		uint16_t address = (state->h << 8) | state->l;
		state->memory[address] = opcode[1];
		state->pc += 2;
		state->cycles -= 10;
		break;
	}
	case 0x37:
		state->cc.cy = 1;
		state->pc++;
		state->cycles -= 4;
		break;
	case 0x39:
	{
		uint16_t hl = (state->h << 8) | state->l;
		uint32_t answer = hl + state->sp;
		state->h = (answer >> 8) & 0xff;
		state->l = answer & 0xff;
		state->cc.cy = (answer > 0xff);
		state->pc++;
		state->cycles -= 11;
		break;
	}
	case 0x3a:
	{
		uint16_t address = (opcode[2] << 8) | opcode[1];
		state->a = state->memory[address];
		state->pc += 3;
		state->cycles -= 13;
		break;
	}
	case 0x3c:
	{
		state->a++;
		SetFlags(state->a, false, state);
		state->pc++;
		state->cycles -= 5;
		break;
	}
	case 0x3d:
		state->a -= 1;
		SetFlags(state->a, false, state);
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x3e:
		state->a = opcode[1];
		state->pc += 2;
		state->cycles -= 7;
		break;
	case 0x41:
		state->b = state->c;
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x42:
		state->b = state->d;
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x43:
		state->b = state->e;
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x46:
	{
		uint16_t address = (state->h << 8) | state->l;
		state->b = state->memory[address];
		state->pc++;
		state->cycles -= 7;
		break;
	}
	case 0x47:
	{
		state->b = state->a;
		state->pc++;
		state->cycles -= 5;
		break;
	}
	case 0x4d:
		state->c = state->l;
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x4e:
	{
		uint16_t address = (state->h << 8) | state->l;
		state->c = state->memory[address];
		state->pc++;
		state->cycles -= 7;
		break;
	}
	case 0x4f:
		state->c = state->a;
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x52: // MOV D,D
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x53: // MOV D,E
		state->d = state->e;
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x56:
	{
		uint16_t address = (state->h << 8) | state->l;
		state->d = state->memory[address];
		state->pc++;
		state->cycles -= 7;
		break;
	}
	case 0x57:
		state->d = state->a;
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x5e:
	{
		uint16_t address = (state->h << 8) | state->l;
		state->e = state->memory[address];
		state->pc++;
		state->cycles -= 7;
		break;
	}
	case 0x5f: // MOV E, A
		state->e = state->a;
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x61:
		state->h = state->c;
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x66:
	{
		uint16_t address = (state->h << 8) | state->l;
		state->h = state->memory[address];
		state->pc++;
		state->cycles -= 7;
		break;
	}
	case 0x67:
		state->h = state->a;
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x68:
		state->l = state->b;
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x69:
		state->l = state->c;
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x6f:
		state->l = state->a;
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x70:
	{
		uint16_t address = (state->h << 8) | state->l;
		state->memory[address] = state->b;
		state->pc++;
		state->cycles -= 7;
		break;
	}
	case 0x71:
	{
		uint16_t address = (state->h << 8) | state->l;
		state->memory[address] = state->c;
		state->pc++;
		state->cycles -= 7;
		break;
	}
	case 0x77:
	{
		uint16_t address = (state->h << 8) | state->l;
		state->memory[address] = state->a;
		state->pc++;
		state->cycles -= 7;
		break;
	}
	case 0x78:
		state->a = state->b;
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x79:
		state->a = state->c;
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x7a: // MOV A, D
		state->a = state->d;
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x7b: // MOV A, E
		state->a = state->e;
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x7c:
		state->a = state->h;
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x7d: // MOV A,L - register A = register L
		state->a = state->l;
		state->pc++;
		state->cycles -= 5;
		break;
	case 0x7e:
	{
		uint16_t address = (state->h << 8) | state->l;
		state->a = state->memory[address];
		state->pc++;
		state->cycles -= 7;
		break;
	}		
	case 0x80:
	{
		uint16_t answer = (uint16_t)state->a + (uint16_t)state->b;
		state->a = answer & 0xff;
		SetFlags(answer, true, state);
		state->pc++;
		state->cycles -= 4;
		break;
	}		
	case 0x81:
	{
		uint16_t answer = (uint16_t)state->a + (uint16_t)state->c;
		state->a = answer & 0xff;
		SetFlags(answer, true, state);
		state->pc++;
		state->cycles -= 4;
		break;
	}
	case 0x85:
		state->a += state->l;
		SetFlags(state->a, true, state);
		state->pc++;
		state->cycles -= 4;
		break;
	case 0x86:
	{
		uint16_t address = (state->h << 8) | state->l;
		uint16_t answer = (uint16_t)state->a + (uint16_t)state->memory[address];
		state->a = answer & 0xff;
		SetFlags(answer, true, state);
		state->pc++;
		state->cycles -= 7;
		break;
	}
	case 0x89:
		state->a += state->c + state->cc.cy;
		SetFlags(state->a, true, state);
		state->pc++;
		state->cycles -= 4;
		break;
	case 0xa0:
		state->a = state->a & state->b;
		SetFlags(state->a, true, state);
		state->pc++;
		state->cycles -= 4;
		break;
	case 0xa6:
	{
		uint16_t address = (state->h << 8) | state->l;
		state->a = state->a & state->memory[address];
		SetFlags(state->a, true, state);
		state->pc++;
		state->cycles -= 7;
		break;
	}
	case 0xa7:
		SetFlags(state->a, true, state);
		state->pc++;
		state->cycles -= 4;
		break;
	case 0xa8:
		state->a = state->a ^ state->b;
		SetFlags(state->a, true, state);
		state->pc++;
		state->cycles -= 4;
		break;
	case 0xaf:
		state->a = state->a ^ state->a;
		SetFlags(state->a, true, state);
		state->pc++;
		state->cycles -= 4;
		break;
	case 0xb0:
		state->a = state->a | state->b;
		SetFlags(state->a, true, state);
		state->pc++;
		state->cycles -= 4;
		break;
	case 0xb4:
		state->a |= state->h;
		SetFlags(state->a, true, state);
		state->pc++;
		state->cycles -= 4;
		break;
	case 0xb6:
	{
		uint16_t address = (state->h << 8) | state->l;
		state->a = state->a | state->memory[address];
		SetFlags(state->a, true, state);
		state->pc++;
		state->cycles -= 7;
		break;
	}
	case 0xb8:
		SetFlags(state->a - state->b, true, state);
		state->pc++;
		state->cycles -= 4;
		break;
	case 0xbe:
	{
		uint16_t address = (state->h << 8) | state->l;
		SetFlags(state->a - state->memory[address], true, state);
		state->pc++;
		state->cycles -= 7;
		break;
	}
	case 0xc0:
		if (state->cc.z == 0)
		{
			state->pc = (state->memory[state->sp] | (state->memory[state->sp + 1] << 8));
			state->sp += 2;
			state->cycles -= 11;
		}
		else
		{
			state->cycles -= 5;
			state->pc++;
		}
		break;
	case 0xc1:
		state->c = state->memory[state->sp];
		state->b = state->memory[state->sp + 1];
		state->sp += 2;
		state->pc++;
		state->cycles -= 10;
		break;
	case 0xc2:
		if (state->cc.z == 0)
		{
			state->pc = opcode[2] * 256 + opcode[1];
			state->cycles -= 15;
		}
		else
		{
			state->pc += 3;
			state->cycles -= 10;
		}
		
		break;
	case 0xc3: // JUMP
		state->pc = ((opcode[2] << 8) | opcode[1]);
		state->cycles -= 10;
		break;
	case 0xc4:
		if (state->cc.z == 0)
		{
			Call((opcode[2] << 8) | opcode[1], 3, state);
			state->cycles -= 18;
		}
		else 
		{
			state->pc += 3;
			state->cycles -= 11;
		}
		break;
	case 0xc5:
		state->memory[state->sp - 2] = state->c;
		state->memory[state->sp - 1] = state->b;
		state->sp -= 2;
		state->pc++;
		state->cycles -= 11;
		break;
	case 0xc6:
	{
		uint16_t answer = (uint16_t)state->a + (uint16_t)opcode[1];
		SetFlags(answer, true, state);
		state->a = answer;
		state->pc += 2;
		state->cycles -= 7;
		break;
	}
	case 0xc8:
		if (state->cc.z)
		{
			state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
			state->sp += 2;
			state->cycles -= 11;
		}
		else
		{
			state->pc++;
			state->cycles -= 5;
		}		
		break;
	case 0xc9: // RET - Get the return address from the stack and put it in the program counter
		state->pc = (state->memory[state->sp] | (state->memory[state->sp + 1] << 8));
		state->sp += 2;
		state->cycles -= 10;
		break;
	case 0xca:
		if (state->cc.z)
		{
			state->pc = (opcode[2] << 8) | opcode[1];
			state->cycles -= 15;
		}
		else
		{
			state->pc += 3;
			state->cycles -= 10;
		}		
		break;
	case 0xcc:
		if (state->cc.z)
		{
			Call((opcode[2] << 8) | opcode[1], 3, state);
			state->cycles -= 18;
		}
		else
		{
			state->pc += 3;
			state->cycles -= 11;
		}
		break;
	case 0xcd: // CALL
	{
		uint16_t returnAddress = state->pc + 3;
		state->memory[state->sp - 1] = (returnAddress >> 8) & 0xff;
		state->memory[state->sp - 2] = returnAddress & 0xff;
		state->sp -= 2;
		state->pc = (opcode[2] << 8) | opcode[1];
		state->cycles -= 17;
		break;
	}
	case 0xd0:
		if (state->cc.cy == 0)
		{
			Return(state);
			state->cycles -= 11;
		}
		else
		{
			state->pc++;
			state->cycles -= 5;
		}
		break;
	case 0xd1: // POP D
		state->e = state->memory[state->sp];
		state->d = state->memory[state->sp + 1];
		state->sp += 2;
		state->pc++;
		state->cycles -= 10;
		break;
	case 0xd2:
		if (!state->cc.cy)
		{
			state->pc = (opcode[2] << 8) | opcode[1];
			state->cycles -= 15;
		}
		else
		{
			state->pc += 3;
			state->cycles -= 10;
		}		
		break;
	case 0xd3:
		state->pc += 2;
		state->cycles -= 10;
		break;
	case 0xd5:
		state->memory[state->sp - 2] = state->e;
		state->memory[state->sp - 1] = state->d;
		state->sp -= 2;
		state->pc++;
		state->cycles -= 11;
		break;
	case 0xd6:
		state->a = state->a - opcode[1];
		SetFlags(state->a, true, state);
		state->pc += 2;
		state->cycles -= 7;
		break;
	case 0xd8: // RC - Return if carry flag is true
		if (state->cc.cy)
		{
			state->pc = (state->memory[state->sp] | (state->memory[state->sp + 1] << 8));
			state->sp += 2;
			state->cycles -= 11;
		}
		else
		{
			state->pc++;
			state->cycles -= 5;
		}		
		break;
	case 0xda:
		if (state->cc.cy)
		{
			state->pc = (opcode[2] << 8) | opcode[1];
			state->cycles -= 15;
		}
		else
		{
			state->pc += 3;
			state->cycles -= 10;
		}
		break;
	case 0xdb:
		if (opcode[1] == 1 || opcode[1] == 2 || opcode[1] == 3)
		{
			//state->a = 0;
		}
		else
		{
			printf("IN requesting port that is not 1 or 2 or 3");
			getchar();
		}
		state->pc += 2;
		state->cycles -= 10;
		break;
	case 0xde:
		state->a = state->a - opcode[1] - state->cc.cy;
		SetFlags(state->a, true, state);
		state->pc += 2;
		state->cycles -= 7;
		break;
	case 0xe1:
		state->l = state->memory[state->sp];
		state->h = state->memory[state->sp + 1];
		state->sp += 2;
		state->pc++;
		state->cycles -= 10;
		break;
	case 0xe3:
	{
		uint16_t hl = (state->h << 8) | state->l;
		state->l = state->memory[state->sp];
		state->memory[state->sp] = hl & 0xff;
		state->h = state->memory[state->sp + 1];
		state->memory[state->sp + 1] = (hl & 0xff00) >> 8;
		state->pc++;
		state->cycles -= 4;
		break;
	}
	case 0xe5:
		state->memory[state->sp - 2] = state->l;
		state->memory[state->sp - 1] = state->h;
		state->sp -= 2;
		state->pc++;
		state->cycles -= 11;
		break;
	case 0xe6:
		state->a = state->a & opcode[1];
		SetFlags(state->a, true, state);
		state->pc += 2;
		state->cycles -= 7;
		break;
	case 0xe8: // RPE - Return if parity flag is true
		if (state->cc.p)
		{
			state->pc = (state->memory[state->sp] | (state->memory[state->sp + 1] << 8));
			state->sp += 2;
			state->cycles -= 11;
		}
		else
		{
			state->pc++;
			state->cycles -= 5;
		}
		break;
	case 0xe9:
	{
		state->pc = (state->h << 8) | state->l;
		state->cycles -= 4;
		break;
	}
	case 0xeb:
	{
		uint8_t temp = state->d;
		state->d = state->h;
		state->h = temp;
		temp = state->e;
		state->e = state->l;
		state->l = temp;
		state->pc++;
		state->cycles -= 4;
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
		state->pc++;
		state->cycles -= 10;
		break;
	case 0xf3: // DI
		state->interrupt_enabled = false;
		state->pc++;
		state->cycles -= 4;
		break;
	case 0xf5:
		state->memory[state->sp - 1] = state->a;
		state->memory[state->sp - 2] = state->cc.z | (state->cc.s << 1) | (state->cc.p << 2) | (state->cc.cy << 3) | (state->cc.ac << 4);
		state->sp -= 2;
		state->pc++;
		state->cycles -= 11;
		break;
	case 0xf6:
		state->a |= opcode[1];
		SetFlags(state->a, true, state);
		state->pc += 2;
		state->cycles -= 7;
		break;
	case 0xfa: // JM adr - Jump if the sign condition code is 1
		if (state->cc.s == 1)
		{
			state->pc = ((opcode[2] << 8) | opcode[1]);
			state->cycles -= 15;
		}
		else
		{
			state->pc += 3;
			state->cycles -= 10;
		}		
		break;
	case 0xfb: // EI
		state->interrupt_enabled = true;
		state->pc++;
		state->cycles -= 4;
		break;
	case 0xfe:
	{
		SetFlags(state->a - opcode[1], true, state);
		state->pc += 2;
		state->cycles -= 7;
		break;
	}		
	default: 
		printf("Error: Instruction 0x%02x not implemented.", *opcode); getchar();
	}

	
}

void SetFlags(int answer, bool changeCarry, State8080* state)
{
	state->cc.z = ((answer & 0xff) == 0);
	state->cc.s = ((answer & 0x80) != 0);
	state->cc.p = answer % 2 == 0;
	if (changeCarry)
		state->cc.cy = (answer > 0xff || answer < 0);
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
				SDL_RenderDrawPoint(sdlHelper.renderer, w, (h + 1) * 8 - b - 1);
				byte /= 2;
			}
			videoPointer++;
		}
	}
	SDL_RenderPresent(sdlHelper.renderer);
}