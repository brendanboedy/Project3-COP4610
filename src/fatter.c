/*
 * The captain of the fattest fat reader around 
 *      (main entry point)
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "commands.h"
#include "lexer.h"
#include "imager.h"


int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <FAT32 image>\n", argv[0]);
        return 1;
    }
    
    fat_state* state = mount_image(argv[1]);
    if (state == NULL) {
        fprintf(stderr, "Failed to mount image: %s\n", argv[1]);
        return 1;
    }

    printf("Image mounted successfully.\n");

    while (1) {
        printf("> ");

        char *input = get_input();
        printf("whole input: %s\n", input);

        tokenlist *tokens = get_tokens(input);
        for (int i = 0; i < tokens->size; i++) {
            printf("token %d: (%s)\n", i, tokens->items[i]);
        }

        handle_command(state, tokens);

        free(input);
        free_tokens(tokens);
    }

    return 0;
}

