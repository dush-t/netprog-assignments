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
  cmd_pipe->is_background = false;
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

/**
 * @brief Parse the given command input string and fill in fields of command* cmd object
 * 
 * @param cmd 
 * @param cmd_input 
 * @param par_offset 
 */
void parseCmd(command *cmd, char *cmd_input, int par_offset, pipe_count prev_pipe_count)
{
  cmd->token = cmd_input;
  cmd->par_offset = par_offset;
  cmd->prev_pipe_count = prev_pipe_count;

  int len = strlen(cmd_input);
  char curr_arg[MAX_ARG_LEN + 1];
  char prev_arg[MAX_ARG_LEN + 1];
  int curr_len = 0;
  int num_args = 0;

  strcpy(curr_arg, "");
  strcpy(prev_arg, "");

  for (int i = 0; i <= len; i++)
  {
    if (i < len)
    {
      char c = cmd_input[i];

      if (!isspace(c))
      { // if the character read is not a space, record it in the arg char array
        assert(curr_len < MAX_ARG_LEN, "argument length longer than max size allowed");
        curr_arg[curr_len++] = c;
        continue;
      }
    }

    if (curr_len == 0)
    { // if the length of arg is 0, continue
      continue;
    }

    if (i + 1 < len && isspace(cmd_input[i + 1]))
    { // if next char is a space as well, continue
      continue;
    }

    curr_arg[curr_len] = '\0'; // end string with null character
    curr_len = 0;              // reset length

    if (strcmp(prev_arg, ">") == 0 || strcmp(prev_arg, ">>") == 0)
    {
      // if the previous arg was an output redirection, store the current arg as the output file
      strcpy(cmd->out_file, curr_arg);
      strcpy(prev_arg, curr_arg);
      continue;
    }

    if (strcmp(prev_arg, "<") == 0)
    {
      // if the previous arg was an input redirection, store the current arg as the output file
      strcpy(cmd->in_file, curr_arg);
      strcpy(prev_arg, curr_arg);
      continue;
    }

    // update previous arg
    strcpy(prev_arg, curr_arg);

    if (strcmp(curr_arg, ">") == 0)
    {
      // string read is an output redirection (no append)
      cmd->out_redirect = true;
      cmd->out_append = false;
      continue;
    }

    if (strcmp(curr_arg, ">>") == 0)
    {
      // string read is an output redirection (append)
      cmd->out_redirect = true;
      cmd->out_append = true;
      continue;
    }

    if (strcmp(curr_arg, "<") == 0)
    {
      // string read is an input redirection
      cmd->in_redirect = true;
      continue;
    }

    assert(num_args < MAX_NUM_ARGS, "number of arguments exceeded maximum limit");

    (cmd->argv)[num_args] = (char *)calloc(curr_len + 1, sizeof(char));
    assert((cmd->argv)[num_args] != NULL, "calloc error (cmd->argv)[num_args]");

    // store the argument
    strcpy((cmd->argv)[num_args++], curr_arg);
  }

  cmd->argc = num_args;
  (cmd->argv)[num_args] = NULL;
}

/**
 * @brief Create command pipeline linked list from the input
 * 
 * @param cmd_input 
 * @param cmd_pipe 
 * @param sc_map 
 * @return true when it is not a shortcut command
 * @return false when it is a shortcut command
 */
