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

typedef struct
{
	char* buffer;
	size_t size;
	size_t pointer;
} buffer;

void buf_new(buffer* buf)
{
	if (buf->buffer != NULL)
		free(buf->buffer);
	buf->buffer = (char*)calloc(16, 1);
	buf->size = 64;
	buf->pointer = 0;
}

void buf_free(buffer* buf)
{
	if (buf->buffer)
	{
		free(buf->buffer);
		buf->buffer = NULL;
	}
	buf->size = 0;
	buf->pointer = 0;
}

void buf_checkresize(buffer* buf, size_t extra)
{
	size_t original = buf->size;
	while(buf->pointer + extra >= buf->size)
		buf->size *= 2;
	if (buf->size == original)
		return;
	char* newbuf = (char*)malloc(buf->size);
	memcpy(newbuf, buf->buffer, original);
	free(buf->buffer);
	buf->buffer = newbuf;
}

void buf_addchar(buffer* buf, char c)
{
	buf_checkresize(buf, sizeof(c));
	buf->buffer[++buf->pointer] = c;
}

#define ASSERT(cond, msg) if (cond) { fputs(msg "\n", stderr); exit(1); };

typedef struct
{
	char* input;
	size_t input_size;
} prog_data;

typedef struct bf_state
{
	buffer output;
	uint8_t* code;
	uint32_t code_size;
	uint8_t* cell;
	uint32_t cell_size;
	uint32_t* loop_map;
	uint32_t ptr;
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
	stack->memory = NULL;
	free(stack);
}

int bf_single_pass_validate(bf_state* s)
{
	unsigned begin_p = 0;
	unsigned end_p = 0;

	stk_state* stack = stk_create_now(s->code_size);

    uint32_t c = 0;
	for (; c < s->code_size; ++c)
	{
		uint8_t chr = s->code[c];
		if (chr == '[')
		{
			++begin_p;
			stk_push(stack, c);
			continue;
		}
		if (chr == ']')
		{
			if (begin_p == 0 || begin_p == end_p)
			{
				fputs("No open parenthesis\n", stderr);
				stk_free(stack);
				return 1;
			}
			++end_p;
			uint32_t opening = stk_pop(stack);
			s->loop_map[c] = opening;
			s->loop_map[opening] = c;
			continue;
		}
		if (chr != '+' && chr != '-' && chr != '.' && chr != '<' && chr != '>' && chr != ',')
		{
			fputs("Input not valid\n", stderr);
			stk_free(stack);
			return 1;
		};
	}
	if (begin_p != end_p)
	{
		fputs("Loops not balanced\n", stderr);
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
					uint8_t* new_cell = (uint8_t*)calloc(new_size, 1);
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
					uint8_t* new_cell = (uint8_t*)calloc(new_size, 1);
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
				buf_addchar(&s->output, s->cell[s->ptr]);
				putc(s->cell[s->ptr], stdout);
				s->pc++;
				continue;
			}
			case ',':
			{
				fprintf(stdout, "%.*s", s->output.pointer, (char*)s->output.buffer);
				buf_new(&s->output);
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

	buf_new(&s->output);
	s->code = new_code;
	s->code_size = code_size;
	s->cell = (uint8_t*)calloc(1, 1);
	s->cell_size = 1;
	s->loop_map = new_loop_map;
	s->ptr = 0;
	s->pc = 0;

	return 0;
}

int bf_exit(bf_state* s)
{
	free(s->loop_map);
	free(s->code);
	free(s->cell);
	s->loop_map = NULL;
	s->code = NULL;
	s->cell = NULL;
	buf_free(&s->output);
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

					FILE* f = fopen(ffile, "rb");
					ASSERT(!f, "Could not open file");

					fseek(f, 0, SEEK_END);
					int fsize = ftell(f);
					rewind(f);

					char* fdata = (char*)malloc(fsize);
					ASSERT(!fdata, "Failed to allocate buffer");
					int failure = fread(fdata, 1, fsize, f) != fsize;
					ASSERT(failure, "Failed to read file");

					pd->input = fdata;
					pd->input_size = fsize - 1;
					return 0;
				}
				fputs("No file after -f\n", stderr);
				return 1;
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
				fputs("No string after -i\n", stderr);
				return 1;
			}
			fputs("Invalid argument\n", stderr);
			return 1;
		}
	}
	fputs("Not enough arguments\n", stderr);
	return 1;
}

int main(int argc, char* argv[])
{
	prog_data* pd = (prog_data*)malloc(sizeof(prog_data));
	int failure = prog_arghandle(argc, argv, pd);
	ASSERT(failure, "Argument handling failed, exiting");

    bf_state* s = (bf_state*)calloc(sizeof(bf_state), 1);

    #ifdef __cplusplus
    	auto begin = std::chrono::high_resolution_clock::now();
    #endif
    bf_init(s, pd);
    bf_single_pass_validate(s);
    bf_run(s);
    #ifdef __cplusplus
	    auto end = std::chrono::high_resolution_clock::now();
	    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - begin);
	    printf("\nmicroseconds elapsed: %i", elapsed.count());
    #endif
    putc('\n', stdout);
    bf_exit(s);
    free(pd);
    return 0;
}
