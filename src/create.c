//Part 3: Create Method Definitions
#include "create.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "imager.h"

/* --------- small FAT / directory helpers (local to this file) --------- */
//Build a short name
static void build_short_name(const char *input, uint8_t dest[11]) {
    memset(dest, ' ', 11);
    size_t len = strlen(input);
    if (len > 8) {
        len = 8;
    }
    for (size_t i = 0; i < len; ++i) {
        dest[i] = (uint8_t)toupper((unsigned char)input[i]);
    }
}

//Write a 32-bit value into the FAT entry for the given cluster
static void set_fat_entry(fat_state *state, uint32_t cluster, uint32_t value) {
    FAT32_Info *cfg = &state->img_config;
    uint32_t fat_offset_bytes = cluster * 4; //4 bytes per FAT32 entry
    uint32_t sector = cfg->first_fat_sector + (fat_offset_bytes / cfg->bytes_per_sector);
    uint32_t offset_in_sector = fat_offset_bytes % cfg->bytes_per_sector;
    long pos = (long)sector * cfg->bytes_per_sector + offset_in_sector;
    fseek(state->image, pos, SEEK_SET);
    fwrite(&value, sizeof(uint32_t), 1, state->image);
}

//Search for free FAT entry 
//Marks  chosen cluster as EOC and returns its index via out_cluster.
static int allocate_free_cluster(fat_state *state, uint32_t *out_cluster) {
    FAT32_Info *cfg = &state->img_config;

    //# of 32-bit entries in one FAT
    uint32_t fat_entries = (cfg->fat_size_32 * cfg->bytes_per_sector) / 4;

    for (uint32_t cluster = 2; cluster < fat_entries; ++cluster) {
        uint32_t fat_offset_bytes = cluster * 4;
        uint32_t sector = cfg->first_fat_sector + (fat_offset_bytes / cfg->bytes_per_sector);
        uint32_t offset_in_sector = fat_offset_bytes % cfg->bytes_per_sector;
        long pos = (long)sector * cfg->bytes_per_sector + offset_in_sector;
        uint32_t value = 0;
        fseek(state->image, pos, SEEK_SET);
        fread(&value, sizeof(uint32_t), 1, state->image);

        if (value == 0) {
            //Found free cluster: mark as end of cluster chain
            uint32_t eoc = END_CLUSTER_CHAIN;
            fseek(state->image, pos, SEEK_SET);
            fwrite(&eoc, sizeof(uint32_t), 1, state->image);

            *out_cluster = cluster;
            return 0;
        }
    }
    //no free cluster
    return -1;
}

//Zero-out every byte in the given cluster
static void clear_cluster(fat_state *state, uint32_t cluster) {
    FAT32_Info *cfg = &state->img_config;
    uint32_t first_sector = first_sector_of_cluster(cluster, cfg);
    uint32_t sectors_per_cluster = cfg->sectors_per_cluster;
    uint16_t bytes_per_sector = cfg->bytes_per_sector;
    uint8_t *zero_buf = malloc(bytes_per_sector);
    if (!zero_buf) return;
    memset(zero_buf, 0, bytes_per_sector);

    for (uint32_t i = 0; i < sectors_per_cluster; ++i) {
        uint32_t sector = first_sector + i;
        long pos = (long)sector * bytes_per_sector;
        fseek(state->image, pos, SEEK_SET);
        fwrite(zero_buf, 1, bytes_per_sector, state->image);
    }
    free(zero_buf);
}

