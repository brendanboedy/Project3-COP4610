#include "update.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "create.h"
#include "fatter.h"
#include "imager.h"

void write_file(char *filename, char *string, file_lst *files, fat_state *state) {
    if (!filename || !string) {
        printf("write: missing operand\n");
        return;
    }

    fat_file *target = get_open_file(filename, files, state);

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
                uint32_t sector_fat =
                    cfg->first_fat_sector + (fat_offset_bytes / cfg->bytes_per_sector);
                uint32_t offset_in_sector = fat_offset_bytes % cfg->bytes_per_sector;
                long pos = (long)sector_fat * cfg->bytes_per_sector + offset_in_sector;
                fseek(state->image, pos, SEEK_SET);
                fwrite(&next_cluster, sizeof(uint32_t), 1, state->image);
            }
            cluster = next_cluster;
        }
    }

    target->offset += bytes_written;
    if (offset > target->entry->file_size) target->entry->file_size = offset;

    // Write updated entry back to disk
    fseek(state->image, target->dir_entry_offset, SEEK_SET);
    fwrite(target->entry, sizeof(short_dir_entry), 1, state->image);
    fflush(state->image);

    free(buffer);
}

void move_entry(fat_state *state, const char *oldname, const char *newname) {
    if (!oldname || !newname) {
        printf("mv: missing operand\n");
        return;
    }

    uint32_t sector, offset;
    short_dir_entry *entry = find_entry(state, (char *)oldname, &sector, &offset);
    if (!entry) {
        printf("mv: cannot stat '%s': No such file or directory\n", oldname);
        return;
    }

    fat_file *openned_file = get_open_file(oldname, state->openned_files, state);
    if (openned_file != NULL) {
        printf("mv: %s is open. You must close a file before moving it.\n", oldname);
        return;
    }

    short_dir_entry *new_entry = find_entry(state, newname, NULL, NULL);

    if (new_entry != NULL && is_dir(new_entry)) {
        move_entry_to_dir(entry, new_entry, state, sector, offset);
        return;
    } else if (new_entry != NULL) {
        // then filename already taken
        printf("mv: target '%s' already exists\n", newname);
        free(entry);
        return;
    }
    // otherwise were just moovign the file

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

void move_entry_to_dir(short_dir_entry *file_entry, short_dir_entry *dir_entry, fat_state *state,
                       uint32_t og_sector, uint32_t og_offset) {
    if (!file_entry || !dir_entry || !state) return;

    FAT32_Info *cfg = &state->img_config;
    uint32_t dir_cluster = first_cluster_of_entry(dir_entry);
    if (dir_cluster == 0) dir_cluster = cfg->root_cluster;

    uint32_t current_cluster = dir_cluster;
    uint32_t prev_cluster = 0;
    long free_slot_pos = -1;

    // Used for when END_OF_ENTRY marker is overwritten with new file entry
    int appending_entry = 0;
    int needs_new_cluster_for_terminator = 0;
    uint32_t terminator_prev_cluster = 0;

    uint8_t *buffer = malloc(cfg->bytes_per_sector);
    if (!buffer) {
        printf("Error: Memory allocation failed\n");
        return;
    }

    int found_end = 0;
    while (current_cluster < END_CLUSTER_MIN && current_cluster != 0 && !found_end) {
        uint32_t start_sector = first_sector_of_cluster(current_cluster, cfg);

        for (uint32_t i = 0; i < cfg->sectors_per_cluster && !found_end; ++i) {
            uint32_t sector = start_sector + i;
            read_sector(buffer, sector, state);

            short_dir_entry *entries = (short_dir_entry *)buffer;
            uint32_t entries_per_sector = cfg->bytes_per_sector / sizeof(short_dir_entry);

            for (uint32_t j = 0; j < entries_per_sector; ++j) {
                short_dir_entry *entry = &entries[j];

                if (is_final_entry(entry)) {
                    if (free_slot_pos == -1) {
                        free_slot_pos =
                            (long)sector_byte_offset(sector, cfg) + (j * sizeof(short_dir_entry));
                        appending_entry = 1;

                        // Check if this is the very last slot in the cluster, if so then EXTRA STEPS!!!!!
                        if (i == cfg->sectors_per_cluster - 1 && j == entries_per_sector - 1) {
                            needs_new_cluster_for_terminator = 1;
                            terminator_prev_cluster = current_cluster;
                        }
                    }
                    found_end = 1;
                    break;
                }

                if (is_deleted_entry(entry)) {
                    if (free_slot_pos == -1) {
                        free_slot_pos =
                            (long)sector_byte_offset(sector, cfg) + (j * sizeof(short_dir_entry));
                    }
                    continue;
                }

                if (memcmp(entry->filename, file_entry->filename, 11) == 0) {
                    printf("Error: File already exists\n");
                    free(buffer);
                    return;
                }
            }
        }

        if (!found_end) {
            prev_cluster = current_cluster;
            current_cluster = get_next_cluster(current_cluster, state);
        }
    }

    free(buffer);

    if (free_slot_pos != -1) {
        fseek(state->image, free_slot_pos, SEEK_SET);
        fwrite(file_entry, sizeof(short_dir_entry), 1, state->image);

        // If we overwrote the final entry, we must ensure a new terminator exists
        if (appending_entry) {
            if (needs_new_cluster_for_terminator) {
                // Yea, you actually may need to allocate A new cluster JUST to have END_OF_ENTRY
                uint32_t new_cluster;
                if (allocate_free_cluster(state, &new_cluster) != 0) {
                    printf("Error: No free space for directory terminator\n");
                    return;
                }
                set_fat_entry(state, terminator_prev_cluster, new_cluster);
                clear_cluster(state, new_cluster);
            } else {
                short_dir_entry terminator;
                memset(&terminator, 0,
                       sizeof(short_dir_entry));  // zeros it out so we have END_OF_ENTRY marker
                fseek(state->image, free_slot_pos + sizeof(short_dir_entry), SEEK_SET);
                fwrite(&terminator, sizeof(short_dir_entry), 1, state->image);
            }
        }
    } else {
        uint32_t new_cluster;
        if (allocate_free_cluster(state, &new_cluster) != 0) {
            printf("Error: No free space\n");
            return;
        }

        set_fat_entry(state, prev_cluster, new_cluster);

        clear_cluster(state, new_cluster);

        uint32_t new_sector = first_sector_of_cluster(new_cluster, cfg);
        long new_pos = (long)sector_byte_offset(new_sector, cfg);
        fseek(state->image, new_pos, SEEK_SET);
        fwrite(file_entry, sizeof(short_dir_entry), 1, state->image);
    }

    // Delete the old entry
    file_entry->filename[0] = DELETED_ENTRY;
    fseek(state->image, og_offset, SEEK_SET);
    fwrite(file_entry, sizeof(short_dir_entry), 1, state->image);
    fflush(state->image);
}