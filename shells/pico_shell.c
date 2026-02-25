#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define PROMPT "PS> "
#define INITIAL_TOK_CAP 8
#define END_MSG "Good Bye!\n"

static void setup_signals(void);
static char** tokenize_input(char* input, size_t* argc_out);
static void free_tokens(char** tokens);

static bool run_builtin(char** argv, size_t argc, int* should_exit, int* exit_code);
static int builtin_echo(char** argv);
static int builtin_pwd(void);
static int builtin_cd(char** argv, size_t argc);
static int builtin_exit(char** argv, size_t argc, int last_status);

int main(void)
{
    char* line = NULL;
    size_t line_cap = 0;
    int shell_exit_code = EXIT_SUCCESS;
    int should_exit = 0;

    setup_signals();

    while (!should_exit)
    {
        ssize_t nread;
        char** argv = NULL;
        size_t argc = 0;

        if (write(STDOUT_FILENO, PROMPT, strlen(PROMPT)) < 0)
        {
            perror("write");
            shell_exit_code = errno;
            break;
        }

        nread = getline(&line, &line_cap, stdin);
        if (nread == -1)
        {
            if (feof(stdin))
            {
                break;
            }
            perror("getline");
            shell_exit_code = errno;
            continue;
        }

        argv = tokenize_input(line, &argc);
        if (argv == NULL)
        {
            shell_exit_code = ENOMEM;
            break;
        }

        if (argc == 0)
        {
            free_tokens(argv);
            continue;
        }

        if (run_builtin(argv, argc, &should_exit, &shell_exit_code))
        {
            free_tokens(argv);
            continue;
        }

        pid_t pid = fork();
        if (pid == -1)
        {
            perror("fork");
            shell_exit_code = errno;
            free_tokens(argv);
            continue;
        }

        if (pid == 0)
        {
            execvp(argv[0], argv);
            if (errno == ENOENT)
            {
                dprintf(STDERR_FILENO, "%s: command not found\n", argv[0]);
            }
            else
            {
                perror(argv[0]);
            }
            _exit(127);
        }

        int status = 0;
        if (waitpid(pid, &status, 0) == -1)
        {
            perror("waitpid");
            shell_exit_code = errno;
        }
        else if (WIFEXITED(status))
        {
            shell_exit_code = WEXITSTATUS(status);
        }
        else if (WIFSIGNALED(status))
        {
            shell_exit_code = 128 + WTERMSIG(status);
        }

        free_tokens(argv);
    }

    free(line);
    return shell_exit_code;
}

static bool run_builtin(char** argv, size_t argc, int* should_exit, int* exit_code)
{
    if (strcmp(argv[0], "echo") == 0)
    {
        *exit_code = builtin_echo(argv);
        return true;
    }

    if (strcmp(argv[0], "pwd") == 0)
    {
        *exit_code = builtin_pwd();
        return true;
    }

    if (strcmp(argv[0], "cd") == 0)
    {
        *exit_code = builtin_cd(argv, argc);
        return true;
    }

    if (strcmp(argv[0], "exit") == 0)
    {
        *exit_code = builtin_exit(argv, argc, *exit_code);
        *should_exit = 1;
        return true;
    }

    return false;
}

static int builtin_echo(char** argv)
{
    for (size_t i = 1; argv[i] != NULL; ++i)
    {
        if (i > 1)
        {
            if (write(STDOUT_FILENO, " ", 1) < 0)
            {
                perror("write");
                return 1;
            }
        }
        if (write(STDOUT_FILENO, argv[i], strlen(argv[i])) < 0)
        {
            perror("write");
            return 1;
        }
    }

    if (write(STDOUT_FILENO, "\n", 1) < 0)
    {
        perror("write");
        return 1;
    }

    return 0;
}

static int builtin_pwd(void)
{
    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        perror("getcwd");
        return 1;
    }

    if (write(STDOUT_FILENO, cwd, strlen(cwd)) < 0)
    {
        perror("write");
        return 1;
    }
    if (write(STDOUT_FILENO, "\n", 1) < 0)
    {
        perror("write");
        return 1;
    }

    return 0;
}

static int builtin_cd(char** argv, size_t argc)
{
    const char* target = NULL;
    if (argc < 2)
    {
        target = getenv("HOME");
        if (target == NULL)
        {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
    }
    else
    {
        target = argv[1];
    }

    if (chdir(target) != 0)
    {
        fprintf(stderr, "cd: %s: %s\n", target, strerror(errno));
        return 1;
    }

    return 0;
}

static int builtin_exit(char** argv, size_t argc, int last_status)
{
    if (write(STDOUT_FILENO, END_MSG, strlen(END_MSG)) < 0)
    {
        perror("write");
    }

    if (argc < 2)
    {
        return last_status;
    }

    char* endptr = NULL;
    long code = strtol(argv[1], &endptr, 10);
    if (endptr == argv[1] || *endptr != '\0')
    {
        return EXIT_FAILURE;
    }

    return (int)(code & 0xFF);
}

static char** tokenize_input(char* input, size_t* argc_out)
{
    size_t cap = INITIAL_TOK_CAP;
    size_t argc = 0;
    char** tokens = malloc(cap * sizeof(*tokens));
    if (tokens == NULL)
    {
        perror("malloc");
        return NULL;
    }

    char* token = strtok(input, " \n");
    while (token != NULL)
    {
        if (argc + 1 >= cap)
        {
            cap *= 2;
            char** tmp = realloc(tokens, cap * sizeof(*tokens));
            if (tmp == NULL)
            {
                free(tokens);
                perror("realloc");
                return NULL;
            }
            tokens = tmp;
        }

        tokens[argc++] = token;
        token = strtok(NULL, " \n");
    }

    tokens[argc] = NULL;
    *argc_out = argc;
    return tokens;
}

static void free_tokens(char** tokens)
{
    free(tokens);
}

static void setup_signals(void)
{
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sa.sa_handler = SIG_IGN;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = SIG_DFL;
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);
}
