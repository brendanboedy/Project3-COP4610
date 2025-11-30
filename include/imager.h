/*
 * Defines helper functions for reading/writing FAT
 */
#pragma once
#include <bits/stdint-uintn.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    // stuff found in bios parameter block or values
    // computed that will be useful later (hence, Info)
    uint16_t bytes_per_sector;       // offset 11–12
    uint8_t sectors_per_cluster;     // offset 13
    uint16_t reserved_sector_count;  // offset 14–15
    uint8_t num_fats;                // offset 16
    uint32_t total_sectors;          // offset 32–35
    uint32_t fat_size_32;            // offset 36–39
    uint32_t root_cluster;           // offset 44–47
    uint32_t image_size;             // computed via ftell()

    uint32_t first_fat_sector;
    uint32_t first_data_sector;  // TODO: Set in mount_image()

} FAT32_Info;

#define END_OF_ENTRIES 0x00
#define DELETED_ENTRY 0x05

// The following are the flags for the attributes member
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_DIR_NAME 0x0F

// For masking the FAT cluster position, and determine end cluster chain
#define END_CLUSTER_CHAIN 0x0FFFFFFF

// If current_cluster >= END_CLUSTER then we are finished with cluster-chain
#define END_CLUSTER_MIN 0x0FFFFFF8

typedef struct {
    uint8_t filename[11];       // 0 - 11 (last index not included)
    uint8_t attributes;         // 11 - 12
    uint8_t __filler_a[8];      // 12-20
    uint16_t first_cluster_hi;  // 20 - 22
    uint8_t __filler_b[4];      // 22-26
    uint16_t first_cluter_low;  // 26 - 28;
    uint32_t file_size;         // 28-32
} __attribute__((packed)) short_dir_entry;

typedef struct {
    FAT32_Info img_config;
    FILE* image;

    char* working_dir;
    // basically the FAT-interpretable version of working dir:
    uint32_t working_dir_start_cluster;
} fat_state;

fat_state* mount_image(const char* filename);

/*
 * Finds an entry in the current directory with target name
 *
 * IMPORTANT: Make sure you use this whenever needed. Very useful.
*/
short_dir_entry* find_entry(fat_state* state, char* target);

uint32_t first_cluster_of_entry(short_dir_entry* dir_entry);
uint32_t first_sector_of_cluster(uint32_t cluster_idx, const FAT32_Info* info);
long long sector_byte_offset(uint32_t sector_idx, FAT32_Info* info);

void free_state(fat_state* state);
void read_sector(uint8_t* buffer, uint32_t sector_idx, fat_state* state);
uint32_t get_next_cluster(uint32_t current_cluster, fat_state* state);
int is_final_entry(const short_dir_entry* entry);
int is_deleted_entry(const short_dir_entry* entry);
int is_long_filename(const short_dir_entry* entry);
int is_dir(const short_dir_entry* entry);

void translate_filename(const uint8_t* filename, char* output_buffer);