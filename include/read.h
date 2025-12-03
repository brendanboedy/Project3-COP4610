#pragma once

#include <bits/stdint-uintn.h>

#include "imager.h"

fat_file* open_file(char* target, char* flags, fat_state* state);

void read_n_bytes(char* filename, uint32_t size, file_lst* file_lst, fat_state* state);
void lseek(char* filename, uint32_t offset, file_lst* file_lst);
void add_file_to_lst(fat_file* file, file_lst* file_lst);
file_lst* create_file_lst(void);

void list_open_files(file_lst* files);
int is_file_open(file_lst* list, char* filename);