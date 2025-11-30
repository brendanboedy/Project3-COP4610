#include "commands.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "imager.h"
#include "lexer.h"
#include "navigate.h"
#include "create.h"

void handle_command(fat_state* state, tokenlist* tokens) {
    if (tokens->size == 0) return;

    const char* cmd = tokens->items[0];
    if (strcmp(cmd, "info") == 0) {
        print_info(&state->img_config);
    } else if (strcmp(cmd, "exit") == 0) {
        exit_program(state->image);
    } else if (strcmp(cmd, "cd") == 0) {
        if (tokens->size != 2){
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
            make_directory(state, tokens->items[1]);
        }
    } else if (strcmp(cmd, "creat") == 0) {
        if (tokens->size != 2) {
            printf("Usage: creat [FILENAME]\n");
        } else {
            create_empty_file(state, tokens->items[1]);
        }
    } else {
        printf("Unknown command: %s\n", tokens->items[0]);
    }
}

void print_info(const FAT32_Info* img_config) {
    uint32_t total_clusters =
        (img_config->total_sectors - (img_config->reserved_sector_count +
                                      (img_config->num_fats * img_config->fat_size_32))) /
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
