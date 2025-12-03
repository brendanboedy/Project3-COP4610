/*
 * The captain of the fattest fat reader around 
 *      (main entry point)
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "commands.h"
#include "imager.h"
#include "lexer.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <FAT32 image>\n", argv[0]);
        return 1;
    }

    fat_state *state = mount_image(argv[1]);
    if (state == NULL) {
        fprintf(stderr, "Failed to mount image: %s\n", argv[1]);
        return 1;
    }

    printf("Image mounted successfully.\n");

    while (1) {
        printf("\n%s > ", state->working_dir);

        char *input = get_input();

        tokenlist *tokens = get_tokens(input);

        handle_command(state, tokens);

        free(input);
        free_tokens(tokens);
    }

    return 0;
}
