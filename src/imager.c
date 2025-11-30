/**
 * Contains a lot of functions that will be helpful when handling
 * FAT stuff (i.e find_entry)
*/
#include "imager.h"

#include <bits/stdint-uintn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

short_dir_entry* find_entry(fat_state* state, char* target) {
    FAT32_Info* config = &state->img_config;
    const uint32_t MAX_ENTRIES = config->bytes_per_sector / sizeof(short_dir_entry);

    uint32_t current_cluster = state->working_dir_start_cluster;
    uint8_t* buffer = malloc(config->bytes_per_sector);

    while (current_cluster < END_CLUSTER_MIN && current_cluster != 0) {
        // Will continue until we've read all the clusters
        uint32_t start_sector_idx = first_sector_of_cluster(current_cluster, config);

        for (uint32_t i = 0; i < config->sectors_per_cluster; ++i) {
            uint32_t sector_idx = start_sector_idx + i;
            read_sector(buffer, sector_idx, state);

            short_dir_entry* entries = (short_dir_entry*)buffer;
            for (uint32_t j = 0; j < MAX_ENTRIES; ++j) {
                short_dir_entry* entry = &entries[j];

                if (is_final_entry(entry)) {
                    free(buffer);
                    return NULL;
                } else if (is_deleted_entry(entry) || is_long_filename(entry)) {
                    continue;
                }

                char fname_buff[13];
                translate_filename(entry->filename, fname_buff);
                if (strcasecmp(fname_buff, target) == 0) {
                    short_dir_entry* result = malloc(sizeof(short_dir_entry));
                    memcpy(result, entry, sizeof(short_dir_entry));
                    free(buffer);
                    return result;
                }
            }
        }

        current_cluster = get_next_cluster(current_cluster, state);
    }

    free(buffer);
    return NULL;
}

fat_state* mount_image(const char* filename) {
    FILE* image = fopen(filename, "rb+");
    if (!image) {
        perror("Error opening image file");
        return NULL;
    }

    fat_state* state = malloc(sizeof(fat_state));
    state->image = image;

    FAT32_Info* img_config = &state->img_config;  // just an alias

    fseek(image, 11, SEEK_SET);
    fread(&img_config->bytes_per_sector, 2, 1, image);

    fseek(image, 13, SEEK_SET);
    fread(&img_config->sectors_per_cluster, 1, 1, image);

    fseek(image, 14, SEEK_SET);
    fread(&img_config->reserved_sector_count, 2, 1, image);

    fseek(image, 16, SEEK_SET);
    fread(&img_config->num_fats, 1, 1, image);

    fseek(image, 32, SEEK_SET);
    fread(&img_config->total_sectors, 4, 1, image);

    fseek(image, 36, SEEK_SET);
    fread(&img_config->fat_size_32, 4, 1, image);

    fseek(image, 44, SEEK_SET);
    fread(&img_config->root_cluster, 4, 1, image);

    fseek(image, 0, SEEK_END);
    img_config->image_size = ftell(image);
    rewind(image);

    img_config->first_fat_sector = img_config->reserved_sector_count;
    img_config->first_data_sector = img_config->reserved_sector_count +
                                    (img_config->num_fats * img_config->fat_size_32);

    state->working_dir = malloc(1024 * sizeof(char));
    strcpy(state->working_dir, "/");
    state->working_dir_start_cluster = img_config->root_cluster;

    return state;
}
uint32_t first_cluster_of_entry(short_dir_entry* dir_entry) {
    return (dir_entry->first_cluster_hi << 16) + dir_entry->first_cluter_low;
}

uint32_t first_sector_of_cluster(uint32_t cluster_idx, const FAT32_Info* info) {
    return ((cluster_idx - 2) * info->sectors_per_cluster) + info->first_data_sector;
}

long long sector_byte_offset(uint32_t sector_idx, FAT32_Info* info) {
    return (long long)info->bytes_per_sector * sector_idx;
}

void read_sector(uint8_t* buffer, uint32_t sector_idx, fat_state* state) {
    long long byte_offset = sector_byte_offset(sector_idx, &state->img_config);

    fseek(state->image, byte_offset, SEEK_SET);
    fread(buffer, 1, state->img_config.bytes_per_sector, state->image);
}

uint32_t get_next_cluster(uint32_t current_cluster, fat_state* state) {
    uint32_t fat_cluster_offset = current_cluster * 4;  // 4 bytes / cluster
    uint32_t sector = state->img_config.first_fat_sector +
                      (fat_cluster_offset / state->img_config.bytes_per_sector);
    uint32_t yield_offset = fat_cluster_offset % state->img_config.bytes_per_sector;

    long pos = ((long)sector * state->img_config.bytes_per_sector) + yield_offset;

    uint32_t next_cluster_value = 0;
    fseek(state->image, pos, SEEK_SET);
    if (fread(&next_cluster_value, sizeof(uint32_t), 1, state->image) != 1) {
        printf("Reading caused huge explosion at get_next_cluster %d\n", current_cluster);
        return END_CLUSTER_CHAIN;
    }

    // This just removes the last 4 bits making the returned value a max of 28 bits
    return next_cluster_value & END_CLUSTER_CHAIN;
}

int is_final_entry(const short_dir_entry* entry) {
    return entry->filename[0] == END_OF_ENTRIES;
}

int is_deleted_entry(const short_dir_entry* entry) {
    return entry->filename[0] == DELETED_ENTRY;
}

int is_long_filename(const short_dir_entry* entry) {
    return entry->attributes == ATTR_LONG_DIR_NAME;
}

void translate_filename(const uint8_t* filename, char* output_buffer) {
    /**
     With filename "foo.md", FAT has: "FOO    MD ", the 'M' is on filename[8],
     In other words, ALL CAPS, first 8 bits are right-padded with spaces, last three bits right padded, with filename[8] == ' ' when NO extension
     
     output_buffer must be of size 13 (8 character filename + 3 character extensions + 1 character null term)
    */

    int output_idx = 0;
    for (int i = 0; i < 11; ++i) {
        if (i == 8 && filename[i] != ' ') {
            output_buffer[output_idx] = '.';
            output_idx += 1;
        }
        if (filename[i] != ' ') {
            output_buffer[output_idx] = filename[i];
            output_idx += 1;
        }
    }
    output_buffer[output_idx] = '\0';
}

int is_dir(const short_dir_entry* entry) {
    return (entry->attributes & ATTR_DIRECTORY) == ATTR_DIRECTORY;
}