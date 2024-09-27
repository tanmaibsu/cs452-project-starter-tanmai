#include <stdio.h>
#include "../src/lab.h"
#include <readline/readline.h>
#include <readline/history.h>
#include <stdbool.h>
#include <string.h>

bool create_process(char **argv, struct shell *sh, bool background);
bool is_background(char *line);

int main(int argc, char **argv)
{

  parse_args(argc, argv);

  // printf("argc=%c, argv= %c\n", argc, **argv);
  
  struct shell terminal;
  sh_init(&terminal);
  char *line;

  printf("I am here\n");
  using_history();

  printf("I am here too\n");
  while((line = readline(terminal.prompt))) {
    line = trim_white(line);
    add_history(line);
    char **argv = cmd_parse(line);
    bool executed_builtin = do_builtin(&terminal, argv);
    if(executed_builtin) {
        printf("executed built in\n");
        cmd_free(argv);
        free(line);
    } else {
        bool background = is_background(line);
        // bool process_created = create_process(argv, &terminal, background);
        create_process(argv, &terminal, background);
    }
    // printf("Line: %s\n", line);
  }

  return 0;
}


bool create_process(char **argv, struct shell *sh, bool background) {
    if (argv == NULL || argv[0] == NULL) return 0;

    pid_t pid;
    // int status;

    pid = fork();
    if(pid == 0) {
      if(!background) {
        pid_t child = getpid();
        setpgid(child, child);
        tcsetpgrp(sh->shell_terminal, child);

        signal (SIGINT, SIG_DFL);
        signal (SIGQUIT, SIG_DFL);
        signal (SIGTSTP, SIG_DFL);
        signal (SIGTTIN, SIG_DFL);
        signal (SIGTTOU, SIG_DFL);
      }
      execvp(argv[0], argv);
      exit(EXIT_FAILURE);
    }
    else if(pid < 0) {
      perror("fork failed");
    }

}

bool is_background(char *line) {
  size_t len = strlen(line);

  while(len > 0 && isspace(line[len - 1])) {
    len--;
  }

  if(len > 0 && line[len - 1] == '&') {
    line[len - 1] = '\0';
    return true;
  }

  return false;
}
