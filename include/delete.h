#pragma once

#include "imager.h"

//Deletes a regular file in the cwd
void remove_file(fat_state *state, const char *filename);

//Deletes an (empty) directory in the cwd
void remove_dir(fat_state *state, const char *dirname);
