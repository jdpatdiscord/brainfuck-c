#ifdef __cplusplus
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#else
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#endif

#define ASSERT(cond, msg) if (cond) { fprintf(stderr, msg "\n"); exit(1); };

typedef struct
{
	char* input;
	size_t input_size;
} prog_data;

typedef struct bf_state
{
	uint8_t* cell;
	uint32_t cell_size;
	uint32_t ptr;
	uint32_t* loop_map;
	uint8_t* code;
	uint32_t code_size;
	uint32_t pc;
} bf_state;

typedef struct stk_state
{
	uint32_t* memory;
	uint32_t size;
	uint32_t top;
} stk_state;

stk_state* stk_create_now(uint32_t size)
{
	stk_state* stack = (stk_state*)malloc(sizeof(stk_state));
	stack->memory = (uint32_t*)malloc(size * 4);
	stack->size = size;
	stack->top = 0;
	return stack;
}

void stk_push(stk_state* stack, uint32_t n)
{
	stack->memory[stack->top++] = n;
}

uint32_t stk_pop(stk_state* stack)
{
	return stack->memory[--stack->top];
}

void stk_free(stk_state* stack)
{
	free(stack->memory);
	free(stack);
}

int bf_single_pass_validate(bf_state* s)
{
	unsigned begin_p = 0;
	unsigned end_p = 0;

	stk_state* stack = stk_create_now(s->code_size);

	for (uint32_t c = 0; c < s->code_size; c++)
	{
		uint8_t chr = s->code[c];
		if (chr == '[')
		{
			begin_p++;
			stk_push(stack, c);
			continue;
		}
		if (chr == ']')
		{
			if (begin_p == 0 || begin_p == end_p)
			{
				fprintf(stderr, "No open parenthesis\n");
				stk_free(stack);
				return 1;
			}
			end_p++;
			uint32_t opening = stk_pop(stack);
			s->loop_map[c] = opening;
			s->loop_map[opening] = c;
			continue;
		}
		if (chr != '+' && chr != '-' && chr != '.' && chr != '<' && chr != '>' && chr != ',')
		{
			fprintf(stderr, "Input not valid\n");
			stk_free(stack);
			return 1;
		};
	}
	if (begin_p != end_p)
	{
		fprintf(stderr, "Loops not balanced\n");
		stk_free(stack);
		return 1;
	}

	stk_free(stack);
	return 0;
}

int bf_run(bf_state* s)
{
	while(1)
	{
		if (s->pc >= s->code_size)
			return 0;
		uint8_t instr = s->code[s->pc];
		switch(instr)
		{
			case '+':
			{
				s->cell[s->ptr]++;
				s->pc++;
				continue;
			}
			case '-':
			{
				s->cell[s->ptr]--;
				s->pc++;
				continue;
			}
			case '<':
			{
				if (s->ptr == 0)
				{
					uint32_t new_size = s->cell_size + 1;
					uint8_t* new_cell = (uint8_t*)malloc(new_size);
					new_cell[new_size] = 0;
					memcpy(new_cell + 1, s->cell, s->cell_size);
					free(s->cell);
					s->cell_size = new_size;
					s->cell = new_cell;
					s->pc++;
					continue;
				}
				s->ptr--;
				s->pc++;
				continue;
			}
			case '>':
			{
				if (s->ptr + 1 == s->cell_size)
				{
					uint32_t new_size = s->cell_size + 1;
					uint8_t* new_cell = (uint8_t*)malloc(new_size);
					memset(new_cell, 0, new_size);
					memcpy(new_cell, s->cell, s->cell_size);
					free(s->cell);
					s->cell_size = new_size;
					s->cell = new_cell;
				}
				s->ptr++;
				s->pc++;
				continue;
			}
			case ']':
			{
				if (s->cell[s->ptr] != 0)
				{
					s->pc = s->loop_map[s->pc];
				}
				s->pc++;
				continue;
			}
			case '[':
			{
			    if (s->cell[s->ptr] == 0)
			    {
			        s->pc = s->loop_map[s->pc];
			    }
			    s->pc++;
			    continue;
			}
			case '.':
			{
				putc(s->cell[s->ptr], stdout);
				s->pc++;
				continue;
			}
			case ',':
			{
				s->cell[s->ptr] = getc(stdin);
				s->pc++;
				continue;
			}
			default:
			{
				s->pc++;
				continue;
			}
		}
	}
	return 0;
}

int bf_init(bf_state* s, const prog_data* pd)
{
	const uint32_t code_size = pd->input_size;
	uint8_t* new_code = (uint8_t*)malloc(code_size);
	uint32_t* new_loop_map = (uint32_t*)malloc(code_size * 4);
	memcpy(new_code, pd->input, code_size);
	
	s->code = new_code;
	s->code_size = code_size;
	s->loop_map = new_loop_map;
	s->cell = (uint8_t*)malloc(1);
	s->cell[0] = 0;
	s->cell_size = 1;
	s->ptr = 0;
	s->pc = 0;

	return 0;
}

int bf_exit(bf_state* s)
{
	free(s->loop_map);
	free(s->code);
	free(s->cell);
	free(s);
	return 0;
}

int prog_arghandle(int argc, char* argv[], prog_data* pd)
{
	if (argc > 1)
	{
		int idx = 1;
		for (; idx <= argc; ++idx)
		{
			char* arg = argv[idx];
			if (!strcmp(arg, "-f")) /* -f for brainfuck from file */
			{
				int fidx = ++idx;
				if (fidx <= argc)
				{
					char* ffile = argv[fidx];

					FILE* f = fopen(ffile, "r");
					ASSERT(!f, "Could not open file");

					fseek(f, 0, SEEK_END);
					int fsize = ftell(f);
					rewind(f);

					char* fdata = (char*)malloc(fsize);
					ASSERT(!fdata, "Failed to allocate buffer");
					bool failure = fread(fdata, 1, fsize, f) != fsize;
					ASSERT(failure, "Failed to read file");

					pd->input = fdata;
					pd->input_size = fsize - 1;
					return 0;
				}
				fprintf(stderr, "No file after -f\n");
			}
			if (!strcmp(arg, "-i")) /* -i for brainfuck from command line */
			{
				int fidx = ++idx;
				if (fidx <= argc)
				{
					char* finput = argv[fidx];

					pd->input = finput;
					pd->input_size = strlen(finput);
					return 0;
				}
				fprintf(stderr, "No string after -i\n");
				return 1;
			}
			fprintf(stderr, "Invalid argument\n");
			return 1;
		}
	}
	fprintf(stderr, "Not enough arguments\n");
	return 1;
}

int main(int argc, char* argv[])
{
	prog_data* pd = (prog_data*)malloc(sizeof(prog_data));
	int failure = prog_arghandle(argc, argv, pd);
	ASSERT(failure, "Argument handling failed, exiting");

    bf_state* s = (bf_state*)malloc(sizeof(bf_state));

    bf_init(s, pd);
    bf_single_pass_validate(s);
    #ifdef __cplusplus
    	auto begin = std::chrono::high_resolution_clock::now();
    #endif
    bf_run(s);
    #ifdef __cplusplus
	    auto end = std::chrono::high_resolution_clock::now();
	    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - begin);
	    printf("µs elapsed: %i\n", elapsed.count());
    #endif
    bf_exit(s);
    return 0;
}