#include <stdio.h>
#include "../src/lab.h"
// #include "../src/lab.c"
#include <readline/readline.h>
#include <readline/history.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <termios.h>
#include <errno.h>

void create_process(char **argv, struct shell *sh, bool background);
bool check_background(char *line);

int main(int argc, char **argv)
{

  parse_args(argc, argv);

  if (argc > 1 && strcmp(argv[1], "-v") == 0)
  {
    return 0;
  }

  struct shell terminal = {0};
  sh_init(&terminal);
  char *line;

  using_history();

  while ((line = readline(terminal.prompt)))
  {
    line = trim_white(line);
    if (strlen(line) == 0)
    {
      printf("line == %s\n", line);
      free(line);
      // cmd_free(argv);
      continue;
    }
    check_jobs();
    add_history(line);
    bool background = check_background(line);
    char **argv = cmd_parse(line);
    bool executed_builtin = do_builtin(&terminal, argv);
    if (executed_builtin)
    {
      printf("executed_builtin \n");
      cmd_free(argv);
      free(line);
    }
    else
    {
      printf("create process \n");
      create_process(argv, &terminal, background);
      printf("done execution");
      cmd_free(argv);
      free(line);
    }
  }

  cleanup_jobs();
  sh_destroy(&terminal);

  return 0;
}

void create_process(char **argv, struct shell *sh, bool background)
{
  pid_t pid;
  int status;

  // Create a child process
  pid = fork();

  if (pid < 0)
  {
    // If fork() fails
    perror("fork failed");
    exit(1);
  }
  else if (pid == 0)
  {

    // if (!background)
    // {
    //   // printf("I am in background\n");

    // }
    pid_t child = getpid();
    setpgid(child, child);
    tcsetpgrp(sh->shell_terminal, child);

    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    // Execute the command
    execvp(argv[0], argv);
    exit(EXIT_FAILURE);
  }
  else
  {
    setpgid(pid, pid);
    if (background)
    {
      add_job(pid, argv);
      printf("job added successfully\n");
    }
    else
    {
      tcsetpgrp(STDIN_FILENO, pid);
      waitpid(pid, &status, WUNTRACED);
      tcsetpgrp(STDIN_FILENO, getpgrp());
    }
    // Parent process: wait for the child to complete

    // // Check if the child process exited successfully
    // if (WIFEXITED(status))
    // {
    //   return;
    //   // printf("Child process exited with status %d\n", WEXITSTATUS(status));
    // }
  }
  // return;
}

bool check_background(char *line)
{
  size_t len = strlen(line);
  while (len > 0 && isspace(line[len - 1]))
    len--;
  if (len > 0 && line[len - 1] == '&')
  {
    line[len - 1] = '\0';
    return true;
  }
  return false;
}
