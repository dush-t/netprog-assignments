#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <unistd.h>

#include "utils.h"

#define BROADCAST_PORT 8080

void errExit(char *);
void printCommands();
void cleanupAndExit(char *err);
void cleanup();
int getMaxFd();
void sigIntHandler(int sig_num);

/* multicast group linked list */
struct multicast_group_list *mc_list = NULL;
/* for parsing command string */
struct command *cmd_obj = NULL;
/* fd for receiving broadcast msgs */
int broadcast_fd = -1;

int main()
{
  /* cleanup on exit through SIGINT */
  signal(SIGINT, sigIntHandler);

  printf("\n##### WELCOME TO GROUP CHAT APPLICATION #####\n");
  printCommands(); /* print available commands to user */

  /* setup broadcast fd */
  broadcast_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (broadcast_fd == -1)
    cleanupAndExit("socket()");
  struct sockaddr_in broadcast_addr;
  memset(&broadcast_addr, 0, sizeof(broadcast_addr));
  broadcast_addr.sin_family = AF_INET;
  broadcast_addr.sin_port = htons(BROADCAST_PORT);
  broadcast_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(broadcast_fd, (struct sockaddr *)&broadcast_addr, (socklen_t)sizeof(broadcast_addr)) == -1)
    cleanupAndExit("bind()");

  mc_list = (struct multicast_group_list *)calloc(1, sizeof(struct multicast_group_list));
  if (mc_list == NULL)
  {
    cleanupAndExit("calloc()");
  }

  for (;;)
  {
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(STDIN_FILENO, &read_set);
    FD_SET(broadcast_fd, &read_set);
    int max_fd = getMaxFd();

    int nready = select(max_fd + 1, &read_set, NULL, NULL, NULL);
    if (nready == -1)
    {
      // TO DO
    }
    if (FD_ISSET(broadcast_fd, &read_set))
    {
      /* received broadcast message */
      char msg[PACKET_SIZE];
      struct sockaddr_in caddr;
      memset(&caddr, 0, sizeof(caddr));
      int clen = sizeof(caddr);

      int n = recvfrom(broadcast_fd, msg, PACKET_SIZE, 0, (struct sockaddr *)&caddr, (socklen_t *)&clen);
      if (n < 0)
      {
        cleanupAndExit("recvfrom()");
      }

      printf(">> Received broadcast from %s:%d\n\n", inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));
    }
    if (FD_ISSET(STDIN_FILENO, &read_set))
    {
      char *cmd_buf = (char *)calloc(COMMAND_LEN, sizeof(char));
      int cmd_buff_len = COMMAND_LEN;
      getline(&cmd_buf, (size_t *)&cmd_buff_len, stdin); /* get command from stdin */

      /* parse the command */
      cmd_obj = parseCommand(cmd_buf);
      if (cmd_obj == NULL)
      {
        free(cmd_buf);
        printf(">> Error: Command could not be parsed.\n\n");
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
        {
          printf(">> Error: [initMulticastGroup] Could not create multicast group.\n\n");
          break;
        }

        /* insert multicast group into linked list */
        if (joinMulticastGroup(mc_grp, mc_list) == -1)
        {
          printf(">> Error: [joinMulticastGroup] Could not join multicast group.\n\n");
          break;
        }

        /* add receive fd to read set to monitor it */
        FD_SET(mc_grp->recv_fd, &read_set);
        max_fd = max(max_fd, mc_grp->recv_fd);

        printf(">> Created group %s at %s:%d\n\n", mc_grp->name, mc_grp->ip, mc_grp->port);
        break;
      }

      case LEAVE_GROUP:
      {
        struct leave_grp_cmd cmd = cmd_obj->cmd_type.leave_grp_cmd;
        struct multicast_group *grp = findGroupByName(cmd.grp_name, mc_list);
        if (grp == NULL)
        {
          printf(">> Error: Group %s not joined.\n\n", cmd.grp_name);
          break;
        }
        if (leaveMulticastGroup(grp, mc_list) == -1)
        {
          printf(">> Error: Could not leave group %s.\n\n", cmd.grp_name);
          break;
        }
        printf(">> Left group %s.\n\n", cmd.grp_name);
        break;
      }

      case LIST_GROUPS:
      {
        if (listGroups(mc_list) == -1)
        {
          printf(">> Error: [listGroups] function\n\n");
          break;
        }

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

  cleanup();
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

int getMaxFd()
{
  int max_fd = STDIN_FILENO;
  if (broadcast_fd > max_fd)
    max_fd = broadcast_fd;

  if (mc_list != NULL && mc_list->count > 0)
  {
    struct multicast_group *curr = mc_list->head;
    while (curr != NULL)
    {
      if (curr->recv_fd > max_fd)
        max_fd = curr->recv_fd;
      curr = curr->next;
    }
  }

  return max_fd;
}

void sigIntHandler(int sig_num)
{
  if (sig_num != SIGINT)
    cleanupAndExit("sigIntHandler()");

  printf("\nExiting...\n");
  cleanup();
  exit(EXIT_SUCCESS);
}