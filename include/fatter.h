#pragma once
#include <stdint.h>
#include <stdio.h>
#include "imager.h"


void print_info(void);
void exit_program(void);

int allocate_free_cluster(fat_state *state, uint32_t *out_cluster);
void clear_cluster(fat_state *state, uint32_t cluster);
