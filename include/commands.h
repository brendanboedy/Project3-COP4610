#include <stdio.h>

#include "imager.h"
#include "lexer.h"

void handle_command(fat_state* state, tokenlist* tokens);

void print_info(const FAT32_Info* img_config);
void exit_program(FILE* image);
char* tokens_to_str(tokenlist* tokens, int start_idx);