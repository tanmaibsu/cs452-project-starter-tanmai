#include "../src/lab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <pwd.h>
#include <errno.h>
#include <termios.h>
#include <limits.h>

/**
 * @brief Set the shell prompt. This function will attempt to load a prompt
 * from the requested environment variable, if the environment variable is
 * not set a default prompt of "shell>" is returned.  This function calls
 * malloc internally and the caller must free the resulting string.
 *
 * @param env The environment variable
 * @return const char* The prompt
 */

// Structure to store background job information

struct job jobs[MAX_JOBS];
int n_jobs = 0;

void add_job(pid_t pid, const char **argv)
{
    if (n_jobs < MAX_JOBS)
    {
        jobs[n_jobs].job_id = n_jobs + 1;
        jobs[n_jobs].pid = pid;
        jobs[n_jobs].command = strdup(argv[0]);
        jobs[n_jobs].status = strdup("Running");
        jobs[n_jobs].active = 1;
        printf("[%d] %d %s\n", jobs[n_jobs].job_id, jobs[n_jobs].pid, argv[0]);
        n_jobs++;
    }
}

void check_jobs()
{
    printf("jobs checked\n");
    printf("Number of jobs: %d\n", n_jobs);
    for (int i = 0; i < n_jobs; i++)
    {
        if (jobs[i].active)
        {
            int status;
            printf("pid: %d\n", jobs[i].pid);
            pid_t result = waitpid(jobs[i].pid, &status, WNOHANG);
            printf("result: %d\n", result);
            if (result > 0)
            { // The process has finished
                jobs[i].active = 0;
                jobs[i].status = strdup("Done");
                free(jobs[i].status);
            }
        }
    }
}

void show_jobs()
{
    for (int i = 0; i < n_jobs; i++)
    {
        // printf("[%d] %d %s %s\n", jobs[i].job_id, jobs[i].pid, jobs[i].status, jobs[i].command);
        if (jobs[i].active || strcmp(jobs[i].status, "Done") == 0)
        {
            printf("[%d] %d %s %s\n", jobs[i].job_id, jobs[i].pid, jobs[i].status, jobs[i].command);
        }
    }
}

void cleanup_jobs()
{
    for (int i = 0; i < n_jobs; i++)
    {
        free(jobs[i].command);
        free(jobs[i].status);
    }
}

char *get_prompt(const char *env)
{
    char *prompt = NULL;
    const char *default_prompt = "shell>";

    const char *env_prompt = getenv(env);

    if (env_prompt == NULL)
    {
        prompt = malloc(strlen(default_prompt) + 1);

        // malloc returns NULL in case of not enough memory
        if (prompt != NULL)
        {
            strcpy(prompt, default_prompt);
        }
    }
    else
    {
        // when env prompt presents
        prompt = malloc(strlen(env_prompt) + 1);

        if (prompt != NULL)
        {
            strcpy(prompt, env_prompt);
            // free(env);
        }
    }

    return prompt;
}

/**
 * Changes the current working directory of the shell. Uses the linux system
 * call chdir. With no arguments the users home directory is used as the
 * directory to change to.
 *
 * @param dir The directory to change to
 * @return  On success, zero is returned.  On error, -1 is returned, and
 * errno is set to indicate the error.
 */

int change_dir(char **dir)
{
    const char *path = NULL;
    // if (dir[1] != NULL) {
    //     path = dir[1];
    // } else {
    //     path = getenv("HOME");
    //     // path = (char *)malloc(strlen(getenv("HOME")) + 1);
    // }
    if (dir[1] == NULL)
    {
        path = getenv("HOME");
        // printf("path = %s\n", path);
        if (path == NULL)
        {
            struct passwd *pw = getpwuid(getuid());
            if (pw == NULL)
            {
                perror("getpwuid");
                return -1;
            }
            path = pw->pw_dir;
        }
        // else
        // {
        //     path = dir[1];
        // }
    }
    else
    {
        path = dir[1];
    }

    if (chdir(path) == 0)
    {
        printf("directory changed successfully.\n");
        // free(pw);
        // free(path);
        return 0;
    }
    else
    {
        printf("Error changing directory to '%s': %s\n", path, strerror(errno));
        // free(pw);
        // free(path);
        return -1;
    }
}

/**
 * @brief Convert line read from the user into to format that will work with
 * execvp. We limit the number of arguments to ARG_MAX loaded from sysconf.
 * This function allocates memory that must be reclaimed with the cmd_free
 * function.
 *
 * @param line The line to process
 *
 * @return The line read in a format suitable for exec
 */

char **cmd_parse(char const *line)
{
    if (line == NULL || *line == '\0')
    {
        return NULL;
    }

    long arg_max = sysconf(_SC_ARG_MAX);
    if (arg_max == -1)
    {
        perror("sysconf failed");
        return NULL;
    }

    char **argv = malloc(sizeof(char *) * (arg_max + 1));
    if (argv == NULL)
    {
        free(argv);
        perror("malloc failed");
        return NULL;
    }

    char *line_copy = strdup(line);
    if (line_copy == NULL)
    {
        free(argv);
        perror("strdup failed");
        return NULL;
    }

    char *token = strtok(line_copy, " \t\n");
    int argc = 0;

    while (token != NULL && argc < arg_max)
    {
        argv[argc] = strdup(token); // Duplicate token
        if (argv[argc] == NULL)
        {
            cmd_free(argv);
            free(line_copy);
            perror("strdup failed");
            return NULL;
        }
        argc++;
        token = strtok(NULL, " \t\n");
    }

    argv[argc] = NULL;

    free(line_copy);

    return argv;
}

/**
 * @brief Free the line that was constructed with parse_cmd
 *
 * @param line the line to free
 */
