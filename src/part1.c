#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "lexer.h"
#include "part1.h"

FAT32_BootSector bpb;
FILE *image = NULL;

int mount_image(const char *filename) {
    image = fopen(filename, "rb+");
    if (!image) {
        perror("Error opening image file");
        return -1;
    }

    fseek(image, 11, SEEK_SET);
    fread(&bpb.bytes_per_sector, 2, 1, image);

    fseek(image, 13, SEEK_SET);
    fread(&bpb.sectors_per_cluster, 1, 1, image);

    fseek(image, 14, SEEK_SET);
    fread(&bpb.reserved_sector_count, 2, 1, image);

    fseek(image, 16, SEEK_SET);
    fread(&bpb.num_fats, 1, 1, image);

    fseek(image, 32, SEEK_SET);
    fread(&bpb.total_sectors, 4, 1, image);

    fseek(image, 36, SEEK_SET);
    fread(&bpb.fat_size_32, 4, 1, image);

    fseek(image, 44, SEEK_SET);
    fread(&bpb.root_cluster, 4, 1, image);

    fseek(image, 0, SEEK_END);
    bpb.image_size = ftell(image);
    rewind(image);

    return 0;
}

void print_info(void) {
    uint32_t total_clusters =
        (bpb.total_sectors -
        (bpb.reserved_sector_count + (bpb.num_fats * bpb.fat_size_32)))
        / bpb.sectors_per_cluster;

    printf("Root cluster (in cluster #): %u\n", bpb.root_cluster);
    printf("Bytes per sector: %u\n", bpb.bytes_per_sector);
    printf("Sectors per cluster: %u\n", bpb.sectors_per_cluster);
    printf("Total clusters in data region: %u\n", total_clusters);
    printf("Entries in one FAT: %u\n", total_clusters);
    printf("Size of image (in bytes): %u\n", bpb.image_size);
}

void exit_program(void) {
    if (image) fclose(image);
    printf("Image closed. Exiting.\n");
    exit(0);
}