//Append a new short_dir_entry into the current working directory.
//Or make new cluster and insert entry there
static int append_entry_to_cwd(fat_state *state, const short_dir_entry *new_entry) {
    FAT32_Info *cfg = &state->img_config;
    const uint32_t max_entries_per_sector = cfg->bytes_per_sector / sizeof(short_dir_entry);
    uint8_t *buffer = malloc(cfg->bytes_per_sector);
    if (!buffer) return -1;
    uint32_t current_cluster = state->working_dir_start_cluster;
    uint32_t prev_cluster = 0;

    while (current_cluster < END_CLUSTER_MIN && current_cluster != 0) {
        prev_cluster = current_cluster;
        uint32_t start_sector = first_sector_of_cluster(current_cluster, cfg);

        for (uint32_t i = 0; i < cfg->sectors_per_cluster; ++i) {
            uint32_t sector_idx = start_sector + i;
            read_sector(buffer, sector_idx, state);
            short_dir_entry *entries = (short_dir_entry *)buffer;
            for (uint32_t j = 0; j < max_entries_per_sector; ++j) {
                short_dir_entry *entry = &entries[j];
                if (is_final_entry(entry) || is_deleted_entry(entry)) {
                    //Found free slot in this sector/cluster
                    long sector_pos =
                        (long)sector_byte_offset(sector_idx, cfg) +
                        (long)j * (long)sizeof(short_dir_entry);

                    fseek(state->image, sector_pos, SEEK_SET);
                    fwrite(new_entry, sizeof(short_dir_entry), 1, state->image);

                    //If overwrote END_OF_ENTRIES marker, next entry marked as end-of-entries
                    if (is_final_entry(entry) && (j + 1) < max_entries_per_sector) {
                        short_dir_entry end_entry;
                        memset(&end_entry, 0, sizeof(short_dir_entry));
                        end_entry.filename[0] = END_OF_ENTRIES;

                        long next_pos =
                            (long)sector_byte_offset(sector_idx, cfg) +
                            (long)(j + 1) * (long)sizeof(short_dir_entry);
                        fseek(state->image, next_pos, SEEK_SET);
                        fwrite(&end_entry, sizeof(short_dir_entry), 1, state->image);
                    }

                    free(buffer);
                    return 0;
                }
            }
        }
        current_cluster = get_next_cluster(current_cluster, state);
    }

    //No free slot found, extend directory with a new cluster
    uint32_t new_dir_cluster = 0;
    if (allocate_free_cluster(state, &new_dir_cluster) != 0) {
        free(buffer);
        return -1;
    }

    //Link the previous last cluster to the new one
    if (prev_cluster != 0) {
        set_fat_entry(state, prev_cluster, new_dir_cluster);
    }
    clear_cluster(state, new_dir_cluster);

    //Write the new entry into the first slot of the new cluster
    uint32_t first_sector = first_sector_of_cluster(new_dir_cluster, cfg);
    long pos = (long)sector_byte_offset(first_sector, cfg);
    fseek(state->image, pos, SEEK_SET);
    fwrite(new_entry, sizeof(short_dir_entry), 1, state->image);

    //Mark the second entry as END_OF_ENTRIES
    short_dir_entry end_entry;
    memset(&end_entry, 0, sizeof(short_dir_entry));
    end_entry.filename[0] = END_OF_ENTRIES;
    long second_pos = pos + (long)sizeof(short_dir_entry);
    fseek(state->image, second_pos, SEEK_SET);
    fwrite(&end_entry, sizeof(short_dir_entry), 1, state->image);
    free(buffer);
    return 0;
}

