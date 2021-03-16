#include "./shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
  command_pipe *cmd_pipe = initCmdPipe();
  hash_map *sc_map = init_map(19);
  for (;;)
  {
    size_t in_size = 0;
    printf("shell> ");
    int count = getline(&cmd_input, &in_size, stdin);
    if (count == 0 || !cmd_input || strcmp(cmd_input, "\n") == 0)
    {
      continue;
    }
    createCmdPipe(cmd_input, cmd_pipe, sc_map);
    // executeCommands(cmd_pipe);
    // printCmdPipe(cmd_pipe);
    resetCmdPipe(cmd_pipe);
  }
  return 0;
}