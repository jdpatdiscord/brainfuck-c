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

// if we ever wanted to take brainfuck speed seriously, do a pass to precalculate cell bounds & "JIT" it somehow
// wherein the cell decrement/increments are pre-calculated (never zero)


//--<-<<+[+[<+>--->->->-<<<]>]<<--.<++++++.<<-..<<.<+.>>.>>.<<<.+++.>>.>>-.<<<+.
int bf_single_pass_validate(bf_state* s)
{
	unsigned begin_p = 0;
	unsigned end_p = 0;

	stk_state* stack = stk_create_now(s->code_size);

	for (uint32_t c = 0; c < s->code_size; c++)
	{
	    //printf("%c = %c\n", c, s->code[c]);
		uint8_t chr = s->code[c];
		if (chr == '[')
		{
			printf("%c", chr);
			begin_p++;
			stk_push(stack, c);
			continue;
		}
		if (chr == ']')
		{
			printf("%c", chr);
			if (begin_p == 0 || begin_p == end_p)
			{
				// error, no open parenthesis
				stk_free(stack);
				return 1;
			}
			end_p++;
			uint32_t opening = stk_pop(stack);
			s->loop_map[c] = opening;
			//printf("at %i is %c\n", c, s->code[c]);
			s->loop_map[opening] = c;
			//printf("at %i is %c\n", opening, s->code[opening]);
			continue;
		}
		if (chr != '+' && chr != '-' && chr != '.' && chr != '<' && chr != '>' && chr != ',' && chr != '[' && chr != ']')
		{
			// error, character not valid
			//stk_free(stack);
			//return 1;
		} else printf("%c", chr);
	}
	if (begin_p != end_p)
	{
		// error, loops not balanced
		stk_free(stack);
		return 1;
	}
	// successful
	stk_free(stack);
	printf("\n");
	return 0;
}

