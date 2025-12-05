#include "update.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include "fatter.h"




/* ============================ WRITE ============================ */

void write_file(char *filename, char *string, file_lst *files, fat_state *state) {
    if (!filename || !string) {
        printf("write: missing operand\n");
        return;
    }

    fat_file *target = NULL;
    for (int i = 0; i < files->file_idx; ++i) {
        if (!files->files[i].open) continue;
        if (strcmp(files->files[i].filename, filename) == 0) {
            target = &files->files[i];
            break;
        }
    }

    if (!target) {
        printf("write: file '%s' is not open\n", filename);
        return;
    }
    if (!target->write) {
        printf("write: file '%s' not opened for writing\n", filename);
        return;
    }

    uint32_t len = strlen(string);
    if (len == 0) return;

    FAT32_Info *cfg = &state->img_config;
    uint32_t bytes_written = 0;
    uint32_t offset = target->offset;

    uint32_t cluster = cluster_from_entry_offset(target->entry, offset, state);
    if (cluster >= END_CLUSTER_MIN || cluster == 0) {
        if (allocate_free_cluster(state, &cluster) != 0) {
            printf("write: no free cluster available\n");
            return;
        }
        target->entry->first_cluster_hi = (uint16_t)((cluster >> 16) & 0xFFFF);
        target->entry->first_cluter_low = (uint16_t)(cluster & 0xFFFF);
    }

    uint8_t *buffer = malloc(cfg->bytes_per_sector);
    if (!buffer) {
        printf("write: memory allocation failed\n");
        return;
    }

    while (bytes_written < len) {
        uint32_t sector = sector_from_entry_offset(cluster, offset, state);
        long long sector_pos = sector_byte_offset(sector, cfg);

        fseek(state->image, sector_pos, SEEK_SET);
        fread(buffer, 1, cfg->bytes_per_sector, state->image);

        uint32_t within_sector = offset % cfg->bytes_per_sector;
        uint32_t space = cfg->bytes_per_sector - within_sector;
        uint32_t to_write = (len - bytes_written < space) ? (len - bytes_written) : space;

        memcpy(buffer + within_sector, string + bytes_written, to_write);

        fseek(state->image, sector_pos, SEEK_SET);
        fwrite(buffer, 1, cfg->bytes_per_sector, state->image);
        fflush(state->image);

        offset += to_write;
        bytes_written += to_write;

        // move to next cluster if needed
        if (offset % (cfg->bytes_per_sector * cfg->sectors_per_cluster) == 0) {
            uint32_t next_cluster = get_next_cluster(cluster, state);
            if (next_cluster >= END_CLUSTER_MIN || next_cluster == 0) {
                if (allocate_free_cluster(state, &next_cluster) != 0) {
                    printf("write: out of space in FAT\n");
                    break;
                }
                // link clusters
                uint32_t fat_offset_bytes = cluster * 4;
                uint32_t sector_fat = cfg->first_fat_sector + (fat_offset_bytes / cfg->bytes_per_sector);
                uint32_t offset_in_sector = fat_offset_bytes % cfg->bytes_per_sector;
                long pos = (long)sector_fat * cfg->bytes_per_sector + offset_in_sector;
                fseek(state->image, pos, SEEK_SET);
                fwrite(&next_cluster, sizeof(uint32_t), 1, state->image);
            }
            cluster = next_cluster;
        }
    }

    target->offset += bytes_written;
    if (offset > target->entry->file_size)
        target->entry->file_size = offset;

    free(buffer);
}

/* ============================ MV ============================ */

void move_entry(fat_state *state, const char *oldname, const char *newname) {
    if (!oldname || !newname) {
        printf("mv: missing operand\n");
        return;
    }

    short_dir_entry *entry = find_entry(state, (char *)oldname);
    if (!entry) {
        printf("mv: cannot stat '%s': No such file or directory\n", oldname);
        return;
    }

    if (find_entry(state, (char *)newname)) {
        printf("mv: target '%s' already exists\n", newname);
        free(entry);
        return;
    }

    // Update filename (FAT short name format)
    memset(entry->filename, ' ', 11);
    size_t len = strlen(newname);
    for (size_t i = 0; i < len && i < 11; ++i)
        entry->filename[i] = (uint8_t)toupper((unsigned char)newname[i]);

    FAT32_Info *cfg = &state->img_config;
    uint32_t cluster = state->working_dir_start_cluster;
    uint8_t *buffer = malloc(cfg->bytes_per_sector);
    if (!buffer) {
        free(entry);
        return;
    }

    const uint32_t max_entries = cfg->bytes_per_sector / sizeof(short_dir_entry);
    int done = 0;

    while (cluster < END_CLUSTER_MIN && cluster != 0 && !done) {
        uint32_t start_sector = first_sector_of_cluster(cluster, cfg);
        for (uint32_t i = 0; i < cfg->sectors_per_cluster && !done; ++i) {
            uint32_t sector_idx = start_sector + i;
            read_sector(buffer, sector_idx, state);
            short_dir_entry *entries = (short_dir_entry *)buffer;

            for (uint32_t j = 0; j < max_entries; ++j) {
                short_dir_entry *slot = &entries[j];
                if (is_final_entry(slot)) break;
                char namebuf[13];
                translate_filename(slot->filename, namebuf);
                if (strcasecmp(namebuf, oldname) == 0) {
                    long sector_pos = (long)sector_byte_offset(sector_idx, cfg) +
                                      (long)j * sizeof(short_dir_entry);
                    fseek(state->image, sector_pos, SEEK_SET);
                    fwrite(entry, sizeof(short_dir_entry), 1, state->image);
                    fflush(state->image);
                    done = 1;
                    break;
                }
            }
        }
        cluster = get_next_cluster(cluster, state);
    }

    free(buffer);
    free(entry);

    if (!done) printf("mv: failed to rename '%s'\n", oldname);
}
