#pragma once

#include "imager.h"
#include "read.h"

void write_file(char *filename, char *string, file_lst *files, fat_state *state);

void move_entry(fat_state *state, const char *oldname, const char *newname);
