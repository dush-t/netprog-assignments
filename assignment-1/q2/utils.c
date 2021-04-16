#include "utils.h"

/**
 * @brief Print error and exit. Cleanup by closing fd1 and fd2.
 * 
 * @param err 
 */
void errExit(char *err, int fd1, int fd2)
{
  close(fd1);
  close(fd2);
  perror("ERROR:\n");
  perror(err);
  exit(EXIT_FAILURE);
}

/**
 * @brief Assert condition. If it fails, exit and cleanup.
 * 
 * @param condition 
 * @param error_string 
 * @param fd1 
 * @param fd2 
 */
void assert(bool condition, char *error_string, int fd1, int fd2)
{
  if (!condition)
  {
    errExit(error_string, fd1, fd2);
  }
}

/**
 * @brief Setup a server on given port and return the socket fd
 * 
 * @param port 
 * @return int 
 */
int serverSetup(int port)
{
  struct sockaddr_in saddr;
  int sfd;
  memset(&saddr, 0, sizeof(saddr));

  saddr.sin_port = htons(port);
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = htonl(INADDR_ANY);

  assert((sfd = socket(PF_INET, SOCK_STREAM, 0)) != -1, "server setup error", -1, -1);
  assert(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) != -1, "setsockopt error", sfd, -1);
  assert(bind(sfd, (struct sockaddr *)&saddr, sizeof(saddr)) != -1, "server bind error", sfd, -1);
  assert(listen(sfd, TCP_BACKLOG) != -1, "server listen error", sfd, -1);

  return sfd;
}

/**
 * @brief Parse the config file having (n1 IP) lines
 * 
 * @return parsed_config* 
 */
parsed_config *parseConfigFile()
{
  FILE *fptr = fopen(CONFIG_FILE_PATH, "r");
  assert(fptr != NULL, "config file open error", -1, -1);
  parsed_config *config = (parsed_config *)calloc(1, sizeof(parsed_config));

  int ip_arr_sz = 10;
  char **ip_arr = (char **)calloc(MAX_CLIENTS_ALLOWED + 1, sizeof(char *));
  char id[10], ip[15];
  int i = 0;
  while (fscanf(fptr, " %s", id) != EOF)
  {
    fscanf(fptr, " %s", ip);
    assert(i < MAX_CLIENTS_ALLOWED, "more number of clients than allowed!", -1, -1);
    ip_arr[i] = strdup(ip);
    i++;
  }
  ip_arr[i] = NULL;

  config->data = ip_arr;
  config->count = i;
  return config;
}

/**
 * @brief Free up memory by resetting config file object
 * 
 * @param config 
 */
void resetConfigObj(parsed_config *config)
{
  char **temp = config->data;
  while (*temp != NULL)
  {
    free(temp);
    temp++;
  }
  config->count = 0;
  free(config->data);
}

/**
 * @brief Init command pipe memory
 * 
 * @return struct command_pipe* 
 */
struct command_pipe *initCommandPipe()
{
  struct command_pipe *cmd_pipe = (struct command_pipe *)calloc(1, sizeof(struct command_pipe));
  cmd_pipe->head = NULL;
  cmd_pipe->count = 0;
  return cmd_pipe;
}

/**
 * @brief Free up memory by resetting command pipe object
 * 
 * @param cmd_pipe 
 */
void resetCommandPipe(struct command_pipe *cmd_pipe)
{
  struct command *head = cmd_pipe->head;
  while (cmd_pipe->count > 0)
  {
    struct command *temp = head->next;
    cmd_pipe->count -= 1;
    free(head);
    head = temp;
  }
  cmd_pipe->head = NULL;
}

/**
 * @brief Insert a command at the end of pipe
 * 
 * @param cmd_pipe 
 * @param cmd 
 */
void insertCommandInPipe(struct command_pipe *cmd_pipe, struct command *cmd)
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
 * @brief Create the command pipe from command input
 * If no machine is specified in a command (eg: ls), then machine_name = -1
 * If it is a broadcast (eg: n*.ls), then machine_name = 0
 * 
 * @param cmd_input 
 * @param cmd_pipe 
 */
