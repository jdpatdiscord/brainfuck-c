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

void* malloc_impl(size_t size)
{
	return malloc(size);
}

void* memset_impl(void* dst, int value, size_t bytecount)
{
	return memset(dst, value, bytecount);
}

void* calloc_impl(size_t nelems, size_t elemsize)
{
	void* block = malloc_impl(nelems * elemsize);
	memset_impl(block, 0, nelems * elemsize);
	return block;
}
void free_impl(void* block)
{
	free(block);
}

void* memcpy_impl(void* dst, void* src, size_t bytecount)
{
	return memcpy(dst, src, bytecount);
}

typedef struct
{
	char* buffer;
	size_t size;
	size_t pointer;
} buffer;


void buf_new(buffer* buf)
{
	if (buf->buffer != NULL)
		free_impl(buf->buffer);
	buf->size = 64;
	buf->buffer = (char*)malloc_impl(buf->size);
	buf->pointer = 0;
}

void buf_free_impl(buffer* buf)
{
	if (buf->buffer)
	{
		free_impl(buf->buffer);
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
	char* newbuf = (char*)malloc_impl(buf->size);
	memcpy_impl(newbuf, buf->buffer, original);
	free_impl(buf->buffer);
	buf->buffer = newbuf;
}

void buf_addchar(buffer* buf, char c)
{
	buf_checkresize(buf, sizeof(c));
	buf->buffer[buf->pointer++] = c;
}

#define ASSERT(cond, msg) if (cond) { fputs_impl(msg "\n", stderr); exit(1); };

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
	stk_state* stack = (stk_state*)malloc_impl(sizeof(stk_state));
	stack->memory = (uint32_t*)malloc_impl(size * 4);
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

void stk_free_impl(stk_state* stack)
{
	free_impl(stack->memory);
	stack->memory = NULL;
	free_impl(stack);
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
				fputs_impl("No open parenthesis\n", stderr);
				stk_free_impl(stack);
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
			fputs_impl("Input not valid\n", stderr);
			stk_free_impl(stack);
			return 1;
		};
	}
	if (begin_p != end_p)
	{
		fputs_impl("Loops not balanced\n", stderr);
		stk_free_impl(stack);
		return 1;
	}

	stk_free_impl(stack);
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
					uint8_t* new_cell = (uint8_t*)calloc_impl(new_size, 1);
					memcpy_impl(new_cell + 1, s->cell, s->cell_size);
					free_impl(s->cell);
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
					uint8_t* new_cell = (uint8_t*)calloc_impl(new_size, 1);
					memcpy_impl(new_cell, s->cell, s->cell_size);
					free_impl(s->cell);
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
				s->pc++;
				continue;
			}
			case ',':
			{
				void* str = calloc_impl(s->output.pointer + 1, 1);
				memcpy_impl(str, s->output.buffer, s->output.pointer);
				fputs_impl(str, stdout);
				free_impl(str);
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
	uint8_t* new_code = (uint8_t*)malloc_impl(code_size);
	uint32_t* new_loop_map = (uint32_t*)malloc_impl(code_size * 4);
	memcpy_impl(new_code, pd->input, code_size);

	buf_new(&s->output);
	s->code = new_code;
	s->code_size = code_size;
	s->cell = (uint8_t*)calloc_impl(1, 1);
	s->cell_size = 1;
	s->loop_map = new_loop_map;
	s->ptr = 0;
	s->pc = 0;

	return 0;
}

int bf_exit(bf_state* s)
{
	free_impl(s->loop_map);
	free_impl(s->code);
	free_impl(s->cell);
	s->loop_map = NULL;
	s->code = NULL;
	s->cell = NULL;
	
	void* str = calloc_impl(s->output.pointer + 1, 1);
	memcpy_impl(str, s->output.buffer, s->output.pointer);
	fputs_impl(str, stdout);
	free_impl(str);

	buf_free_impl(&s->output);
	free_impl(s);
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

					char* fdata = (char*)malloc_impl(fsize);
					ASSERT(!fdata, "Failed to allocate buffer");
					int failure = fread(fdata, 1, fsize, f) != fsize;
					ASSERT(failure, "Failed to read file");

					pd->input = fdata;
					pd->input_size = fsize - 1;
					return 0;
				}
				fputs_impl("No file after -f\n", stderr);
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
				fputs_impl("No string after -i\n", stderr);
				return 1;
			}
			fputs_impl("Invalid argument\n", stderr);
			return 1;
		}
	}
	fputs_impl("Not enough arguments\n", stderr);
	return 1;
}

int main(int argc, char* argv[])
{
	prog_data* pd = (prog_data*)malloc_impl(sizeof(prog_data));
	int failure = prog_arghandle(argc, argv, pd);
	ASSERT(failure, "Argument handling failed, exiting");

    bf_state* s = (bf_state*)calloc_impl(sizeof(bf_state), 1);

    //#ifdef __cplusplus
    //	auto begin = std::chrono::high_resolution_clock::now();
    //#endif
    bf_init(s, pd);
    bf_single_pass_validate(s);
    bf_run(s);
    //#ifdef __cplusplus
	//    auto end = std::chrono::high_resolution_clock::now();
	//    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - begin);
	//    printf("\nmicroseconds elapsed: %i", elapsed.count());
    //#endif
    putc('\n', stdout);
    bf_exit(s);
    free_impl(pd);
    return 0;
}
