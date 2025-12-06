#include "commands.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "create.h"
#include "delete.h"
#include "imager.h"
#include "lexer.h"
#include "navigate.h"
#include "read.h"
#include "update.h"

void handle_command(fat_state* state, tokenlist* tokens) {
    if (tokens->size == 0) return;

    const char* cmd = tokens->items[0];
    if (strcmp(cmd, "info") == 0) {
        print_info(&state->img_config);
    } else if (strcmp(cmd, "exit") == 0) {
        exit_program(state->image);
    } else if (strcmp(cmd, "cd") == 0) {
        if (tokens->size != 2) {
            printf("cd: expected one argument\n");
        } else {
            change_dir(state, tokens->items[1]);
        }
    } else if (strcmp(cmd, "ls") == 0) {
        list_entries_in_dir(state);
    } else if (strcmp(cmd, "mkdir") == 0) {
        if (tokens->size != 2) {
            printf("Usage: mkdir [DIRNAME]\n");
        } else {
            make_dir(state, tokens->items[1]);
        }
    } else if (strcmp(cmd, "creat") == 0) {
        if (tokens->size != 2) {
            printf("Usage: creat [FILENAME]\n");
        } else {
            create_ef(state, tokens->items[1]);
        }
    } else if (strcmp(cmd, "close") == 0) {
        if (tokens->size != 2) {
            printf("Usage: close [FILENAME]\n");
        } else {
            close_file(tokens->items[1], state->openned_files, state);
        }
    } else if (strcmp(cmd, "open") == 0) {
        if (tokens->size != 3) {
            printf("Usage: open [FILENAME] [FLAGS]\n");
            return;
        }
        char* fname = tokens->items[1];
        char* flags = tokens->items[2];

        fat_file* f = open_file(fname, flags, state);
        if (f == NULL) {
            return;
        }

        add_file_to_lst(f, state->openned_files);
    } else if (strcmp(cmd, "lsof") == 0) {
        list_open_files(state->openned_files);
    } else if (strcmp(cmd, "lseek") == 0) {
        if (tokens->size != 3) {
            printf("Usage: lseek [FILENAME] [OFFSET]\n");
        } else {
            char* fname = tokens->items[1];
            int offset = atoi(tokens->items[2]);  // Convert string to int
            lseek(fname, offset, state->openned_files, state);
        }
    } else if (strcmp(cmd, "read") == 0) {
        if (tokens->size != 3) {
            printf("Usage: read [FILENAME] [SIZE]\n");
        } else {
            char* fname = tokens->items[1];
            int size = atoi(tokens->items[2]);  // Convert string to int
            read_n_bytes(fname, size, state->openned_files, state);
        }
    } else if (strcmp(cmd, "write") == 0) {
        if (tokens->size < 3) {
            printf("Usage: write [FILENAME] \"STRING\"\n");
        } else {
            char* fname = tokens->items[1];
            char* start = strchr(tokens->items[2], '\"');
            char* end = strrchr(tokens->items[tokens->size - 1], '\"');
            if (start == NULL || end == NULL || end == start) {
                printf("write: string must be in quotes\n");
            } else {
                char* content = tokens_to_str(tokens, 2);
                write_file(fname, content, state->openned_files, state);

                free(content);
            }
        }
    } else if (strcmp(cmd, "mv") == 0) {
        if (tokens->size != 3) {
            printf("Usage: mv [OLDNAME] [NEWNAME]\n");
        } else {
            move_entry(state, tokens->items[1], tokens->items[2]);
        }
    } else if (strcmp(cmd, "rm") == 0) {
        if (tokens->size != 2) {
            printf("Usage: rm [FILENAME]\n");
        } else {
            remove_file(state, tokens->items[1]);
        }
    } else if (strcmp(cmd, "rmdir") == 0) {
        if (tokens->size != 2) {
            printf("Usage: rmdir [DIRNAME]\n");
        } else {
            remove_dir(state, tokens->items[1]);
        }
    } else {
        printf("Unknown command: %s\n", tokens->items[0]);
    }
}

/**
 * Assumes wrapped in quotation marks and skips them
*/
char* tokens_to_str(tokenlist* tokens, int start_idx) {
    size_t len = 0;
    for (int i = start_idx; i < tokens->size; ++i) {
        len += strlen(tokens->items[i]);
    }
    len += tokens->size + 1;  // adds space for spaces, '\0', and an extra one for peace of mind

    char* result = malloc(sizeof(char) * len);
    size_t result_idx = 0;

    for (int i = start_idx; i < tokens->size; ++i) {
        char* word = tokens->items[i];
        if (i == start_idx) {
            word += 1;
        }

        strcpy(result + result_idx, word);

        result_idx += strlen(tokens->items[i]) - (i == start_idx ? 1 : 0);

        if (i < tokens->size - 1) {
            strcpy(result + result_idx, " ");
            result_idx += 1;
        }
    }

    result[result_idx - 1] = '\0';  // minus one cause quotation marks

    return result;
}

void print_info(const FAT32_Info* img_config) {
    uint32_t total_clusters =
        (img_config->total_sectors -
         (img_config->reserved_sector_count + (img_config->num_fats * img_config->fat_size_32))) /
        img_config->sectors_per_cluster;

    printf("Root cluster (in cluster #): %u\n", img_config->root_cluster);
    printf("Bytes per sector: %u\n", img_config->bytes_per_sector);
    printf("Sectors per cluster: %u\n", img_config->sectors_per_cluster);
    printf("Total clusters in data region: %u\n", total_clusters);
    printf("Entries in one FAT: %u\n", total_clusters);
    printf("Size of image (in bytes): %u\n", img_config->image_size);
}

void exit_program(FILE* image) {
    if (image) fclose(image);
    printf("Image closed. Exiting.\n");
    exit(0);
}