bool createCmdPipe(char *cmd_input, command_pipe *cmd_pipe, hash_map *sc_map)
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

    if (!isdigit(cmd_input[i]))
    {
      printf("Short-cut command index not a number.\n");
      return false;
    }
    while (cmd_input[i] != ' ')
    {
      if (!isdigit(cmd_input[i]))
      {
        printf("Short-cut command index not a number.\n");
        return false;
      }
      idx = idx * 10 + cmd_input[i] - '0';
      i++;
    }

    if (sc_flag == 'i') // command is to be inserted in hash map
    {
      if (find_in_map(sc_map, idx) != NULL)
      {
        printf("\nCommand with index %d already exists, replace (yes/no)? ", idx);
        char choice[10];
        scanf("%s", choice);
        getchar(); // ignore newline that scanf leaves in buffer

        if (strcmp(choice, "yes") == 0)
          delete_from_map(sc_map, idx);
        else if (strcmp(choice, "no") == 0 || strcmp(choice, "") == 0)
          return false;
        else
        {
          printf("\nInvalid choice entered.");
          return false;
        }
      }
      command_pipe *cmd_pipe_2 = initCmdPipe(); // create a second command pipe for the actual command to be inserted in map
      createCmdPipe(cmd_input + i + 1, cmd_pipe_2, sc_map);
      insert_into_map(sc_map, idx, (command_pipe *)cmd_pipe_2);
      printf("Stored shortcut command.\n");
    }
    else // command is to be deleted in hash map
    {
      command_pipe *temp_pipe = (command_pipe *)find_in_map(sc_map, idx);
      resetCmdPipe(temp_pipe);
      delete_from_map(sc_map, idx);
      printf("Deleted shortcut command.\n");
    }

    return false;
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
        cmd_pipe->is_background = true;
        temp[len - 2] = '\0';
      }

      if (pipeCount == 1)
      {
        command *cmd = initCmd();
        parseCmd(cmd, temp, 0, SINGLE_PIPE);
        insertCmdInPipe(cmd_pipe, cmd);
      }
      else if (pipeCount == 2)
      {
        char *cmd1_str, *cmd2_str;
        cmd1_str = strsep(&temp, ",");
        cmd2_str = strsep(&temp, ",");
        assert(cmd1_str != NULL && cmd2_str != NULL, "expected 2 comma separated commands");

        command *cmd1 = initCmd(), *cmd2 = initCmd();
        parseCmd(cmd1, cmd1_str, -1, DOUBLE_PIPE);
        parseCmd(cmd2, cmd2_str, -2, DOUBLE_PIPE);
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
        parseCmd(cmd1, cmd1_str, -1, TRIPLE_PIPE);
        parseCmd(cmd2, cmd2_str, -2, TRIPLE_PIPE);
        parseCmd(cmd3, cmd3_str, -3, TRIPLE_PIPE);
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

    return true;
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
  free(cmd_pipe);
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
  printf("in_redirect: %d, out_redirect: %d, out_append: %d, par_offest: %d, pipe_count: %d\n", cmd->in_redirect, cmd->out_redirect, cmd->out_append, cmd->par_offset, cmd->prev_pipe_count);
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
  if (cmd_pipe == NULL)
  {
    printf("No command in pipeline.\n");
    return;
  }
  printf("is_background? %d\n\n", cmd_pipe->is_background);
  command *head = cmd_pipe->head;
  while (head)
  {
    printCmd(head);
    head = head->next;
  }
}

/**
 * @brief Close all file descriptors except the one in index except_idx
 * 
 * @param pipe_fd 
 * @param n 
 * @param except_idx 
 * @param is_read if true, close read FDs else close write FDs
 */
void closeAllFdExcept(int pipe_fd[][2], int n, int except_fd, bool is_read)
{
  for (int i = 0; i < n; i++)
  {
    int fd = pipe_fd[i][is_read ? 0 : 1];
    if (fd == except_fd)
      continue;
    close(pipe_fd[i][is_read ? 0 : 1]);
  }
}

void closeAllPipeFd(int pipe_fd[][2], int n)
{
  closeAllFdExcept(pipe_fd, n, -1, true);
  closeAllFdExcept(pipe_fd, n, -1, false);
}

void sigIntExecCmdHandler(int sig_num)
{
  assert(sig_num == SIGINT, "[sigIntExecCmdHandler] received unexpected signal");
  printf("\nPlease wait while the command finishes executing.\n");
  fflush(stdout);
}

/**
 * @brief Execute the commands in the given pipeline
 * 
 * @param cmd_pipe 
 * @param initial_pgrp
 */