//Initialize . and .. entries inside a freshly allocated directory cluster
static void init_dot_entries(fat_state *state, uint32_t new_dir_cluster, uint32_t parent_cluster) {
    FAT32_Info *cfg = &state->img_config;

    short_dir_entry dot;
    short_dir_entry dotdot;

    memset(&dot, 0, sizeof(short_dir_entry));
    memset(&dotdot, 0, sizeof(short_dir_entry));

    // . entry
    memset(dot.filename, ' ', 11);
    dot.filename[0] = '.';
    dot.attributes = ATTR_DIRECTORY;
    dot.first_cluster_hi = (uint16_t)((new_dir_cluster >> 16) & 0xFFFF);
    dot.first_cluter_low = (uint16_t)(new_dir_cluster & 0xFFFF);
    dot.file_size = 0;

    // .. entry
    memset(dotdot.filename, ' ', 11);
    dotdot.filename[0] = '.';
    dotdot.filename[1] = '.';
    dotdot.attributes = ATTR_DIRECTORY;
    dotdot.first_cluster_hi = (uint16_t)((parent_cluster >> 16) & 0xFFFF);
    dotdot.first_cluter_low = (uint16_t)(parent_cluster & 0xFFFF);
    dotdot.file_size = 0;

    uint32_t first_sector = first_sector_of_cluster(new_dir_cluster, cfg);
    long base_pos = (long)sector_byte_offset(first_sector, cfg);

    //Write .
    fseek(state->image, base_pos, SEEK_SET);
    fwrite(&dot, sizeof(short_dir_entry), 1, state->image);

    //Write ..
    long second_pos = base_pos + (long)sizeof(short_dir_entry);
    fseek(state->image, second_pos, SEEK_SET);
    fwrite(&dotdot, sizeof(short_dir_entry), 1, state->image);

    //Mark the next entry as END_OF_ENTRIES
    short_dir_entry end_entry;
    memset(&end_entry, 0, sizeof(short_dir_entry));
    end_entry.filename[0] = END_OF_ENTRIES;

    long third_pos = base_pos + 2 * (long)sizeof(short_dir_entry);
    fseek(state->image, third_pos, SEEK_SET);
    fwrite(&end_entry, sizeof(short_dir_entry), 1, state->image);
}

/* --------- public Part 3 entry points --------- */
void make_dir(fat_state *state, const char *dirname) {
    if (dirname == NULL || dirname[0] == '\0') {
        printf("mkdir: missing operand\n");
        return;
    }

    if (strcmp(dirname, ".") == 0 || strcmp(dirname, "..") == 0) {
        printf("mkdir: cannot create special directory %s\n", dirname);
        return;
    }

    //Check if an entry already exists with this name
    short_dir_entry *existing = find_entry(state, (char *)dirname);
    if (existing != NULL) {
        free(existing);
        printf("mkdir: cannot create '%s': entry already exists\n", dirname);
        return;
    }

    //Allocate a new cluster for the directory itself
    uint32_t new_cluster = 0;
    if (allocate_free_cluster(state, &new_cluster) != 0) {
        printf("mkdir: no free clusters available\n");
        return;
    }

    //Clear the cluster and initialize its . and .. entries
    clear_cluster(state, new_cluster);
    init_dot_entries(state, new_cluster, state->working_dir_start_cluster);

    //Prepare the short directory entry that will live in the parent directory
    short_dir_entry dir_entry;
    memset(&dir_entry, 0, sizeof(short_dir_entry));

    build_short_name(dirname, dir_entry.filename);
    dir_entry.attributes = ATTR_DIRECTORY;
    dir_entry.first_cluster_hi = (uint16_t)((new_cluster >> 16) & 0xFFFF);
    dir_entry.first_cluter_low = (uint16_t)(new_cluster & 0xFFFF);
    dir_entry.file_size = 0;

    if (append_entry_to_cwd(state, &dir_entry) != 0) {
        printf("mkdir: failed to insert directory entry for '%s'\n", dirname);
        return;
    }
}

void create_ef(fat_state *state, const char *filename) {
    if (filename == NULL || filename[0] == '\0') {
        printf("creat: missing operand\n");
        return;
    }

    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
        printf("creat: invalid file name '%s'\n", filename);
        return;
    }

    //Check if an entry already exists with this name
    short_dir_entry *existing = find_entry(state, (char *)filename);
    if (existing != NULL) {
        free(existing);
        printf("creat: cannot create '%s': entry already exists\n", filename);
        return;
    }

    short_dir_entry file_entry;
    memset(&file_entry, 0, sizeof(short_dir_entry));

    build_short_name(filename, file_entry.filename);
    file_entry.attributes = ATTR_ARCHIVE;
    file_entry.first_cluster_hi = 0;
    file_entry.first_cluter_low = 0;
    file_entry.file_size = 0;

    if (append_entry_to_cwd(state, &file_entry) != 0) {
        printf("creat: failed to insert file entry for '%s'\n", filename);
        return;
    }
}
