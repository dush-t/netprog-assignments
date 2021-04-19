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
int getMaxFd();
void sigIntHandler(int sig_num);

/* multicast group linked list */
struct multicast_group_list *mc_list = NULL;
/* for parsing command string */
struct command *cmd_obj = NULL;
/* fd for receiving broadcast msgs */
int broadcast_fd = -1;
/* fd for receiving unicast msgs */
int unicast_fd = -1;

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
  broadcast_addr.sin_port = htons(BROADCAST_REQ_PORT);
  broadcast_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(broadcast_fd, (struct sockaddr *)&broadcast_addr, (socklen_t)sizeof(broadcast_addr)) == -1)
    cleanupAndExit("bind()");

  /* setup unicast fd for receiving replies */
  unicast_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (unicast_fd == -1)
    cleanupAndExit("socket()");
  struct sockaddr_in unicast_addr;
  memset(&unicast_addr, 0, sizeof(unicast_addr));
  unicast_addr.sin_family = AF_INET;
  unicast_addr.sin_port = htons(UNICAST_PORT);
  unicast_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(unicast_fd, (struct sockaddr *)&unicast_addr, (socklen_t)sizeof(unicast_addr)) == -1)
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

    /* set fd to monitor */
    FD_SET(STDIN_FILENO, &read_set);
    FD_SET(broadcast_fd, &read_set);
    if (mc_list != NULL && mc_list->count > 0)
    {
      struct multicast_group *curr = mc_list->head;
      while (curr != NULL)
      {
        FD_SET(curr->recv_fd, &read_set);
        curr = curr->next;
      }
    }

    int max_fd = getMaxFd();

    int nready = select(max_fd + 1, &read_set, NULL, NULL, NULL);
    if (nready == -1)
    {
      cleanupAndExit("select()");
    }

    /* check if it is a broadcast message */
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

      struct message *parsed_msg = deserialize(msg);
      if (parsed_msg == NULL)
      {
        printf(">> Error: Could not parse received message.\n\n");
        break;
      }
      if (parsed_msg->msg_type == FIND_GROUP_REQ)
      {
        if (handleFindGroupReq(parsed_msg, mc_list, broadcast_fd, caddr) == -1)
          printf(">> Error: error occurred while handling find group request.\n\n");
      }

      free(parsed_msg);
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
        int init_mc_err = 0;
        struct multicast_group *mc_grp = initMulticastGroup(cmd.grp_name, cmd.ip, cmd.port, mc_list, &init_mc_err);
        if (mc_grp == NULL)
        {
          if (init_mc_err == -1)
            printf(">> Error: Multicast group %s already joined.\n\n", cmd.grp_name);
          else if (init_mc_err == -2)
            printf(">> Error: Multicast group %s:%d already joined.\n\n", cmd.ip, cmd.port);
          else
            printf(">> Error: [initMulticastGroup] Could not create multicast group.\n\n");
          break;
        }

        /* insert multicast group into linked list */
        if (joinMulticastGroup(mc_grp, mc_list) == -1)
        {
          printf(">> Error: [joinMulticastGroup] Could not join multicast group.\n\n");
          break;
        }

        printf(">> Created group %s at %s:%d\n\n", mc_grp->name, mc_grp->ip, mc_grp->port);
        break;
      }

      case JOIN_GROUP:
      {
        struct join_grp_cmd cmd = cmd_obj->cmd_type.join_grp_cmd;
        printf("group name: %s\n", cmd.grp_name);
        /* ensure that the group is not joined already */
        if (findGroupByName(cmd.grp_name, mc_list) != NULL)
        {
          printf(">> Group %s already joined.\n\n", cmd.grp_name);
          break;
        }

        /* send broacast message to find the group with given name */
        struct message req_msg;
        req_msg.msg_type = FIND_GROUP_REQ;
        strcpy(req_msg.payload.find_grp_req.query, cmd.grp_name);

        /* Send the broadcast message */
        if (findGroupSend(&req_msg) == -1)
        {
          cleanupAndExit("findGroupSend()");
        }

        /* Receive reply */
        struct message *reply_msg = findGroupRecv(unicast_fd);
        if (reply_msg == NULL)
        {
          printf(">> Error: No group found with name %s within %d seconds.\n\n", cmd.grp_name, FIND_GROUP_TIMEOUT);
          break;
        }

        struct find_grp_reply find_grp_reply = reply_msg->payload.find_grp_reply;

        /* create multicast group */
        int init_mc_err = 0;
        struct multicast_group *mc_grp = initMulticastGroup(find_grp_reply.grp_name, find_grp_reply.ip, find_grp_reply.port, mc_list, &init_mc_err);
        if (mc_grp == NULL)
        {
          if (init_mc_err == -1)
            printf(">> Error: Multicast group %s already joined.\n\n", find_grp_reply.grp_name);
          else if (init_mc_err == -2)
            printf(">> Error: Multicast group %s:%d already joined.\n\n", find_grp_reply.ip, find_grp_reply.port);
          else
            printf(">> Error: [initMulticastGroup] Could not create multicast group.\n\n");
          break;
        }

        /* insert multicast group into linked list */
        if (joinMulticastGroup(mc_grp, mc_list) == -1)
        {
          printf(">> Error: [joinMulticastGroup] Could not join multicast group.\n\n");
          break;
        }

        printf(">> Joined group %s (%s:%d).\n\n", find_grp_reply.grp_name, find_grp_reply.ip, find_grp_reply.port);
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

      case FIND_GROUP:
      {
        struct find_grp_cmd cmd = cmd_obj->cmd_type.find_grp_cmd;

        /* ensure that already not a part of the group */
        struct multicast_group *grp = NULL;
        if ((grp = findGroupByName(cmd.query, mc_list)) != NULL)
        {
          printf(">> Already joined group %s (%s:%d).\n\n", grp->name, grp->ip, grp->port);
          break;
        }

        /* create message */
        struct message req_msg;
        req_msg.msg_type = FIND_GROUP_REQ;
        strcpy(req_msg.payload.find_grp_req.query, cmd.query);

        /* Send the broadcast message */
        if (findGroupSend(&req_msg) == -1)
        {
          cleanupAndExit("findGroupSend()");
        }

        /* Receive reply */
        struct message *reply_msg = findGroupRecv(unicast_fd);
        if (reply_msg == NULL)
        {
          printf(">> No group found withing %d seconds.\n\n", FIND_GROUP_TIMEOUT);
          break;
        }

        struct find_grp_reply find_grp_reply = reply_msg->payload.find_grp_reply;

        printf(">> Found group %s with address %s:%d\n\n", find_grp_reply.grp_name, find_grp_reply.ip, find_grp_reply.port);

        free(reply_msg);
        break;
      }

      case SEND_MESSAGE:
      {
        struct send_msg_cmd cmd = cmd_obj->cmd_type.send_msg_cmd;

        /* ensure that the group is joined */
        struct multicast_group *grp = NULL;
        if ((grp = findGroupByName(cmd.grp_name, mc_list)) == NULL)
        {
          printf(">> Group %s not joined.\n\n", cmd.grp_name);
          break;
        }

        /* send message to group */
        if (sendSimpleMessage(grp, cmd.msg) == -1)
        {
          printf(">> Error while sending message to group.\n\n");
          break;
        }

        printf(">> Message sent to group %s.\n\n", cmd.grp_name);
        break;
      }

      case INIT_POLL:
      {
        struct init_poll_cmd cmd = cmd_obj->cmd_type.init_poll_cmd;

        /* ensure that the group is joined */
        struct multicast_group *grp = NULL;
        if ((grp = findGroupByName(cmd.grp_name, mc_list)) == NULL)
        {
          printf(">> Group %s not joined.\n\n", cmd.grp_name);
          break;
        }

        struct poll_req poll_req;
        poll_req.id = getpid();
        poll_req.option_cnt = cmd.option_cnt;
        strcpy(poll_req.que, cmd.que);
        for (int i = 0; i < cmd.option_cnt; i++)
          strcpy(poll_req.options[i], cmd.options[i]);

        /* send poll on group */
        int res = handlePollCommand(grp, poll_req, unicast_fd);
        if (res == -1)
        {
          printf(">> Error occurred while sending poll.\n\n");
        }
        else if (res == -2)
        {
          printf(">> Error occurred while receiving poll.\n\n");
        }
        /* else the function would have successfully printed output */

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

    if (mc_list != NULL && mc_list->count > 0)
    {
      struct multicast_group *curr_mc_grp = mc_list->head;

      while (curr_mc_grp)
      {
        if (FD_ISSET(curr_mc_grp->recv_fd, &read_set))
        {
          /* received multicast message */
          char msg[PACKET_SIZE];
          struct sockaddr_in caddr;
          memset(&caddr, 0, sizeof(caddr));
          int clen = sizeof(caddr);

          int n = recvfrom(curr_mc_grp->recv_fd, msg, PACKET_SIZE, 0, (struct sockaddr *)&caddr, (socklen_t *)&clen);
          if (n < 0)
          {
            cleanupAndExit("recvfrom()");
          }

          struct message *parsed_msg = deserialize(msg);
          if (parsed_msg == NULL)
          {
            printf("Error: Could not parse received message.\n\n");
            continue;
          }

          if (parsed_msg->msg_type == SIMPLE_MSG)
          {
            printf(">> Received from %s:%d\n>> %s\n\n", inet_ntoa(caddr.sin_addr), caddr.sin_port, parsed_msg->payload.simple_msg.msg);
          }
          else if (parsed_msg->msg_type == POLL_REQ)
          {
            struct poll_req poll_req = parsed_msg->payload.poll_req;

            /* Print poll */
            printf(">> Please vote for the poll - \n");
            printf(">> %s\n", poll_req.que);
            for (int i = 0; i < poll_req.option_cnt; i++)
            {
              printf(">> %d. %s\n", i + 1, poll_req.options[i]);
            }
            printf("\nEnter option number: ");

            /* Get reply */
            int poll_opt_selected;
            for (;;)
            {
              scanf("%d", &poll_opt_selected);
              if (poll_opt_selected < 1 || poll_opt_selected > poll_req.option_cnt)
              {
                printf("\nEnter valid option number: ");
              }
              else
              {
                break;
              }
            }

            /* send poll reply */
            struct message poll_reply_msg;
            poll_reply_msg.msg_type = POLL_REPLY;
            poll_reply_msg.payload.poll_reply.id = poll_req.id;
            poll_reply_msg.payload.poll_reply.option = poll_opt_selected - 1; // since poll_opt_selected would be 1 based

            int buff_len;
            char *buff = serialize(&poll_reply_msg, &buff_len);
            if (buff == NULL)
            {
              printf(">> Error while sending poll reply.\n\n");
              continue;
            }

            int sfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (sfd == -1)
            {
              perror("socket()");
              free(buff);
              printf(">> Error while sending poll reply.\n\n");
              continue;
            }

            /* ignore sendto() error as the listener might have stopped listening & closed socket */
            caddr.sin_port = htons(UNICAST_PORT);
            clen = sizeof(caddr);
            sendto(sfd, buff, buff_len, 0, (struct sockaddr *)&caddr, (socklen_t)clen);

            close(sfd);
            free(buff);
          }
          free(parsed_msg);
        }
        curr_mc_grp = curr_mc_grp->next;
      }
    }
  }

  cleanup();
}

void printCommands()
{
  printf("\nAvailable commands:");
  printf("\n> create-group [GROUP_NAME] [GROUP_IP] [GROUP_PORT]");
  printf("\n> join-group [GROUP_NAME]");
  printf("\n> leave-group [GROUP_NAME]");
  printf("\n> find-group [GROUP_NAME]");
  printf("\n> list-groups");
  printf("\n> send-message [GROUP_NAME] \"[MESSAGE]\"");
  printf("\n> init-poll [GROUP_NAME] \"[QUESTION]\" [OPTION_COUNT] \"[OPTION_1]\" \"[OPTION_2]\"...");
  printf("\n> help");
  printf("\n> exit\n\n");
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
    struct multicast_group *curr_mc_grp = mc_list->head;

    while (curr_mc_grp)
    {
      leaveMulticastGroup(curr_mc_grp, mc_list);
      curr_mc_grp = curr_mc_grp->next;
    }

    free(mc_list);
  }

  if (cmd_obj != NULL)
    free(cmd_obj);

  if (broadcast_fd != -1)
    close(broadcast_fd);

  if (unicast_fd != -1)
    close(broadcast_fd);
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