void createCommandPipe(char *cmd_input, struct command_pipe *cmd_pipe)
{
  char *tok = strtok(cmd_input, "|");
  while (tok != NULL)
  {
    struct command *cmd = (struct command *)calloc(1, sizeof(struct command));
    char *dot;
    if ((dot = strchr(tok, '.')) != NULL)
    {
      int dot_idx = (int)(dot - tok);

      if (dot_idx > 0 && (isdigit(tok[dot_idx - 1]) || tok[dot_idx - 1] == '*') && dot_idx >= 2)
      { // just some assumptions to make dot thing more robust
        char *machine_name = (char *)calloc(10, sizeof(char));
        int i;
        for (i = 0; i < dot_idx; i++)
        {
          machine_name[i] = tok[i];
        }
        machine_name[i] = '\0';
        char *machine_name_begin_ptr = machine_name;

        while (*machine_name == ' ')
          machine_name++;

        if (*machine_name != 'n')
        {
          printf("Machine name %s does not begin with 'n'\n", machine_name);
          pthread_exit(NULL);
        }

        int machine;
        if (i < 2)
        {
          printf("Error while converting machine name (%s) from string to int, make sure command is like n1.ls\n", machine_name);
          pthread_exit(NULL);
        }
        if (machine_name[1] == '*')
        {
          machine = 0; // broadcast
        }
        else
        {
          machine = atoi(machine_name + 1);
          if (machine == 0)
          {
            printf("Machine name %s invalid.\n", machine_name);
            pthread_exit(NULL);
          }
        }
        cmd->machine = machine;
        char *command_str = tok + dot_idx + 1;
        int len = strlen(command_str);
        if (command_str[len - 1] == ' ' || command_str[len - 1] == '\n')
          command_str[len - 1] = '\0';
        cmd->cmd = command_str;

        free(machine_name_begin_ptr);
      }
      else
      {
        cmd->machine = -1; // run on local machine
        cmd->cmd = tok;
      }
    }
    else
    {
      cmd->machine = -1; // run on local machine
      cmd->cmd = tok;
    }

    insertCommandInPipe(cmd_pipe, cmd);
    tok = strtok(NULL, "|");
  }
}

void printCommand(struct command *cmd)
{
  printf("\n=== Begin Command ===\n");
  if (cmd->machine > 0)
    printf("Command: %s, Machine: n%d", cmd->cmd, cmd->machine);
  else if (cmd->machine == -1)
    printf("Command: %s, Machine: local", cmd->cmd);
  else if (cmd->machine == 0)
    printf("Command: %s, Machine: n*", cmd->cmd);
  else
    printf("Command: %s, Machine: invalid", cmd->cmd);
  printf("\n=== End Command ===\n");
  fflush(stdout);
}

void printCommandPipe(struct command_pipe *cmd_pipe)
{
  if (cmd_pipe == NULL)
  {
    printf("Empty command pipe!\n");
    fflush(stdout);
    return;
  }
  struct command *head = cmd_pipe->head;
  while (head)
  {
    printCommand(head);
    head = head->next;
  }
}

/**
 * @brief Setup TCP connection to a server at given address and port
 * 
 * @param addr 
 * @param port 
 * @param arg_fd 
 * @return int 
 */
int clientSetup(char *addr, int port, int arg_fd)
{
  struct sockaddr_in saddr;
  int sfd;
  memset(&saddr, 0, sizeof(saddr));

  saddr.sin_port = htons(port);
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = inet_addr(addr);

  if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    printf("Socket creation error while connecting to IP: %s, Port: %d.\n", addr, port);
    errExit("[clientSetup] socket creation error", arg_fd, -1);
  }
  if (connect(sfd, (struct sockaddr *)&saddr, (socklen_t)sizeof(saddr)) == -1)
  {
    printf("Could not connect to IP: %s, Port: %d.\n", addr, port);
    errExit("[clientSetup] connect error", arg_fd, sfd);
  }

  return sfd;
}