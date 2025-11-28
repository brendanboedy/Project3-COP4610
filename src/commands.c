#include <stdio.h>
#include <string.h>
#include "lexer.h"
#include "part1.h"

void handle_command(tokenlist *tokens) {
    if (tokens->size == 0) return;

    if (strcmp(tokens->items[0], "info") == 0) {
        print_info();
    } 
    else if (strcmp(tokens->items[0], "exit") == 0) {
        exit_program();
    } 
    else {
        printf("Unknown command: %s\n", tokens->items[0]);
    }
}
