#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <pwd.h>
#include "../src/lab.h"
#include <errno.h>
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
        // when env prompts env present
        prompt = malloc(strlen(env_prompt) + 1);

        if (prompt != NULL)
        {
            strcpy(prompt, env_prompt);
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

    printf("dir = %s\n", dir[1]);
    if (dir[1] == NULL)
    {
        // path = getenv("HOME");
        // printf("path = %s\n", path);
        if (path == NULL)
        {
            printf("I am in path == NULL\n");
            struct passwd *pw = getpwuid(getuid());
            if (pw == NULL)
            {
                perror("getpwuid");
                return -1;
            }
            path = pw->pw_dir;
            printf("path = %s\n", path);
        }
        else
        {
            path = dir[1];
        }
    } else {
        path = dir[1];
    }

    if (chdir(path) == 0)
    {
        printf("directory changed successfully\n");
        return 0;
    }
    else
    {
        printf("Error changing directory to '%s': %s\n", path, strerror(errno));
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
    long arg_max = sysconf(_SC_ARG_MAX);
    if (arg_max == -1)
    {
        arg_max = 4096;
    }

    char **argv = malloc(sizeof(char *) * (arg_max + 1));
    char *mutable_line = strdup(line);

    char *token = strtok(mutable_line, " \t\n");
    int i = 0;
    while (token != NULL && i < arg_max)
    {
        argv[i] = strdup(token);
        if (argv[i] == NULL)
        {
            cmd_free(argv);
            free(mutable_line);
            return NULL;
        }
        token = strtok(NULL, " \t\n");
        i++;
    }
    argv[i] = NULL;

    free(mutable_line);
    return argv;
}

/**
 * @brief Free the line that was constructed with parse_cmd
 *
 * @param line the line to free
 */
void cmd_free(char **line)
{
    if(line == NULL) {
        return;
    } else {
        k = 0;
        while(line[k] != NULL) {
            free(line[k]);
        }

        free(line[k]);
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
    printf("I am here: builtin\n");
    printf("%s\n", argv[0]);

    if (argv[0] == NULL)
    {
        return false;
    }

    if (strcmp(argv[0], "exit") == 0)
    {
        printf("I exist\n");
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
        }
        else
        {
            printf("Error: %s\n", strerror(errno));
            return 1;
        }

        return 0;
    }

    if (strcmp(argv[0], "history") == 0)
    {
        // using_history();
        HIST_ENTRY **h_list;
        h_list = history_list();
        if(h_list) {
            for (int i = 0; h_list[i]; i++)
            {
                printf("%d %s\n", i + history_base, h_list[i]->line);
            }
        }
    }
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
    int c;

    while ((c = getopt(argc, argv, "v")) != -1)
    {
        switch (c)
        {
        case 'v':
            printf("Shell Version: %d, %d\n", lab_VERSION_MAJOR, lab_VERSION_MINOR);
            return;
        case '?':
            if (isprint(optopt))
                fprintf(stderr, "Unknown \n", optopt);
            else
                fprintf(stderr, "Unknown \n", optopt);
            return;
        default:
            abort();
        }
    }
}