//Part 3: Create declarations
#pragma once

#include "imager.h"


// mkdir [DIRNAME]  - create a new directory in the cwd
void make_dir(fat_state *state, const char *dirname);
//creat [FILENAME] - create an empty file (size = 0) in the cwd
void create_ef(fat_state *state, const char *filename);