void executeCmdPipe(command_pipe *cmd_pipe, pid_t initial_pgrp)
{
  if (cmd_pipe == NULL)
    return;

  command *curr_cmd = cmd_pipe->head;
  int count = cmd_pipe->count;
  bool is_background = cmd_pipe->is_background;

  int child_pid; // make a child to ensure it is not a group leader
  if ((child_pid = fork()) == -1)
  {
    errExit("fork error");
  }

  if (child_pid == 0)
  {
    // handle SIGINT when a command is running
    signal(SIGINT, sigIntExecCmdHandler);

    // if the child is in background and calls tcsetpgrp(),
    // SIGTTOU signal is triggered which should be ignored
    signal(SIGTTOU, SIG_IGN);

    // make child the group leader
    assert(setpgid(0, 0) == 0, "setpgid() error");

    // make the child process group foreground if it is not background
    if (!is_background)
    {
      assert(tcsetpgrp(STDIN_FILENO, getpgrp()) == 0, "tcsetpgrp() error");
    }

    int pipe_fd[count - 1][2]; // (count-1) pipe FDs
    // initialise pipes
    for (int i = 0; i < count - 1; i++)
    {
      assert(pipe(pipe_fd[i]) != -1, "pipe creation error");
    }

    int i;
    for (i = 0; i < count; i++, curr_cmd = curr_cmd->next)
    {
      pid_t pid;
      assert((pid = fork()) != -1, "fork error");

      if (pid == 0)
      {
        int my_pid = getpid();
        pid_t read_pipe = pipe_fd[i - 1][0];

        // logic for || and |||
        // if it is a comma separated command and there is another comma separated command on the right
        // then read data from the pipe, store it in a buffer and write the data to the pipe of next command
        // First make sure there is at least one command remaining
        if (i < count - 1)
        { // the current command is first in || or first/second in |||
          if (curr_cmd->par_offset == -1 || (curr_cmd->par_offset == -2 && curr_cmd->prev_pipe_count == TRIPLE_PIPE))
          {
            // read data from pipe and store into a buffer
            char buff[STDOUT_BUF_SIZE];
            int buff_cnt = 1, buff_idx = 0, read_cnt = 0;
            char *pipe_data = (char *)calloc(STDOUT_BUF_SIZE, sizeof(char));
            pipe_data[0] = '\0';
            while ((read_cnt = read(pipe_fd[i - 1][0], &buff, STDOUT_BUF_SIZE)) != 0)
            {
              strcpy(pipe_data + buff_idx, buff);
              if (read_cnt == STDOUT_BUF_SIZE)
              {
                // increase buffer size if all bytes were consumed
                pipe_data = (char *)realloc(pipe_data, STDOUT_BUF_SIZE * buff_cnt);
              }
              buff_idx += read_cnt;
              buff_cnt++;
            }

            // create a temp pipe for the current command
            // this is because the pipe_fd[i-1] data would have been destructed on read
            // and we cannot write back to it as we would have closed the write fd
            int temp_fd[2];
            assert(pipe(temp_fd) != -1, "temp_fd pipe creation error");
            // write data to the temp pipe
            assert(write(temp_fd[1], pipe_data, buff_idx) != -1, "temp_fd[1] write error");
            read_pipe = -1;
            close(temp_fd[1]);                                                  // close write end to prevent read blocking forever
            assert(dup2(temp_fd[0], STDIN_FILENO) != -1, "temp_fd dup2 error"); // make stdin read end of pipe

            // write data to the to the next pipe
            assert(write(pipe_fd[i][1], pipe_data, buff_idx) != -1, "pipe_fd[i][1] write error");
          }
        }

        // make stdin read end of pipe for all processes except first one
        if (i != 0)
        {
          closeAllFdExcept(pipe_fd, count - 1, read_pipe, true);
          if (read_pipe != -1) // will be -1 when the current command is first in || or first/second in |||
            assert(dup2(read_pipe, STDIN_FILENO) != -1, "dup2 read fd error");
        }
        else
        {
          closeAllFdExcept(pipe_fd, count - 1, -1, true);
        }

        // make stdout write end of pipe for all processes except last
        if (i != count - 1)
        {
          int write_fd = pipe_fd[i][1];

          // logic for write pipe in case of || and |||
          // note that if it is the first command after ||, it should not write to the pipe of next command
          // instead, it should write to the next to next command
          // similarly for others
          if (curr_cmd->par_offset == -1)
          { // the command is first after || or |||
            if (curr_cmd->prev_pipe_count == DOUBLE_PIPE)
            { // command is first after ||
              if (i != count - 2)
              { // make sure command is not second last as then output should be to stdout
                write_fd = pipe_fd[i + 1][1];
              }
              else
              {
                write_fd = -1;
              }
            }
            else if (curr_cmd->prev_pipe_count == TRIPLE_PIPE)
            { // command is first after |||
              if (i != count - 3)
              { // make sure command is not third last as then output should be to stdout
                write_fd = pipe_fd[i + 2][1];
              }
              else
              {
                write_fd = -1;
              }
            }
          }
          else if (curr_cmd->par_offset == -2)
          { // the command is second after || or |||

            if (curr_cmd->prev_pipe_count == TRIPLE_PIPE)
            { // command is second after |||
              if (i != count - 2)
              { // make sure command is not second last as then output should be to stdout
                write_fd = pipe_fd[i + 1][1];
              }
              else
              {
                write_fd = -1;
              }
            }
          }

          closeAllFdExcept(pipe_fd, count - 1, write_fd, false);
          if (write_fd != -1)
            assert(dup2(write_fd, STDOUT_FILENO) != -1, "dup2 write fd error");
        }
        else
        {
          closeAllFdExcept(pipe_fd, count - 1, -1, false);
        }

        // output redirection
        if (curr_cmd->out_redirect)
        {
          int flags = O_WRONLY | O_CREAT;
          if (curr_cmd->out_append)
            flags |= O_APPEND;
          else
            flags |= O_TRUNC;

          int out_file_fd = open(curr_cmd->out_file, flags, 0777);
          assert(out_file_fd != -1, "output file open error");

          assert(dup2(out_file_fd, STDOUT_FILENO) != -1, "dup2 output file fd error");
          assert(close(out_file_fd) == 0, "close output file fd error");
        }

        // input redirection
        if (curr_cmd->in_redirect)
        {
          int in_file_fd = open(curr_cmd->out_file, O_RDONLY, 0777);
          assert(in_file_fd != -1, "input file open error");

          assert(dup2(in_file_fd, STDIN_FILENO) != -1, "dup2 input file fd error");
          assert(close(in_file_fd) == 0, "close input file fd error");
        }

        assert(execvp((curr_cmd->argv)[0], curr_cmd->argv) == 0, "execv error");
      }
      else
      {
        if (i != count - 1) // close write end to prevent read blocking forever
          close(pipe_fd[i][1]);

        if (i != 0) // close read end to prevent error
          close(pipe_fd[i - 1][0]);

        assert(wait(NULL) != -1, "wait error");
      }
    }

    // close all pipes
    closeAllPipeFd(pipe_fd, count - 1);

    // give foreground process control back to shell(parent) on exit if (!is_background)
    if (!is_background)
    {
      assert(tcsetpgrp(STDIN_FILENO, initial_pgrp) == 0, "tcsetpgrp(): foreground control back to shell error");
    }

    _exit(EXIT_SUCCESS);
  }
  else
  {

    // make the child process group foreground if it is not background
    // Note that the same code for setpgid() and tcsetpgrp () is there in the child
    // to avoid race conditions. We call at both places as we do not know whether child
    // will run first or the parent.
    if (!is_background)
    {
      // ensure the child is group leader
      setpgid(child_pid, child_pid);
      tcsetpgrp(STDIN_FILENO, child_pid);
      assert(wait(NULL) != -1, "wait error");
    }
  }
}