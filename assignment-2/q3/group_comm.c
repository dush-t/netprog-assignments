#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "utils.h"

void errExit(char *);
void printCommands();

int main()
{
  printf("\n##### WELCOME TO GROUP CHAT APPLICATION #####\n");
  printCommands();

  fd_set read_set;
  FD_ZERO(&read_set);
  FD_SET(STDIN_FILENO, &read_set);
  int max_fd = STDIN_FILENO;

  for (;;)
  {
    int nready = select(max_fd + 1, &read_set, NULL, NULL, NULL);
    if (nready == -1)
    {
      // TO DO
    }

    if (FD_ISSET(STDIN_FILENO, &read_set))
    {
      char *cmd_buf = (char *)calloc(COMMAND_LEN, sizeof(char));
      int cmd_buff_len = COMMAND_LEN;
      getline(&cmd_buf, (size_t *)&cmd_buff_len, stdin);

      struct command *cmd_obj = (struct command *)calloc(1, sizeof(struct command));
      parseCommand(cmd_buf, cmd_obj);
      if (cmd_obj == NULL)
      {
        // error
        // TO DO
      }

      free(cmd_buf);
      printf("Received command: \n");
      printCommand(cmd_obj);
    }
  }

  char *command = (char *)calloc(COMMAND_LEN, sizeof(char));
  int command_len = COMMAND_LEN;
  getline(&command, (size_t *)&command_len, stdin);
  struct command *cmd_obj = (struct command *)calloc(1, sizeof(struct command));
  parseCommand(command, cmd_obj);
  printCommand(cmd_obj);
}

void printCommands()
{
  printf("\nAvailable commands:");
  printf("\n> create-group [GROUP_NAME] [GROUP_IP] [GROUP_PORT]");
  printf("\n> join-group [GROUP_IP] [GROUP_PORT]");
  printf("\n> leave-group [GROUP_NAME]");
  printf("\n> find-groups [SEARCH_STRING]");
  printf("\n> send-message [GROUP_NAME] \"[MESSAGE]\"");
  printf("\n> init-poll [GROUP_NAME] \"[QUESTION]\" [OPTION_COUNT] \"[OPTION_1]\" \"[OPTION_2]\"...");
  printf("\n> exit\n");
}

void errExit(char *err)
{
  perror(err);
  exit(EXIT_FAILURE);
}