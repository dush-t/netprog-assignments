#include "./shell.h"

/**
 * @brief Initialise a command pipeline object
 * 
 * @return command_pipe* 
 */
command_pipe *initCmdPipe()
{
  command_pipe *cmd_pipe = (command_pipe *)calloc(1, sizeof(command_pipe));
  assert(cmd_pipe != NULL, "not enough memory for command pipe object");
  cmd_pipe->count = 0;
  cmd_pipe->head = NULL;
  cmd_pipe->tail = NULL;
  cmd_pipe->isBackground = false;
  return cmd_pipe;
}

/**
 * @brief Initialise a command object
 * 
 * @return command* 
 */
command *initCmd()
{
  command *cmd = (command *)calloc(1, sizeof(command));
  assert(cmd != NULL, "not enough memory for command object");
  cmd->argc = 0;
  for (int i = 0; i < MAX_NUM_ARGS; i++)
    (cmd->argv)[i] = NULL;
  strcpy(cmd->in_file, "");
  strcpy(cmd->out_file, "");

  cmd->in_redirect = false;
  cmd->out_redirect = false;
  cmd->out_append = false;
  cmd->par_offset = -1;
  cmd->next = NULL;
  return cmd;
}

/**
 * @brief Insert command at the end of pipeline linked list
 * 
 * @param cmd_pipe 
 * @param cmd 
 */
void insertCmdInPipe(command_pipe *cmd_pipe, command *cmd)
{
  if (cmd_pipe->count == 0)
  {
    cmd_pipe->head = cmd;
    cmd_pipe->tail = cmd;
  }
  else
  {
    cmd_pipe->tail->next = cmd;
    cmd_pipe->tail = cmd_pipe->tail->next;
  }
  cmd_pipe->count += 1;
}

void parseCmd(command *cmd, char *cmd_input, int par_offset)
{
  // TO DO
  cmd->token = cmd_input;
  cmd->par_offset = par_offset;
}

/**
 * @brief Create command pipeline linked list from the input
 * 
 * @param cmd_input 
 * @param cmd_pipe 
 * @param sc_map 
 */
