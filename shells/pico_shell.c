#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

/****** Declarations/Macros ******/
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define PROMPT_MAX_SZ    (HOST_NAME_MAX + 9)
/* define the shell built-in command */
#define CMD_IDX_00       0
#define CMD_IDX_01       1
/* define command to be executed in child proc */
#define CMD_IDX_10       2
#define CMD_IDX_11       3
#define CMD_UNDEFINED    (-1)
#define MAX_TOKENS_COUNT 1024
#define TOTAL_SUP_CMD    4
#define ERR_INVL_CMD     "ERROR: Invalid command\n"
#define END_MSG          "Good Bye!\n"
#define ERR_MEM_ALLOC    "ERROR: Memory allocation failed\n"

typedef struct
{
    char  *sup_cmds;
    size_t cmd_id;
} cmdConfig_t;

/****** global varialbes ******/
const char *sup_commands[] = {"exit", "cd", "echo", "pwd"};
static int  exit_code      = 0;
static int  exit_flag      = 0;

/****** function prototypes ******/
// command functions
int  PS_get_cmd_idx(char *cmdName);
void PS_cmd_echo(char *argv[]);
void PS_cmd_pwd(char *argv[]);
int  PS_cmd_exit(char *argv[]);
int  PS_cmd_cd(char *path);

// utility functions
void   setup_signals(void);
char **tokenize_input(char *input, size_t *token_count);
void   PS_init_cmdDB(cmdConfig_t *totalcmds, int cmdIdx);
char  *create_prompt(void);
void   free_tokens(char **tokens);
bool   isBuiltin(int cmdIdx);

/****** main function ******/
int main(void)
{
    // Setup signal handlers
    setup_signals();

    /** define local variables */
    cmdConfig_t sup_cmds[TOTAL_SUP_CMD];
    char       *buf       = NULL;
    char      **tokensArr = NULL;
    char       *prompt    = NULL;
    size_t      bufcount  = 0;
    size_t      tokcount  = 0;
    ssize_t     nread     = 0;

    // Create shell prompt
    prompt = create_prompt();
    if (!prompt)
    {
        return EXIT_FAILURE;
    }
    // fill the command database
    for (int i = TOTAL_SUP_CMD - 1; i >= 0; --i)
    {
        PS_init_cmdDB(&(sup_cmds[i]), i);
    }

    while (1)
    {
        if (write(STDOUT_FILENO, prompt, PROMPT_MAX_SZ) < 0)
        {
            perror("write");
            exit(1);
        }
        fflush(stdout);

        // Read input from stdin
        nread = getline(&buf, &bufcount, stdin);

        if (nread == -1)
        {
            if (feof(stdin))
            {
                // End of file reached only when ctrl+D is pressed. exit the
                // program.
                break;
            }
            else
            {
                perror("getline");
                continue;
            }
            // "EINVAL" error number shouldn't happen as there is no valid
            // reason to occur.
        }

        // skip empty lines
        if (nread <= 1)
        {
            continue;
        }

        tokensArr = tokenize_input(buf, &tokcount);

        /* if the command is not cd nor exit, we can execute it in the
         * child process, otherwise execute it in the parent process.*/
        // check if the command is a built-in command
        int cmdIdx = PS_get_cmd_idx(tokensArr[0]);
        if (cmdIdx == CMD_UNDEFINED)
        {
            write(STDERR_FILENO, ERR_INVL_CMD, strlen(ERR_INVL_CMD));
            free_tokens(tokensArr);
            tokensArr = NULL;
            continue;
        }
        // check if the command is a built-in command
        if (isBuiltin(cmdIdx))
        {
            // Built-in command, execute in the parent process
            switch (cmdIdx)
            {
            case CMD_IDX_00:
                exit_code = PS_cmd_exit(&tokensArr[1]);
                exit_flag = 1;
                break;
            case CMD_IDX_01:
                PS_cmd_cd(tokensArr[1]);
                break;
            default:
                break;
            }
            // Free tokens array and continue to the next iteration
            free_tokens(tokensArr);
            tokensArr = NULL;

            if (exit_flag)
            {
                break;
            }
            else
            {
                continue;
            }
        }
        else
        {
            // External command, execute in the child process
            pid_t PID = fork();

            if (PID == -1) // failed to create child proc
            {
                perror("fork");
                free_tokens(tokensArr);
                tokensArr = NULL;
                exit_code = errno;
                break;
            }
            else if (PID == 0) // child process
            {
                switch (PS_get_cmd_idx(tokensArr[0]))
                {
                case CMD_IDX_10:
                    PS_cmd_echo(&tokensArr[0]);
                    break;
                case CMD_IDX_11:
                    PS_cmd_pwd(&tokensArr[0]);
                    break;
                default:
                    write(STDERR_FILENO, ERR_INVL_CMD, strlen(ERR_INVL_CMD));
                    break;
                }
                // terminate the child proc
                free_tokens(tokensArr);
                tokensArr = NULL;
                break;
            }
            else // parent proc
            {
                int status;
                if (waitpid(PID, &status, 0) == -1) // failure indication
                {
                    perror("Child process failed");
                    exit(errno);
                }

                // Free tokens array regardless of exit condition
                free_tokens(tokensArr);
                tokensArr = NULL;
            }
        }
    }

    if (buf)
    {
        free(buf);
        buf = NULL;
    }
    if (prompt)
    {
        free(prompt);
        prompt = NULL;
    }

    return exit_code;
}
// function prototypes
inline int PS_get_cmd_idx(char *cmdName)
{
    for (int i = 0; i < TOTAL_SUP_CMD; ++i)
    {
        if (strcmp(cmdName, sup_commands[i]) == 0)
        {
            return i;
        }
    }

    // command not found
    return CMD_UNDEFINED;
}

