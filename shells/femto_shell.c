#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define CMD_IDX_0        0
#define CMD_IDX_1        1
#define UNDEFINED_CMD    (-1)
#define MAX_TOKENS_COUNT 1024
#define TOTAL_SUP_CMD    2
#define ERR_INVL_CMD     "ERROR: Invalid command\n"
#define END_MSG          "Good Bye!\n"

typedef void (*cmdFuncPtr_t)(void);
typedef struct
{
    char  *sup_cmds;
    size_t cmd_id;
} cmdConfig_t;

// set the supported commands as const for now
const char *sup_commands[] = {"echo", "exit"};

// function prototypes
int  FS_get_cmd_idx(char *cmdName);
void FS_exec_echo(char *argv[]);
int  FS_exec_exit(char *argv[]);
void FS_fill_cmdDB(cmdConfig_t *totalcmds, int cmdIdx);

int  main(int argc, char *argv[])
{
    /*! add signal handling to our shell so that it can be responsive to
            1. ctrl+D
            2. ctrl+\
            3. ctrl+z
    */
    struct sigaction sa;
    // handling Ctrl+C signal
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIG_IGN; // ignore action for SIGINT
    sa.sa_flags   = 0;
    sigaction(SIGINT, &sa, NULL);
    // handling Ctrl+\ signal (core dump)
    sa.sa_handler = SIG_DFL; // default action for SIGQUIT
    sigaction(SIGQUIT, &sa, NULL);
    // handling Ctrl+Z signal
    sa.sa_handler = SIG_DFL;
    sigaction(SIGTSTP, &sa, NULL);

    /*! handling shared memory for child/parent IPC */
    const char *sh_name1 = "/exit_flag";
    const char *sh_name2 = "/exit_code";
    int         shm_fd1  = shm_open(sh_name1, O_CREAT | O_RDWR, 0666);
    int         shm_fd2  = shm_open(sh_name2, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd1, sizeof(int));
    ftruncate(shm_fd2, sizeof(int));
    int *exit_flag = (int *)mmap(0, sizeof(int), PROT_READ | PROT_WRITE,
                                 MAP_SHARED, shm_fd1, 0);
    int *exit_code = (int *)mmap(0, sizeof(int), PROT_READ | PROT_WRITE,
                                 MAP_SHARED, shm_fd2, 0);
    /** define local variables */
    char       *buf        = NULL;
    char       *token      = NULL;
    char      **tokens_Arr = NULL;
    size_t      bufcount   = 0;
    ssize_t     nread      = 0;
    cmdConfig_t sup_cmds[TOTAL_SUP_CMD];

    for (int i = TOTAL_SUP_CMD - 1; i >= 0; --i)
    {
        FS_fill_cmdDB(&(sup_cmds[i]), i);
    }

    while (1)
    {
        if (write(STDOUT_FILENO, "FS> ", strlen("FS> ")) != 4)
        {
            perror("write");
            exit(1);
        }
        fflush(stdout);
        // get the shell input from the user to be parsed later.
        nread = getline(&buf, &bufcount, stdin);
        if (nread == -1)
        {
            if (feof(stdin))
            {
                // End of file reached only when ctrl+D is pressed. exit the
                // program.
                break;
            }
            else if (errno == EINVAL)
            {
                perror("getline error: ");
                continue;
            }
            // "EINVAL" error number shouldn't happen as there is no valid
            // reason to occur.
        }
        else
        {
            tokens_Arr = (char **)malloc(sizeof(char *) * MAX_TOKENS_COUNT);

            token      = strtok(buf, " \n");
            int i      = 0;
            while (token)
            {
                tokens_Arr[i] = token;
                token         = strtok(NULL, " \n");
                i++;
            }
            tokens_Arr[i] = NULL;

            pid_t PID     = fork();

            if (PID == -1) // failed to create child proc
            {
                perror("Femto shell break-down :(");
                // clean up and exit!
                if (tokens_Arr)
                {
                    free((void *)tokens_Arr);
                    tokens_Arr = NULL;
                }
                free(buf);
                shm_unlink(sh_name1);
                shm_unlink(sh_name2);
                exit(errno);
            }
            else if (PID == 0) // child process
            {
                switch (FS_get_cmd_idx(tokens_Arr[0]))
                {
                case CMD_IDX_0:

                    FS_exec_echo(&tokens_Arr[0]);
                    break;
                case CMD_IDX_1:
                    *exit_code = FS_exec_exit(&tokens_Arr[1]);
                    *exit_flag = 1;
                    break;
                default:
                    write(STDERR_FILENO, ERR_INVL_CMD, strlen(ERR_INVL_CMD));
                    break;
                }
                // terminate the child proc
                if (tokens_Arr)
                {
                    free((void *)tokens_Arr);
                    tokens_Arr = NULL;
                }
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
                if (tokens_Arr)
                {
                    free((void *)tokens_Arr);
                    tokens_Arr = NULL;
                }

                // Break out of the main loop if exit flag is set
                if (*exit_flag)
                {
                    break;
                }
            }
        }
    }

    // clean up
    free(buf);
    shm_unlink(sh_name1);
    shm_unlink(sh_name2);

    return *exit_code;
}

void FS_exec_echo(char *argv[])
{
    if (execve("/usr/bin/echo", argv, NULL) == -1)
    {
        perror("failed with execve");
        exit(errno);
    }
}

int FS_exec_exit(char *argv[])
{
    // exit with 0 if no arguments or error code if any
    write(STDOUT_FILENO, END_MSG, strlen(END_MSG));
    if (argv[0] != NULL)
    {
        int retCode = atoi(argv[0]);
        if (retCode != 0)
        {
            return retCode;
        }
    }

    return EXIT_SUCCESS;
}

void FS_fill_cmdDB(cmdConfig_t *totalcmds, int cmdIdx)
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

inline int FS_get_cmd_idx(char *cmdName)
{
    for (int i = 0; i < TOTAL_SUP_CMD; ++i)
    {
        if (strcmp(cmdName, sup_commands[i]) == 0)
        {
            return i;
        }
    }

    // command not found
    return -1;
}