void cmd_free(char **line)
{
    if (line == NULL)
    {
        return;
    }
    else
    {
        int k = 0;
        while (line[k] != NULL)
        {
            free(line[k]);
            k++;
        }

        free(line);
    }
}

/**
 * @brief Trim the whitespace from the start and end of a string.
 * For example "   ls -a   " becomes "ls -a". This function modifies
 * the argument line so that all printable chars are moved to the
 * front of the string
 *
 * @param line The line to trim
 * @return The new line with no whitespace
 */
char *trim_white(char *line)
{
    char *end;

    while (isspace((unsigned char)*line))
        line++;

    if (*line == 0)
    {
        return line;
    }

    end = line + strlen(line) - 1;
    while (end > line && isspace((unsigned char)*end))
        end--;

    *(end + 1) = '\0';

    return line;
}

/**
 * @brief Takes an argument list and checks if the first argument is a
 * built in command such as exit, cd, jobs, etc. If the command is a
 * built in command this function will handle the command and then return
 * true. If the first argument is NOT a built in command this function will
 * return false.
 *
 * @param sh The shell
 * @param argv The command to check
 * @return True if the command was a built in command
 */

bool do_builtin(struct shell *sh, char **argv)
{
    if (argv == NULL || argv[0] == NULL)
    {
        return false;
    }

    if (strcmp(argv[0], "exit") == 0)
    {
        sh_destroy(sh);
        exit(0);
    }

    if (strcmp(argv[0], "cd") == 0)
    {
        int directory_changed = change_dir(argv);

        if (directory_changed == 0)
        {
            return true;
        }

        return false;
    }

    if (strcmp(argv[0], "pwd") == 0)
    {
        char cwd[PATH_MAX];

        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            printf("%s\n", cwd);
            return true;
        }
        else
        {
            printf("Error: %s\n", strerror(errno));
            return false;
        }

        return false;
    }

    if (strcmp(argv[0], "history") == 0)
    {
        // using_history();
        HIST_ENTRY **h_list;
        h_list = history_list();
        if (h_list)
        {
            for (int i = 0; h_list[i]; i++)
            {
                printf("%d %s\n", i + history_base, h_list[i]->line);
            }
        }
        else
        {
            printf(" No History");
        }
        return true;
    }

    if (strcmp(argv[0], "jobs") == 0)
    {
        show_jobs();
        // free(argv);
        return true;
    }

    return false;
}

/**
 * @brief Initialize the shell for use. Allocate all data structures
 * Grab control of the terminal and put the shell in its own
 * process group. NOTE: This function will block until the shell is
 * in its own program group. Attaching a debugger will always cause
 * this function to fail because the debugger maintains control of
 * the subprocess it is debugging.
 *
 * @param sh
 */

void sh_init(struct shell *sh)
{
    // sh->prompt = get_prompt("MY_PROMPT");
    // sh->shell_terminal = STDIN_FILENO;
    // sh->shell_is_interactive = isatty(sh->shell_terminal);

    // if (!sh->shell_is_interactive)
    // {
    //     return; // No need for terminal and process group setup if not interactive
    // }

    // // Wait for the shell to be in its own process group
    // while (tcgetpgrp(sh->shell_terminal) != (sh->shell_pgid = getpgrp()))
    // {
    //     kill(sh->shell_pgid, SIGTTIN);
    // }

    // // Ignore terminal control signals
    // signal(SIGINT, SIG_IGN);
    // signal(SIGQUIT, SIG_IGN);
    // signal(SIGTSTP, SIG_IGN);
    // signal(SIGTTIN, SIG_IGN);
    // signal(SIGTTOU, SIG_IGN);

    // // Set the shell's process group ID to its PID
    // sh->shell_pgid = getpid();
    // if (setpgid(sh->shell_pgid, sh->shell_pgid) < 0)
    // {
    //     perror("Failed to put the shell in its own process group");
    //     exit(1);
    // }

    // // Set the controlling terminal to the shell's process group
    // if (tcsetpgrp(sh->shell_terminal, sh->shell_pgid) < 0)
    // {
    //     perror("Failed to set process group for terminal");
    //     exit(1);
    // }

    // // Save the current terminal settings
    // if (tcgetattr(sh->shell_terminal, &sh->shell_tmodes) < 0)
    // {
    //     perror("Failed to get terminal attributes");
    //     exit(1);
    // }
    sh->shell_terminal = STDIN_FILENO;
    sh->shell_is_interactive = isatty(sh->shell_terminal);

    if (sh->shell_is_interactive)
    {
        while (tcgetpgrp(sh->shell_terminal) != (sh->shell_pgid = getpgrp()))
        {
            kill(-sh->shell_pgid, SIGTTIN);
        }

        sh->shell_pgid = getpid();
        if (setpgid(sh->shell_pgid, sh->shell_pgid) < 0)
        {
            perror("Couldn't put the shell in its own process group");
            exit(1);
        }

        tcsetpgrp(sh->shell_terminal, sh->shell_pgid);
        tcgetattr(sh->shell_terminal, &sh->shell_tmodes);
    }

    sh->prompt = get_prompt("MY_PROMPT");
}

void sh_destroy(struct shell *sh)
{
    if (sh->prompt)
    {
        free(sh->prompt);
    }
}

void parse_args(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "v")) != -1)
    {
        switch (opt)
        {
        case 'v':
            printf("Shell version: %d.%d\n", lab_VERSION_MAJOR, lab_VERSION_MINOR);
            break;
        case '?':
            if (isprint(optopt))
                fprintf(stderr, "Unknown: '%c'\n", optopt);
            else
                fprintf(stderr, "Unknown: '%c'\n", optopt);
        default:
            abort();
        }
    }
}