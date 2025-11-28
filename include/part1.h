#pragma once
#include <stdio.h>
#include <stdint.h>

// -----------------------------------------------------------
// FAT32 Boot Sector Structure
// -----------------------------------------------------------
typedef struct {
    uint16_t bytes_per_sector;        // offset 11–12
    uint8_t  sectors_per_cluster;     // offset 13
    uint16_t reserved_sector_count;   // offset 14–15
    uint8_t  num_fats;                // offset 16
    uint32_t total_sectors;           // offset 32–35
    uint32_t fat_size_32;             // offset 36–39
    uint32_t root_cluster;            // offset 44–47
    uint32_t image_size;              // computed via ftell()
} FAT32_BootSector;

// -----------------------------------------------------------
// Global variables (shared across modules)
// -----------------------------------------------------------
extern FAT32_BootSector bpb;
extern FILE *image;

// -----------------------------------------------------------
// Function prototypes
// -----------------------------------------------------------

// Opens the FAT32 image and reads its boot sector fields
int mount_image(const char *filename);

// Prints FAT32 boot sector info (for "info" command)
void print_info(void);

// Safely closes the image and exits the program (for "exit")
void exit_program(void);