int bf_run(bf_state* s)
{
	while(1)
	{
		if (s->pc >= s->code_size)
			return 0;
		//printf("switching, pc = %i\n", s->pc);
		uint8_t instr = s->code[s->pc];
		switch(instr)
		{
			case '+':
			{
				//printf("%c: ptr = %02i, pc = %02i, cell_size = %02i\n", instr, s->ptr, s->pc, s->cell_size);
				s->cell[s->ptr]++;
				s->pc++;
				continue;
			}
			case '-':
			{
				//printf("%c: ptr = %02i, pc = %02i, cell_size = %02i\n", instr, s->ptr, s->pc, s->cell_size);
				s->cell[s->ptr]--;
				s->pc++;
				continue;
			}
			case '<':
			{
				//printf("%c: ptr = %02i, pc = %02i, cell_size = %02i\n", instr, s->ptr, s->pc, s->cell_size);
				if (s->ptr == 0) // real ptr is 0, reallocate at + 1
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
				s->ptr--; // not 0? do normal
				s->pc++;
				continue;
			}
			case '>':
			{
				//printf("%c: ptr = %02i, pc = %02i, cell_size = %02i\n", instr, s->ptr, s->pc, s->cell_size);
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
				//printf("%c: ptr = %02i, pc = %02i, cell_size = %02i\n", instr, s->ptr, s->pc, s->cell_size);
				if (s->cell[s->ptr] != 0)
				{
					s->pc = s->loop_map[s->pc];
				}
				s->pc++;
				continue;
			}
			case '[':
			{
				//printf("%c: ptr = %02i, pc = %02i, cell_size = %02i\n", instr, s->ptr, s->pc, s->cell_size);
			    if (s->cell[s->ptr] == 0)
			    {
			        s->pc = s->loop_map[s->pc];
			    }
			    s->pc++;
			    continue;
			}
			case '.':
			{
				//printf("%c: ptr = %02i, pc = %02i, cell_size = %02i\n", instr, s->ptr, s->pc, s->cell_size);
				putc(s->cell[s->ptr], stdout);
				s->pc++;
				continue;
			}
			case ',':
			{
				//printf("%c: ptr = %02i, pc = %02i, cell_size = %02i\n", instr, s->ptr, s->pc, s->cell_size);
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

int bf_init(bf_state* s, const uint8_t* code)
{
	const uint32_t code_size = strlen((const char*)code);
	uint8_t* new_code = (uint8_t*)malloc(code_size);
	uint32_t* new_loop_map = (uint32_t*)malloc(code_size * 4);
	memcpy(new_code, code, code_size);
	
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

int main(int argc, char* argv[])
{
    printf("Input -> %s\n", argv[1]);
    bf_state* s = (bf_state*)malloc(sizeof(bf_state));
    //bf_init(s, (const uint8_t*)("[.....]+>>>[-]<[-]++++++++[->+++++++++<]>.----[--<+++>]<-.+++++++.><.+++.[-][[-]>[-]+++++++++[<+++++>-]<+...--------------.>++++++++++[<+++++>-]<.+++.-------.>+++++++++[<----->-]<.-.>++++++++[<+++++++>-]<++.-----------.--.-----------.+++++++.----.++++++++++++++.>++++++++++[<----->-]<..[-]++++++++++.[-]+++++++[.,]-]<<-++++++++[>++++++++<-]>[<++++>-]+<[>-<[>++++<-]>[<++++++++>-]<[>++++++++<-]+>[>>+[<]>-<++>[-]++++++[<+++++++>-]<.------------.[-]<[>+<[-]]>++++++++>[-]++++++++++[<+++++++++++>-]<.--------.+++.------.--------.[-]<[-]<[-]>]<[>>++>[-]+++++[<++++++>-]<.[-]+>>++++[-<<[->++++<]>[-<+>]>]<+<[>>>[-]++++++++++[<+++++++++++>-]<+++++++++.--------.+++.------.--------.[-]<[-]<[-]]>[>>[-]>[-]+++++++++[<++++++++++>-]<.>++++[<+++++>-]<+.--.-----------.+++++++.----.[-]<<[-]]<<<[-]]>[-]<]>[>+++++[>++++<-]>[<+++++++++++++>-]<----[[-]>[-]+++++[<++++++>-]<++.>+++++[<+++++++>-]<.>++++++[<+++++++>-]<+++++.>++++[<---->-]<-.++.++++++++.------.-.[-]]++>[-]+++++[<++++++>-]<.[-]>++[>++<-]>[<<+>>[-<<[>++++<-]>[<++++>-]>]]<<[>++++[>---<++++]>++.[<++>+]<.[>+<------]>.+++.[<--->++]<--.[-]<[-]][-]>[-]>[-]<++[>++++++++<-]>[<++++++++>-]<[>++++++++<-]>[<++++++++>-]<[<++++++++>-]<[[-]>[-]+++++++++[<++++++++++>-]<.>++++[<+++++>-]<+.--.-----------.+++++++.----.>>[-]<+++++[>++++++<-]>++.<<[-]][-]<[>+<[-]]>+++++>[-]+++++++++[<+++++++++>-]<.>++++[<++++++>-]<.+++.------.--------.[-]<[-]]<+[[>]<-]>>[<[[<[[<[[<[,]]]<]<]<]<][-]]+>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>-[+<-]+++[->++++++<]>[-<+++++++>]<[->>[>]+[<]<]>>[->]<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<--[>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>++<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<+]+>----[++++>----]-[+<-]+[>]<-[-<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<++++>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>][..][>++++++[<+++>-]<+[>+++++++++<-]>+[[->+>+<<]>>[-<<+>>]<[<[->>+<<]+>[->>+<<]+[>]<-]<-]<[-<]]>+[>[[-]+++>[-]++++++-[<++++++>-]<.<[-]>[-]]+<]<++++++++[[>]+[<]>-]>[>]<[[-]<]+[[>]<-[,]+[>]<-[]][-]>[-]>[-]>[-]>[-]>[-]>[-]>[-]<<<<<<<++++[->>++++>>++++>>++++<<<<<<]++++++++++++++>>>>+>>++<<<<<<[->>[->+>[->+>[->+>+[>>>+<<]>>[-<<+>]<-[<<<[-]<<[-]<<[-]<<[-]>>>[-]>>[-]>>[-]>->+]<<<]>[-<+>]<<<]>[-<+>]<<<]>[-<+>]<<<]>+>[[-]<->]<[->>>>>>>[-<<<<<<<<+>>>>>>>>]<<<<<<<]<>+<[>[-]>[-]+++++[<++++++>-]<++.[-]<[[->>+<<]>>[-<++>[-<+>[-<+>[-<+>[-<+>[-<+>[-<+>[-<+>[-<+>[<[-]+>->+<[<-]]]]]]]]]]>]<<[>++++++[<++++++++>-]<-.[-]<]]]>[>[-]>[-]++++[<++++++++>-]<[<++++++++>-]>+++[<++++++++>-]<+++++++[<-------->-]<------->+<[[-]>-<]>[>[-]<[-]++++[->++++++++<]>.++++++[-<++>]<.[-->+++<]>++.<++++[>----<-]>.[-]<]<[-]>[-]++++++++[<++++++++>-]<[>++++<-]+>[<->[-]]<[>[-]<[-]++++[->++++++++<]>.---[-<+++>]<.---.--------------.[-->+<]>--.[-]<]]<++++++++[[>]+[<]>-]>[>]<[[-]<][-]++++++++++.[-][,,.]<<"));
    bf_init(s, (const uint8_t*)("--<-<<+[+[<+>--->->->-<<<]>]<<--.<++++++.<<-..<<.<+.>>.>>.<<<.+++.>>.>>-.<<<+."));
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