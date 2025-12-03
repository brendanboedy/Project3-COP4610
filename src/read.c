#define _POSIX_C_SOURCE 200809L
#include "read.h"

#include <bits/stdint-uintn.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "imager.h"

fat_file* open_file(char* target, char* flags, fat_state* state) {
    short_dir_entry* file_entry = find_entry(state, target);

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
    } else if (strcmp(flags, "-w")) {
        write = 1;
    } else if (strcmp(flags, "-rw")) {
        read = 1;
        write = 1;
    } else if (strcmp(flags, "-wr")) {
        read = 1;
        write = 1;
    } else {
        printf("Invalid flags, must be one of '-r', '-w', '-rw', '-wr'.");
    }

    char* full_path = strdup(state->working_dir);
    strcat(full_path, target);

    fat_file* f = malloc(sizeof(fat_file));
    f->filename = strdup(target);
    f->full_path = full_path;
    f->entry = file_entry;
    f->read = read;
    f->write = write;
    f->offset = 0;
    f->open = 1;

    return f;
}

void read_n_bytes(char* filename, uint32_t size, file_lst* file_lst, fat_state* state) {
    fat_file* files = file_lst->files;
    FAT32_Info* conf = &state->img_config;
    fat_file* file = NULL;
    for (int i = 0; i < file_lst->file_idx; ++i) {
        if (!files[i].open) {
            continue;
        }
        if (strcmp(filename, files[i].filename) != 0) {
            continue;
        }
        file = &files[i];
        break;
    }

    if (file == NULL) {
        printf("No openned file found with filename %s", filename);
        return;
    }

    if (!file->open) {
        printf("File %s is not open", filename);
    }

    uint32_t bytes_remaining = size;
    if (file->offset + size > file->entry->file_size) {
        bytes_remaining = (file->offset + size) - file->entry->file_size;
    }

    uint32_t offset = file->offset;

    uint32_t cluster_idx = cluster_from_entry_offset(file->entry, offset, state);
    uint32_t sector_idx = sector_from_entry_offset(cluster_idx, offset, state);
    uint32_t start_byte = offset % state->img_config.bytes_per_sector;
    uint32_t remaining_sectors =
        conf->sectors_per_cluster - (sector_idx % conf->sectors_per_cluster);
    printf(
        "start_byte = %d, bytes_remaining = %d, remaining_sectors = %d, cluster_idx = %d, "
        "sector_idx = %d\n",
        start_byte, bytes_remaining, remaining_sectors, cluster_idx, sector_idx);

    uint8_t* buffer = malloc(sizeof(uint32_t) * conf->bytes_per_sector);
    while (bytes_remaining > 0 && cluster_idx < END_CLUSTER_MIN && cluster_idx != 0) {
        for (uint32_t i = 0; i < remaining_sectors && bytes_remaining > 0; ++i) {
            read_sector(buffer, sector_idx + i, state);

            uint32_t bytes_to_print = conf->bytes_per_sector;
            if (bytes_remaining < conf->bytes_per_sector) {
                bytes_to_print = bytes_remaining;
            }

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
    printf("index\tfilename\tmode\toffset\tpath\n");
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

        printf("%d\t%s\t\t%s\t%d\t%s\n", i, f->filename, mode, f->offset, f->full_path);
    }
}

void lseek(char* filename, uint32_t offset, file_lst* file_lst) {
    fat_file* files = file_lst->files;
    fat_file* target_file = NULL;
    for (int i = 0; i < file_lst->file_idx; ++i) {
        if (strcmp(filename, files[i].filename) != 0) {
            continue;
        }
        target_file = &files[i];
        break;
    }

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

int is_file_open(file_lst* list, char* filename) {
    for (int i = 0; i < list->file_idx; ++i) {
        if (strcmp(list->files[i].filename, filename) != 0) {
            continue;
        }
        if (list->files[i].open) {
            return 1;
        }
    }
    return 0;
}