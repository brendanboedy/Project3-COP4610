#include "navigate.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands.h"
#include "imager.h"
#include "navigate.h"

void change_dir(fat_state* state, char* dest_dir) {
    if (strcmp(dest_dir, ".") == 0) {
        return;
    }

    short_dir_entry* entry = find_entry(state, dest_dir, NULL, NULL);

    if (entry == NULL) {
        printf("No file or directory %s found", dest_dir);
        return;
    }

    if (!is_dir(entry)) {
        printf("%s is not a directory", dest_dir);
        free(entry);
        return;
    }

    for (int i = 0; i < strlen(dest_dir) - 1 || dest_dir[i] != '\0'; i++) {
        dest_dir[i] = toupper((unsigned char)dest_dir[i]);
    }

    FAT32_Info* config = &state->img_config;
    uint32_t new_working_cluster = first_cluster_of_entry(entry);
    state->working_dir_start_cluster =
        (new_working_cluster == 0) ? config->root_cluster : new_working_cluster;

    if (strcmp(dest_dir, "..") == 0) {
        if (strcmp(state->working_dir, "/") == 0) {
            free(entry);
            return;
        }

        size_t len = strlen(state->working_dir);
        state->working_dir[len - 1] = '\0';

        char* last_slash = strrchr(state->working_dir, '/');

        if (last_slash != NULL) {
            *(last_slash + 1) = '\0';
        } else {
            printf("BUG BRO! last_slash null somehow. FIX IT YOU IMBECILE.");
        }
    } else {
        // concatenates /home/ with documents and thus /home/documents/
        strcat(state->working_dir, dest_dir);
        strcat(state->working_dir, "/");
    }

    free(entry);
}

void list_entries_in_dir(fat_state* state) {
    FAT32_Info* config = &state->img_config;
    const uint32_t MAX_ENTRIES = config->bytes_per_sector / sizeof(short_dir_entry);

    uint32_t current_cluster = state->working_dir_start_cluster;
    uint8_t* buffer = malloc(sizeof(uint8_t) * config->bytes_per_sector);

    int finished_traversal = 0;

    while (current_cluster < END_CLUSTER_MIN && current_cluster != 0) {
        uint32_t start_sector_idx = first_sector_of_cluster(current_cluster, config);
        for (uint32_t i = 0; i < config->sectors_per_cluster && !finished_traversal; ++i) {
            uint32_t sector_idx = start_sector_idx + i;
            read_sector(buffer, sector_idx, state);

            short_dir_entry* entries = (short_dir_entry*)buffer;
            for (uint32_t j = 0; j < MAX_ENTRIES; ++j) {
                short_dir_entry* entry = &entries[j];

                if (is_final_entry(entry)) {
                    // Basically the end of directory
                    finished_traversal = 1;
                    break;
                } else if (is_deleted_entry(entry)) {
                    continue;
                } else if (is_long_filename(entry)) {
                    // long filenames are spread out through many entries, with each one containing the long
                    // entry flag. The final entry is a short entry with the actual data. So we just ignore
                    // it until the problem goes away
                    continue;
                }

                char filename[14];
                translate_filename(entry->filename, filename);
                if (is_dir(entry)) {
                    strcat(filename, "/");
                }
                printf("%s\n", filename);
            }
        }
        current_cluster = get_next_cluster(current_cluster, state);
    }
    // finished_traversal only exists cause of this free(buffer) command. maybe overkill idk
    free(buffer);
}