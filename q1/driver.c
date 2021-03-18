#include "./shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
  hash_map *sc_map = init_map(19);
  for (;;)
  {
    command_pipe *cmd_pipe = initCmdPipe();
    size_t in_size = 0;
    printf("shell> ");
    int count = getline(&cmd_input, &in_size, stdin);
    if (count == 0 || !cmd_input || strcmp(cmd_input, "\n") == 0)
    {
      continue;
    }
    createCmdPipe(cmd_input, cmd_pipe, sc_map);
    executeCmdPipe(cmd_pipe);
    // printCmdPipe(cmd_pipe);

    // VERIFY: do not reset command pipe if it is a background command
    if (!(cmd_pipe->is_background))
      resetCmdPipe(cmd_pipe);
  }
  return 0;
}