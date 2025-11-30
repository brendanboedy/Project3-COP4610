#include "commands.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "imager.h"
#include "lexer.h"
#include "navigate.h"

void handle_command(fat_state* state, tokenlist* tokens) {
    if (tokens->size == 0) return;

    if (strcmp(tokens->items[0], "info") == 0) {
        print_info(&state->img_config);
    } else if (strcmp(tokens->items[0], "exit") == 0) {
        exit_program(state->image);
    } else if (strcmp(tokens->items[0], "cd") == 0) {
        change_dir(state, tokens->items[1]);
    } else if (strcmp(tokens->items[0], "ls") == 0) {
        list_entries_in_dir(state);
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
