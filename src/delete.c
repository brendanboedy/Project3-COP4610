#include "delete.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "imager.h"
#include "read.h" 

//-------------------- local helpers --------------------
//Write FAT entry for given cluster
static void set_fat_entry(fat_state *state, uint32_t cluster, uint32_t value) {
    FAT32_Info *cfg = &state->img_config;

    uint32_t fat_offset_bytes = cluster * 4;
    uint32_t sector = cfg->first_fat_sector + (fat_offset_bytes / cfg->bytes_per_sector);
    uint32_t offset_in_sector = fat_offset_bytes % cfg->bytes_per_sector;
    long pos = (long)sector * cfg->bytes_per_sector + offset_in_sector;

    fseek(state->image, pos, SEEK_SET);
    fwrite(&value, sizeof(uint32_t), 1, state->image);
    fflush(state->image);
}

//Free entire FAT cluster chain
static void free_cluster_chain(fat_state *state, uint32_t start_cluster) {
    if (start_cluster == 0) {
        // No data allocated
        return;
    }
    uint32_t current = start_cluster;
    while (current >= 2 && current < END_CLUSTER_MIN) {
        uint32_t next = get_next_cluster(current, state);
        //Mark cluster as free
        set_fat_entry(state, current, 0x00000000);
        if (next == 0 || next >= END_CLUSTER_MIN) {
            break;
        }
        current = next;
    }
}

//Mark directory entry in CWD as deleted
static int mark_entry_deleted(fat_state *state, const char *name) {
    FAT32_Info *cfg = &state->img_config;
    uint8_t *buffer = malloc(cfg->bytes_per_sector);
    if (!buffer) return -1;

    const uint32_t max_entries = cfg->bytes_per_sector / sizeof(short_dir_entry);
    uint32_t cluster = state->working_dir_start_cluster;
    while (cluster < END_CLUSTER_MIN && cluster != 0) {
        uint32_t start_sector = first_sector_of_cluster(cluster, cfg);
        for (uint32_t s = 0; s < cfg->sectors_per_cluster; ++s) {
            uint32_t sector_idx = start_sector + s;
            read_sector(buffer, sector_idx, state);

            short_dir_entry *entries = (short_dir_entry *)buffer;
            for (uint32_t i = 0; i < max_entries; ++i) {
                short_dir_entry *entry = &entries[i];

                if (is_final_entry(entry)) {
                    free(buffer);
                    //reached end - not found
                    return -1;
                }
                if (is_deleted_entry(entry) || is_long_filename(entry)) {
                    continue;
                }
                char fname[13];
                translate_filename(entry->filename, fname);
                if (strcasecmp(fname, name) == 0) {
                    //Mark as deleted
                    entry->filename[0] = DELETED_ENTRY;

                    long pos = (long)sector_byte_offset(sector_idx, cfg) +
                        (long)i * (long)sizeof(short_dir_entry);
                    fseek(state->image, pos, SEEK_SET);
                    fwrite(entry, sizeof(short_dir_entry), 1, state->image);
                    fflush(state->image);

                    free(buffer);
                    return 0;
                }
            }
        }
        cluster = get_next_cluster(cluster, state);
    }
    free(buffer);
    return -1;
}

//Check whether a directory is empty
static int is_directory_empty(fat_state *state, uint32_t dir_cluster) {
    FAT32_Info *cfg = &state->img_config;
    uint8_t *buffer = malloc(cfg->bytes_per_sector);
    if (!buffer) return 0;

    const uint32_t max_entries = cfg->bytes_per_sector / sizeof(short_dir_entry);
    uint32_t cluster = dir_cluster;
    while (cluster < END_CLUSTER_MIN && cluster != 0) {
        uint32_t start_sector = first_sector_of_cluster(cluster, cfg);
        for (uint32_t s = 0; s < cfg->sectors_per_cluster; ++s) {
            uint32_t sector_idx = start_sector + s;
            read_sector(buffer, sector_idx, state);

            short_dir_entry *entries = (short_dir_entry *)buffer;
            for (uint32_t i = 0; i < max_entries; ++i) {
                short_dir_entry *entry = &entries[i];
                if (is_final_entry(entry)) {
                    free(buffer);
                    return 1;
                }
                if (is_deleted_entry(entry) || is_long_filename(entry)) {
                    continue;
                }
                char namebuf[13];
                translate_filename(entry->filename, namebuf);
                if (strcmp(namebuf, ".") == 0 || strcmp(namebuf, "..") == 0) {
                    continue;
                }
                //Found a real entry
                free(buffer);
                return 0;
            }
        }
        cluster = get_next_cluster(cluster, state);
    }
    free(buffer);
    return 1;
}

