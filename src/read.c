#define _POSIX_C_SOURCE 200809L
#include "read.h"

#include <bits/stdint-uintn.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "imager.h"

fat_file* open_file(char* target, char* flags, fat_state* state) {
    fat_file* openned_file = get_open_file(target, state->openned_files, state);
    if (openned_file != NULL) {
        printf("File %s is already open.", target);
        return NULL;
    }

    uint32_t sector, offset;
    short_dir_entry* file_entry = find_entry(state, target, &sector, &offset);

    if (file_entry == NULL) {
        printf("No such file found %s", target);
        return NULL;
    }

    int read = 0;
    int write = 0;

    if (strlen(flags) > 3) {
        printf("Invalid flags, must be one of '-r', '-w', '-rw', '-wr'.");
        return NULL;
    }
    if (strcmp(flags, "-r") == 0) {
        read = 1;
    } else if (strcmp(flags, "-w") == 0) {
        write = 1;
    } else if (strcmp(flags, "-rw") == 0) {
        read = 1;
        write = 1;
    } else if (strcmp(flags, "-wr") == 0) {
        read = 1;
        write = 1;
    } else {
        printf("Invalid flags, must be one of '-r', '-w', '-rw', '-wr'.");
    }

    char* full_path = malloc(strlen(state->working_dir) + strlen(target) + 1);
    if (full_path == NULL) {
        printf("Memory allocation failed");
        return NULL;
    }
    strcpy(full_path, state->working_dir);
    strcat(full_path, target);

    fat_file* f = malloc(sizeof(fat_file));
    f->filename = strdup(target);
    f->full_path = full_path;
    f->entry = file_entry;
    f->read = read;
    f->write = write;
    f->offset = 0;
    f->open = 1;
    f->dir_entry_sector = sector;
    f->dir_entry_offset = offset;

    return f;
}

void read_n_bytes(char* filename, uint32_t size, file_lst* file_lst, fat_state* state) {
    FAT32_Info* conf = &state->img_config;
    fat_file* file = get_open_file(filename, file_lst, state);

    if (file == NULL) {
        printf("No file at %s. Make sure you are in the directory containing the openned file.",
               filename);
        return;
    }

    if (!file->open) {
        printf("File %s is not open", filename);
        return;
    }

    if (!file->read) {
        printf("File %s is not open for reading", filename);
        return;
    }

    uint32_t bytes_remaining = size;
    if (file->offset >= file->entry->file_size) {
        bytes_remaining = 0;
    } else if (file->offset + size > file->entry->file_size) {
        bytes_remaining = file->entry->file_size - file->offset;
    }

    uint32_t offset = file->offset;

    uint32_t cluster_idx = cluster_from_entry_offset(file->entry, offset, state);
    uint32_t sector_idx = sector_from_entry_offset(cluster_idx, offset, state);
    uint32_t start_byte = offset % state->img_config.bytes_per_sector;

    uint32_t offset_in_cluster = offset % (conf->bytes_per_sector * conf->sectors_per_cluster);
    uint32_t sector_offset_in_cluster = offset_in_cluster / conf->bytes_per_sector;
    uint32_t remaining_sectors = conf->sectors_per_cluster - sector_offset_in_cluster;

    uint8_t* buffer = malloc(conf->bytes_per_sector);
    while (bytes_remaining > 0 && cluster_idx < END_CLUSTER_MIN && cluster_idx != 0) {
        for (uint32_t i = 0; i < remaining_sectors && bytes_remaining > 0; ++i) {
            read_sector(buffer, sector_idx + i, state);

            uint32_t available = conf->bytes_per_sector - start_byte;
            uint32_t bytes_to_print = (bytes_remaining < available) ? bytes_remaining : available;

            fwrite(buffer + start_byte, 1, bytes_to_print, stdout);

            start_byte = 0;
            file->offset += bytes_to_print;
            bytes_remaining -= bytes_to_print;
        }
        cluster_idx = get_next_cluster(cluster_idx, state);
        sector_idx = first_sector_of_cluster(cluster_idx, &state->img_config);
        remaining_sectors = state->img_config.sectors_per_cluster;
    }

    free(buffer);
}

void list_open_files(file_lst* files) {
    printf("%-7s %-20s %-6s %-8s %s\n", "index", "filename", "mode", "offset", "path");
    uint32_t printed_idx = 0;
    for (int i = 0; i < files->file_idx; ++i) {
        fat_file* f = &files->files[i];
        if (!f->open) {
            continue;
        }

        char mode[4] = {'-'};
        if (f->read) {
            strcat(mode, "r");
        }
        if (f->write) {
            strcat(mode, "w");
        }
        printf("%-7d %-20s %-6s %-8d %s\n", printed_idx, f->filename, mode, f->offset,
               f->full_path);

        printed_idx += 1;
    }
}
void close_file(char* filename, file_lst* open_files, fat_state* state) {
    fat_file* file = get_open_file(filename, open_files, state);

    if (file == NULL) {
        printf("File %s not found in open files", filename);
        return;
    }

    if (!file->open) {
        printf("File %s is already closed.", filename);
        return;
    }

    free(file->filename);
    free(file->full_path);
    free(file->entry);
    file->filename = NULL;
    file->full_path = NULL;

    file->open = 0;
    file->entry = NULL;
}

void lseek(char* filename, uint32_t offset, file_lst* file_lst, fat_state* state) {
    fat_file* target_file = get_open_file(filename, file_lst, state);

    if (target_file == NULL) {
        printf("No openned file found with filename %s", filename);
        return;
    }

    if (target_file->entry->file_size <= offset) {
        printf("Provided offset is greater than or equal to filesize.\n");
        return;
    }

    target_file->offset = offset;
}

file_lst* create_file_lst(void) {
    file_lst* list = (file_lst*)malloc(sizeof(file_lst));
    if (list == NULL) {
        return NULL;
    }

    list->size = 10;
    list->file_idx = 0;
    list->files = (fat_file*)malloc(sizeof(fat_file) * list->size);
    memset(list->files, 0, sizeof(fat_file) * list->size);

    if (list->files == NULL) {
        free(list);
        return NULL;
    }

    return list;
}

void add_file_to_lst(fat_file* file, file_lst* files) {
    if (files == NULL || file == NULL) {
        return;
    }

    if (files->file_idx >= files->size) {
        int new_size = files->size * 2;
        fat_file* new_array = (fat_file*)realloc(files->files, sizeof(fat_file) * new_size);

        if (new_array == NULL) {
            return;
        }

        files->files = new_array;
        files->size = new_size;
    }

    files->files[files->file_idx] = *file;
    files->file_idx++;
}