void createCmdPipe(char *cmd_input, command_pipe *cmd_pipe, hash_map *sc_map)
{
  int len = strlen(cmd_input); // length of input
  char buff[3];                // buffer to check where it is a short-cut command
  buff[0] = '\0';

  if (len > 2)
  {
    memcpy(buff, cmd_input, sizeof(char) * 2); // copy first 2 chars of input into buffer
    buff[2] = '\0';
  }

  if (strcmp(buff, "sc") == 0) // it is a shortcut command (sc)
  {
    cmd_pipe = NULL;
    char sc_flag = '\0';
    int sc_flag_idx = 0;
    int i;
    for (i = 0; i < len; i++)
      if (cmd_input[i] == '-')
        break;

    sc_flag_idx = i + 1; // index of the flag: i or d
    assert(sc_flag_idx < len, "short-cut command flag not found");

    sc_flag = cmd_input[sc_flag_idx];
    assert(sc_flag == 'i' || sc_flag == 'd', "short-cut command invalid flag, expected -i or -d");

    int idx = 0;
    i = sc_flag_idx + 2; // index from which the command's index (hash map key) begins
    while (cmd_input[i] != ' ')
    {
      assert(isdigit(cmd_input[i]), "short-cut command index not a number");
      idx = idx * 10 + cmd_input[i] - '0';
      i++;
    }

    if (sc_flag == 'i') // command is to be inserted in hash map
    {
      command_pipe *cmd_pipe_2 = initCmdPipe(); // create a second command pipe for the actual command to be inserted in map
      createCmdPipe(cmd_input + i + 1, cmd_pipe_2, sc_map);
      insert_into_map(sc_map, idx, (command_pipe *)cmd_pipe_2);
    }
    else // command is to be deleted in hash map
    {
      command_pipe *temp_pipe = (command_pipe *)find_in_map(sc_map, idx);
      resetCmdPipe(temp_pipe);
      delete_from_map(sc_map, idx);
    }
  }
  else // it is not a shortcut command
  {
    char *temp;
    int pipeCount = 1; // to check whether it is |, || or |||
    while ((temp = strsep(&cmd_input, "|")) != NULL)
    {
      if (strcmp(temp, "") == 0)
      {
        pipeCount++;
        continue;
      }

      int len = strlen(temp);
      assert(len > 0, "command is empty");
      if (temp[len - 2] == '&') // it is a background command
      {
        cmd_pipe->isBackground = true;
        temp[len - 2] = '\0';
      }

      if (pipeCount == 1)
      {
        command *cmd = initCmd();
        parseCmd(cmd, temp, -1);
        insertCmdInPipe(cmd_pipe, cmd);
      }
      else if (pipeCount == 2)
      {
        char *cmd1_str, *cmd2_str;
        cmd1_str = strsep(&temp, ",");
        cmd2_str = strsep(&temp, ",");
        assert(cmd1_str != NULL && cmd2_str != NULL, "expected 2 comma separated commands");

        command *cmd1 = initCmd(), *cmd2 = initCmd();
        parseCmd(cmd1, cmd1_str, -1);
        parseCmd(cmd2, cmd2_str, -2);
        insertCmdInPipe(cmd_pipe, cmd1);
        insertCmdInPipe(cmd_pipe, cmd2);
      }
      else if (pipeCount == 3)
      {
        char *cmd1_str, *cmd2_str, *cmd3_str;
        cmd1_str = strsep(&temp, ",");
        cmd2_str = strsep(&temp, ",");
        cmd3_str = strsep(&temp, ",");
        assert(cmd1_str != NULL && cmd2_str != NULL && cmd3_str != NULL, "expected 3 comma separated commands");

        command *cmd1 = initCmd(), *cmd2 = initCmd(), *cmd3 = initCmd();
        parseCmd(cmd1, cmd1_str, -1);
        parseCmd(cmd2, cmd2_str, -2);
        parseCmd(cmd3, cmd3_str, -3);
        insertCmdInPipe(cmd_pipe, cmd1);
        insertCmdInPipe(cmd_pipe, cmd2);
        insertCmdInPipe(cmd_pipe, cmd3);
      }
      else
      {
        errExit("Expected |, || or ||| pipes");
      }

      pipeCount = 1;
    }
  }
}

/**
 * @brief Reset (deallocate memory) command pipeline after running shell commands
 * 
 * @param cmd_pipe 
 */
void resetCmdPipe(command_pipe *cmd_pipe)
{
  if (cmd_pipe == NULL)
    return;
  command *head = cmd_pipe->head;
  while (cmd_pipe->count > 0)
  {
    command *temp = head->next;
    free(head);
    cmd_pipe->count -= 1;
    head = temp;
  }
  cmd_pipe->head = NULL;
  cmd_pipe->tail = NULL;
}

/**
 * @brief Utility function to print a single command
 * 
 * @param cmd 
 */
void printCmd(command *cmd)
{
  printf("\n--- BEGIN COMMAND ---\n");
  printf("token: %s\n", cmd->token);
  printf("in_redirect: %d, out_redirect: %d, out_append: %d, par_offest: %d\n", cmd->in_redirect, cmd->out_redirect, cmd->out_append, cmd->par_offset);
  printf("in_file: %s\n", cmd->in_file);
  printf("out_file: %s\n", cmd->out_file);
  printf("argc: %d\n", cmd->argc);
  printf("argv: ");
  for (int i = 0; i < cmd->argc; i++)
  {
    printf("%s, ", (cmd->argv)[i]);
  }
  printf("\n--- END COMMAND ---\n");
}

/**
 * @brief Utility function to print all commands in pipeline
 * 
 * @param cmd_pipe 
 */
void printCmdPipe(command_pipe *cmd_pipe)
{
  printf("isBackground? %d\n\n", cmd_pipe->isBackground);
  command *head = cmd_pipe->head;
  while (head)
  {
    printCmd(head);
    head = head->next;
  }
}