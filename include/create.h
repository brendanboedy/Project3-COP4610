//Part 3: Create declarations
#pragma once

#include "imager.h"

// mkdir [DIRNAME]  - create a new directory in the cwd
void make_dir(fat_state *state, const char *dirname);
//creat [FILENAME] - create an empty file (size = 0) in the cwd
void create_ef(fat_state *state, const char *filename);
void build_short_name(const char *input, uint8_t dest[11]);
void set_fat_entry(fat_state *state, uint32_t cluster, uint32_t value);
