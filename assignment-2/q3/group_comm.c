#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <unistd.h>

#include "utils.h"

void errExit(char *);
void printCommands();
void cleanupAndExit(char *err);
void cleanup();

/* multicast group linked list */
struct multicast_group_list *mc_list = NULL;
/* for parsing command string */
struct command *cmd_obj = NULL;

int main()
{
  printf("\n##### WELCOME TO GROUP CHAT APPLICATION #####\n");
  printCommands(); /* print available commands to user */

  fd_set read_set;
  FD_ZERO(&read_set);
  FD_SET(STDIN_FILENO, &read_set);
  int max_fd = STDIN_FILENO;

  mc_list = (struct multicast_group_list *)calloc(1, sizeof(struct multicast_group_list));
  if (mc_list == NULL)
  {
    cleanupAndExit("calloc()");
  }

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
      getline(&cmd_buf, (size_t *)&cmd_buff_len, stdin); /* get command from stdin */

      cmd_obj = (struct command *)calloc(1, sizeof(struct command));
      if (cmd_obj == NULL)
      {
        free(cmd_buf);
        cleanupAndExit("calloc()");
      }

      /* parse the command */
      parseCommand(cmd_buf, cmd_obj);
      if (cmd_obj == NULL)
      {
        free(cmd_buf);
        printf("Command could not be parsed.\n");
        continue;
      }

      /* free command buffer */
      free(cmd_buf);

      switch (cmd_obj->cmd_name)
      {
      case CREATE_GROUP:
      {
        struct create_grp_cmd cmd = cmd_obj->cmd_type.create_grp_cmd;

        /* create multicast group */
        struct multicast_group *mc_grp = initMulticastGroup(cmd.grp_name, cmd.ip, cmd.port, true);
        if (mc_grp == NULL)
          cleanupAndExit("[initMulticastGroup] Could not create multicast group.");

        /* insert multicast group into linked list */
        if (insertMulticastGroup(mc_list, mc_grp) == -1)
          cleanupAndExit("[insertMulticastGroup] Could not add multicast group to list.");

        /* add receive fd to read set to monitor it */
        FD_SET(mc_grp->recv_fd, &read_set);
        max_fd = max(max_fd, mc_grp->recv_fd);

        printf("\nCreated group %s:%d\n", mc_grp->ip, mc_grp->port);
        break;
      }

      case LIST_GROUPS:
      {
        if (listGroups(mc_list) == -1)
          cleanupAndExit("Error in [listGroups] function");

        break;
      }

      case HELP_CMD:
      {
        printCommands();
        break;
      }

      case EXIT_CMD:
      {
        printf("\nExiting...\n");
        cleanup();
        exit(EXIT_SUCCESS);
      }

      default:
        break;
      }
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
  printf("\n> list-groups");
  printf("\n> send-message [GROUP_NAME] \"[MESSAGE]\"");
  printf("\n> init-poll [GROUP_NAME] \"[QUESTION]\" [OPTION_COUNT] \"[OPTION_1]\" \"[OPTION_2]\"...");
  printf("\n> help");
  printf("\n> exit\n");
}

void errExit(char *err)
{
  perror(err);
  exit(EXIT_FAILURE);
}

void cleanup()
{
  if (mc_list != NULL)
  {
    // TO DO: leave groups and free memory
  }

  if (cmd_obj != NULL)
    free(cmd_obj);
}

void cleanupAndExit(char *err)
{
  cleanup();
  errExit(err);
}