//Check for dirname in CWD
static int has_open_files_in_dir(fat_state *state, const char *dirname) {
    file_lst *open_files = state->openned_files;
    if (!open_files) return 0;
    size_t base_len = strlen(state->working_dir);
    size_t dir_len = strlen(dirname);
    //Build directory path
    size_t buf_len = base_len + dir_len + 2;  // + '/' + '\0'
    char *dir_path = malloc(buf_len);
    if (!dir_path) return 0;
    snprintf(dir_path, buf_len, "%s%s/", state->working_dir, dirname);
    size_t prefix_len = strlen(dir_path);
    for (int i = 0; i < open_files->file_idx; ++i) {
        fat_file *f = &open_files->files[i];
        if (!f->open || !f->full_path) {
            continue;
        }

        if (strncmp(f->full_path, dir_path, prefix_len) == 0) {
            free(dir_path);
            return 1;
        }
    }
    free(dir_path);
    return 0;
}

// -------------------- main methods --------------------
//rm filename
void remove_file(fat_state *state, const char *filename) {
    if (!filename || filename[0] == '\0') {
        printf("rm: missing operand\n");
        return;
    }
    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
        printf("rm: cannot remove special entry '%s'\n", filename);
        return;
    }
    if (is_file_open(state->openned_files, (char *)filename)) {
        printf("rm: cannot remove '%s': file is open\n", filename);
        return;
    }
    short_dir_entry *entry = find_entry(state, (char *)filename);
    if (!entry) {
        printf("rm: cannot remove '%s': No such file\n", filename);
        return;
    }
    if (is_dir(entry)) {
        printf("rm: cannot remove '%s': is a directory\n", filename);
        free(entry);
        return;
    }
    //Reclaim actual file data
    uint32_t first_cluster = first_cluster_of_entry(entry);
    if (first_cluster != 0) {
        free_cluster_chain(state, first_cluster);
    }
    //Remove entry from current directory
    if (mark_entry_deleted(state, filename) != 0) {
        printf("rm: internal error deleting directory entry for '%s'\n", filename);
    }
    free(entry);
}

//rmdir dirname
void remove_dir(fat_state *state, const char *dirname) {
    if (!dirname || dirname[0] == '\0') {
        printf("rmdir: missing operand\n");
        return;
    }
    if (strcmp(dirname, ".") == 0 || strcmp(dirname, "..") == 0) {
        printf("rmdir: cannot remove special directory '%s'\n", dirname);
        return;
    }
    short_dir_entry *entry = find_entry(state, (char *)dirname);
    if (!entry) {
        printf("rmdir: failed to remove '%s': No such file or directory\n", dirname);
        return;
    }
    if (!is_dir(entry)) {
        printf("rmdir: failed to remove '%s': Not a directory\n", dirname);
        free(entry);
        return;
    }
    uint32_t dir_cluster = first_cluster_of_entry(entry);

    //Check emptiness
    if (!is_directory_empty(state, dir_cluster)) {
        printf("rmdir: failed to remove '%s': Directory not empty\n", dirname);
        free(entry);
        return;
    }
    //Check that no file under this directory is open
    if (has_open_files_in_dir(state, dirname)) {
        printf("rmdir: failed to remove '%s': A file is open in that directory\n", dirname);
        free(entry);
        return;
    }
    //Free directory's cluster chain
    if (dir_cluster != 0) {
        free_cluster_chain(state, dir_cluster);
    }
    //Remove directory entry from paren
    if (mark_entry_deleted(state, dirname) != 0) {
        printf("rmdir: internal error deleting directory entry for '%s'\n", dirname);
    }
    free(entry);
}
