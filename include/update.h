#pragma once

#include <bits/stdint-uintn.h>

#include "imager.h"
#include "read.h"

void write_file(char *filename, char *string, file_lst *files, fat_state *state);

void move_entry(fat_state *state, const char *oldname, const char *newname);
void move_entry_to_dir(short_dir_entry *file_entry, short_dir_entry *dir_entry, fat_state *state,
                       uint32_t og_sector, uint32_t og_offset);