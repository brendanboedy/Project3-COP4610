#pragma once

#include "imager.h"

void change_dir(fat_state* state, char* dest_dir);
void list_entries_in_dir(fat_state* state);