void PS_cmd_echo(char *argv[])
{
    if (execve("/usr/bin/echo", argv, NULL) == -1)
    {
        perror("failed with execve");
        exit(errno);
    }
}

int PS_cmd_exit(char *argv[])
{
    // exit with 0 if no arguments or error code if any
    write(STDOUT_FILENO, END_MSG, strlen(END_MSG));

    if (argv[0] != NULL)
    {
        return atoi(argv[0]);
    }

    return EXIT_SUCCESS;
}

void PS_cmd_pwd(char *argv[])
{
    if (execve("/usr/bin/pwd", argv, NULL) == -1)
    {
        perror("failed with execve");
        exit(errno);
    }
}

int PS_cmd_cd(char *path)
{
    char  cwd[PATH_MAX];
    char *oldpwd = getenv("OLDPWD");

    // Handle '~' (home directory)
    if (path == NULL || strcmp(path, "~") == 0)
    {
        path = getenv("HOME");
    }
    // Handle '-' (previous directory)
    else if (strcmp(path, "-") == 0)
    {
        if (oldpwd == NULL)
        {
            fprintf(stderr, "cd: OLDPWD not set\n");
            return -1;
        }
        path = oldpwd;
    }

    // Save current directory before changing
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        perror("getcwd");
        return -1;
    }

    // Attempt to change directory
    if (chdir(path) != 0)
    {
        perror("cd");
        return -1;
    }

    // Update OLDPWD and PWD environment variables
    setenv("OLDPWD", cwd, 1);
    // Get the new current directory
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        perror("getcwd");
        return -1;
    }

    setenv("PWD", cwd, 1);

    // Print the new current directory
    if (write(STDOUT_FILENO, cwd, strlen(cwd)) < 0)
    {
        perror("write");
        return -1;
    }
    if (write(STDOUT_FILENO, "\n", 1) < 0)
    {
        perror("write");
        return -1;
    }

    return 0;
}

void PS_init_cmdDB(cmdConfig_t *totalcmds, int cmdIdx)
{
    if (totalcmds != NULL)
    {
        totalcmds->sup_cmds = (char *)sup_commands[cmdIdx];
        totalcmds->cmd_id   = cmdIdx;
    }
    else
    {
        if (write(STDERR_FILENO, "Invlaid pointer\n",
                  strlen("Invlaid pointer\n")) < 0)
        {
            exit(-1);
        }
    }
}

char *create_prompt(void)
{
    char hostname[HOST_NAME_MAX + 1];

    if (gethostname(hostname, sizeof(hostname)) == -1)
    {
        perror("gethostname");
        return NULL;
    }

    char *prompt = malloc(PROMPT_MAX_SZ);
    if (!prompt)
    {
        perror(ERR_MEM_ALLOC);
        return NULL;
    }

    snprintf(prompt, PROMPT_MAX_SZ, "[%s@spl]$ ", hostname);

    return prompt;
}

char **tokenize_input(char *input, size_t *token_count)
{
    char **tokens = malloc(sizeof(char *) * MAX_TOKENS_COUNT);
    if (!tokens)
    {
        perror(ERR_MEM_ALLOC);
        return NULL;
    }

    char  *token = strtok(input, " \n");
    size_t i     = 0;

    while (token && i < MAX_TOKENS_COUNT - 1)
    {
        tokens[i++] = token;
        token       = strtok(NULL, " \n");
    }

    tokens[i]    = NULL;
    *token_count = i;

    return tokens;
}

void free_tokens(char **tokens)
{
    if (tokens)
    {
        free(tokens);
    }
}

bool isBuiltin(int cmdIdx)
{
    if (cmdIdx == CMD_IDX_00 || cmdIdx == CMD_IDX_01)
    {
        return true;
    }
    return false;
}

void setup_signals(void)
{
    /*! add signal handling to our shell so that it can be responsive to
         1. ctrl+C
         2. ctrl+\
         3. ctrl+z
    */
    struct sigaction sa;

    // Set up common signal attributes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // Ignore SIGINT (Ctrl+C)
    sa.sa_handler = SIG_IGN;
    sigaction(SIGINT, &sa, NULL);

    // Default action for SIGQUIT (Ctrl+\)
    sa.sa_handler = SIG_DFL;
    sigaction(SIGQUIT, &sa, NULL);

    // Default action for SIGTSTP (Ctrl+Z)
    sa.sa_handler = SIG_DFL;
    sigaction(SIGTSTP, &sa, NULL);
}