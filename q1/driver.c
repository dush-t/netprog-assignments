#include "./shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int receivedSigInt = 0;

static void sigIntHandler(int sig_num);

int main()
{
  hash_map *sc_map = init_map(19);
  signal(SIGINT, sigIntHandler);
  for (;;)
  {
    receivedSigInt = false;
    command_pipe *cmd_pipe = initCmdPipe();
    char *cmd_input;
    size_t in_size = 0;
    printf("\nshell> ");
    int count = getline(&cmd_input, &in_size, stdin);
    if (count == 0 || !cmd_input || strcmp(cmd_input, "\n") == 0)
    {
      continue;
    }
    if (receivedSigInt)
    {
      // the input must be the index of the command to be executed
      int index = stringToNum(cmd_input);
      resetCmdPipe(cmd_pipe);
      cmd_pipe = (command_pipe *)find_in_map(sc_map, index);
      if (cmd_pipe == NULL)
      {
        printf("Short cut command with index %d not found.\n", index);
        continue;
      }
      executeCmdPipe(cmd_pipe, tcgetpgrp(STDIN_FILENO));
    }
    else
    {
      if (strcmp(cmd_input, "exit\n") == 0)
      {
        return 0;
      }
      if (createCmdPipe(cmd_input, cmd_pipe, sc_map))
      {
        // execute command if it is not a shortcut command

        executeCmdPipe(cmd_pipe, tcgetpgrp(STDIN_FILENO));
        // do not reset command pipe if it is a background command
        if (!(cmd_pipe->is_background))
          resetCmdPipe(cmd_pipe);
      }
    }
  }
  return 0;
}

static void sigIntHandler(int sig_num)
{
  assert(sig_num == SIGINT, "[sigIntHandler] received unexpected signal");
  printf("\nEnter command index: ");
  fflush(stdout);
  receivedSigInt = true;
}