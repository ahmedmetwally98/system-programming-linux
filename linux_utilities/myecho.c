#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>  // Use standard boolean type


// Error messages
#define ERR_UNKNOWN_OPT         "Error: invalid argument.\n"
#define ERR_MULTI_OPTION_FMT    "Error: -%c option is given multiple times\n"

// Program options
typedef struct {
    bool add_newline;        // Whether to add a newline at the end
    bool interpret_escapes;  // Whether to interpret escape sequences
} Options;

const char escape_chars[] = {'\n', '\t', '\r', '\b', '\v', '\f', '\7', '\0', '\\'};
const char *space_char = " ";

/**
 * Print usage information
 * @param program_name The name of the program
 */
void print_usage(const char *program_name) {
    printf("Usage: %s [options] <message>\n", program_name);
    printf("Options:\n");
    printf("\t-e: Enable interpretation of backslash escapes\n");
    printf("\t-n: Do not print the trailing newline character\n");
    printf("\t-h: Show this help message\n");
}

/**
 * Print an error message to stderr and exit
 * @param message The error message
 */
void print_error_and_exit(const char *message) {
    write(STDERR_FILENO, message, strlen(message));
    exit(EXIT_FAILURE);
}

/**
 * Print a formatted error message for multiple option usage
 * @param option The option character that was used multiple times
 */
void print_multi_option_error_and_exit(char option) {
    char buffer[50];
    snprintf(buffer, sizeof(buffer), ERR_MULTI_OPTION_FMT, option);
    print_error_and_exit(buffer);
}

/**
 * Print a string with escape sequence interpretation
 * @param str The string to print
 */
void print_with_escapes(const char *str) {
    for (; *str; str++) {
        if (*str == '\\' && *(str + 1)) {
            str++;
            switch (*str) {
                case 'n': write(STDOUT_FILENO, escape_chars, 1); break;
                case 't': write(STDOUT_FILENO, escape_chars+1, 1); break;
                case 'r': write(STDOUT_FILENO, escape_chars+2, 1); break;
                case 'b': write(STDOUT_FILENO, escape_chars+3, 1); break;
                case 'v': write(STDOUT_FILENO, escape_chars+4, 1); break;
                case 'f': write(STDOUT_FILENO, escape_chars+5, 1); break;
                case 'a': write(STDOUT_FILENO, escape_chars+6, 1); break;
                case '0': write(STDOUT_FILENO, escape_chars+7, 1); break;
                case '\\': write(STDOUT_FILENO, escape_chars+8, 1); break;
                default : write(STDOUT_FILENO, str, 1); break;
            }
        } else {
            write(STDOUT_FILENO, str, 1);
        }
    }
}

/**
 * Parse command line options
 * @param argc Argument count
 * @param argv Argument vector
 * @param options Pointer to options structure to fill
 * @return Index of the first non-option argument
 */
int parse_options(int argc, char **argv, Options *options) {
    int i;
    
    // Initialize default options
    options->add_newline = true;
    options->interpret_escapes = false;
    
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (arg[0] != '-' || arg[1] == '\0')
        {
            // Not an option or just "-"
            break;
        }

        // Process each character in the option
        for (int j = 1; arg[j]; j++) {
            switch (arg[j]) {
                case 'n':
                    if (!options->add_newline) {
                        print_multi_option_error_and_exit('n');
                    }
                    options->add_newline = false;
                    break;
                    
                case 'e':
                    if (options->interpret_escapes) {
                        print_multi_option_error_and_exit('e');
                    }
                    options->interpret_escapes = true;
                    break;
                    
                case 'h':
                    print_usage(argv[0]);
                    exit(EXIT_SUCCESS);
                    
                default:
                    write(STDOUT_FILENO, arg+i, 1);
                    print_error_and_exit(ERR_UNKNOWN_OPT);
                    break;
            }
        }
    }
    
    return i;  // Return index of first non-option argument
}

/**
 * Print the message according to the specified options
 * @param argv Argument vector
 * @param start_idx Index of the first message argument
 * @param argc Argument count
 * @param options The options structure
 */
void print_message(char **argv, int start_idx, int argc, const Options *options) {
    // If no message arguments, do nothing (except newline if needed)
    if (start_idx >= argc) {
        if (options->add_newline) {
            write(STDOUT_FILENO, escape_chars, 1);
        }
        return;
    }
    
    // Print each argument
    for (int i = start_idx; i < argc; i++) {
        if (options->interpret_escapes) {
            print_with_escapes(argv[i]);
        } else {
            write(STDOUT_FILENO, argv[i], strlen(argv[i]));
        }
        
        // Add space between arguments, but not after the last one
        if (i < argc - 1) {
            write(STDOUT_FILENO, space_char, 1);
        }
    }
    
    // Add newline if needed
    if (options->add_newline) {
        write(STDOUT_FILENO, escape_chars, 1);
    }
}

int main(int argc, char **argv) {
    // Handle special case: no arguments
    if (argc == 1) {
        write(STDOUT_FILENO, escape_chars, 1);
        return EXIT_SUCCESS;
    }
    
    Options options;
    int message_start_idx = parse_options(argc, argv, &options);
    
    print_message(argv, message_start_idx, argc, &options);

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);


    return EXIT_SUCCESS;
}
