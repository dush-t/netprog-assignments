#include "utils.h"

void parseCommand(char *cmd, struct command *command_obj)
{
  memset(command_obj, 0, sizeof(struct command));

  char *cmd_name = strtok(cmd, " ");
  if (strcmp(cmd_name, CREATE_GROUP_STR) == 0)
  {
    command_obj->cmd_name = CREATE_GROUP;
    struct create_grp_cmd create_grp_cmd;
    char *grp_name = strtok(NULL, " ");
    char *ip = strtok(NULL, " ");
    char *port = strtok(NULL, " ");

    if (grp_name == NULL || ip == NULL || port == NULL)
    {
      command_obj = NULL;
      return;
    }
    strcpy(create_grp_cmd.grp_name, grp_name);
    strcpy(create_grp_cmd.ip, ip);
    create_grp_cmd.port = atoi(port);

    command_obj->cmd_type.create_grp_cmd = create_grp_cmd;
  }
  else if (strcmp(cmd_name, JOIN_GROUP_STR) == 0)
  {
    command_obj->cmd_name = JOIN_GROUP;
    struct join_grp_cmd join_grp_cmd;
    char *ip = strtok(NULL, " ");
    char *port = strtok(NULL, " ");

    if (ip == NULL || port == NULL)
    {
      command_obj = NULL;
      return;
    }
    strcpy(join_grp_cmd.ip, ip);
    join_grp_cmd.port = atoi(port);

    command_obj->cmd_type.join_grp_cmd = join_grp_cmd;
  }
  else if (strcmp(cmd_name, LEAVE_GROUP_STR) == 0)
  {
    command_obj->cmd_name = LEAVE_GROUP;
    struct leave_grp_cmd leave_grp_cmd;
    char *grp_name = strtok(NULL, " ");
    if (grp_name == NULL)
    {
      command_obj = NULL;
      return;
    }
    strcpy(leave_grp_cmd.grp_name, grp_name);

    command_obj->cmd_type.leave_grp_cmd = leave_grp_cmd;
  }
  else if (strcmp(cmd_name, FIND_GROUPS_STR) == 0)
  {
    command_obj->cmd_name = FIND_GROUPS;

    struct find_grp_cmd find_grp_cmd;
    char *query = strtok(NULL, " ");
    if (query == NULL)
    {
      command_obj = NULL;
      return;
    }
    strcpy(find_grp_cmd.query, query);

    command_obj->cmd_type.find_grp_cmd = find_grp_cmd;
  }
  else if (strcmp(cmd_name, SEND_MESSAGE_STR) == 0)
  {
    command_obj->cmd_name = SEND_MESSAGE;

    struct send_msg_cmd send_msg_cmd;
    char *grp_name = strtok(NULL, " ");
    char *msg = strtok(NULL, "\"");
    if (grp_name == NULL || msg == NULL)
    {
      command_obj = NULL;
      return;
    }

    strcpy(send_msg_cmd.grp_name, grp_name);
    strcpy(send_msg_cmd.msg, msg);

    command_obj->cmd_type.send_msg_cmd = send_msg_cmd;
  }
  else if (strcmp(cmd_name, INIT_POLL_STR) == 0)
  {
    command_obj->cmd_name = INIT_POLL;

    struct init_poll_cmd init_poll_cmd;
    char *grp_name = strtok(NULL, " ");
    char *que = strtok(NULL, "\"");
    char *option_cnt = strtok(NULL, " ");
    printf("\ngrp_name: %s, que: %s, option_cnt: %s\n", grp_name, que, option_cnt);
    if (grp_name == NULL || que == NULL || option_cnt == NULL)
    {
      command_obj = NULL;
      return;
    }
    int cnt = atoi(option_cnt);
    init_poll_cmd.option_cnt = cnt;
    strcpy(init_poll_cmd.grp_name, grp_name);
    strcpy(init_poll_cmd.que, que);

    for (int i = 0; i < cnt; i++)
    {
      char *opt = strtok(NULL, "\"");
      strtok(NULL, "\"");

      if (opt == NULL)
      {
        command_obj = NULL;
        return;
      }
      strcpy(init_poll_cmd.options[i], opt);
    }

    command_obj->cmd_type.init_poll_cmd = init_poll_cmd;
  }
  else if (strcmp(cmd_name, LIST_GROUPS_STR) == 0)
  {
    command_obj->cmd_name = LIST_GROUPS;
  }
  else if (strcmp(cmd_name, HELP_CMD_STR) == 0)
  {
    command_obj->cmd_name = HELP_CMD;
  }
  else if (strcmp(cmd_name, EXIT_CMD_STR) == 0)
  {
    command_obj->cmd_name = EXIT_CMD;
  }
  else
  {
    // TO DO
  }
}

void printCommand(struct command *command_obj)
{
  if (command_obj == NULL)
  {
    printf("Command not recognized.\n");
    return;
  }

  switch (command_obj->cmd_name)
  {
  case CREATE_GROUP:
  {
    struct create_grp_cmd cmd = command_obj->cmd_type.create_grp_cmd;
    printf("create-group %s %s %d\n", cmd.grp_name, cmd.ip, cmd.port);
    break;
  }

  case JOIN_GROUP:
  {
    struct join_grp_cmd cmd = command_obj->cmd_type.join_grp_cmd;
    printf("join-group %s %d\n", cmd.ip, cmd.port);
    break;
  }

  case LEAVE_GROUP:
  {
    struct leave_grp_cmd cmd = command_obj->cmd_type.leave_grp_cmd;
    printf("leave-group %s\n", cmd.grp_name);
    break;
  }

  case FIND_GROUPS:
  {
    struct find_grp_cmd cmd = command_obj->cmd_type.find_grp_cmd;
    printf("find-groups %s\n", cmd.query);
    break;
  }

  case SEND_MESSAGE:
  {
    struct send_msg_cmd cmd = command_obj->cmd_type.send_msg_cmd;
    printf("sned-message %s %s\n", cmd.grp_name, cmd.msg);
    break;
  }

  case INIT_POLL:
  {
    struct init_poll_cmd cmd = command_obj->cmd_type.init_poll_cmd;
    printf("init-poll %s \"%s\" %d", cmd.grp_name, cmd.que, cmd.option_cnt);
    for (int i = 0; i < cmd.option_cnt; i++)
    {
      printf(" \"%s\"", cmd.options[i]);
    }
    printf("\n");
    break;
  }

  case LIST_GROUPS:
  {
    printf("list-groups\n");
    break;
  }

  case HELP_CMD:
  {
    printf("help\n");
    break;
  }

  case EXIT_CMD:
  {
    printf("exit\n");
    break;
  }

  default:
    printf("Command not recognized.\n");
    break;
  }
}

struct multicast_group *initMulticastGroup(char *name, char *ip, int port, bool is_admin)
{
  // TO DO: check that group does not already exist
  struct multicast_group *grp = (struct multicast_group *)calloc(1, sizeof(struct multicast_group));
  if (grp == NULL)
  {
    perror("[initMulticastGroup] calloc");
    return NULL;
  }

  memset(grp, 0, sizeof(struct multicast_group));

  strcpy(grp->name, name);
  strcpy(grp->ip, ip);
  grp->port = port;
  grp->is_admin = is_admin;

  grp->next = NULL;
  grp->prev = NULL;

  struct sockaddr_in send_addr, recv_addr;
  memset(&send_addr, 0, sizeof(send_addr));
  memset(&recv_addr, 0, sizeof(recv_addr));
  int send_fd, recv_fd;

  send_addr.sin_family = AF_INET;
  send_addr.sin_addr.s_addr = inet_addr(ip);
  send_addr.sin_port = htons(port);

  recv_addr.sin_family = AF_INET;
  recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  recv_addr.sin_port = htons(port);

  recv_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (recv_fd == -1)
  {
    perror("[initMulticastGroup] socket()");
    return NULL;
  }

  int ok = 1;
  if (setsockopt(recv_fd, SOL_SOCKET, SO_REUSEADDR, &ok, sizeof(ok)) < 0)
  {
    perror("[initMulticastGroup] setsockopt()");
    return NULL;
  }

  if (bind(recv_fd, (struct sockaddr *)&recv_addr, (socklen_t)sizeof(recv_addr)) == -1)
  {
    perror("[initMulticastGroup] bind()");
    return NULL;
  }

  send_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (send_fd == -1)
  {
    perror("[initMulticastGroup] socket()");
    return NULL;
  }

  // TO DO: multicast loop option??

  grp->recv_addr = recv_addr;
  grp->recv_fd = recv_fd;
  grp->send_addr = send_addr;
  grp->send_fd = send_fd;

  return grp;
}

int insertMulticastGroup(struct multicast_group_list *list, struct multicast_group *group)
{
  if (list == NULL || group == NULL)
  {
    return -1;
  }
  if (list->count == 0)
  {
    list->head = group;
    list->tail = group;
  }
  else
  {
    list->tail->next = group;
    group->prev = list->tail;
    list->tail = group;
  }
  list->count += 1;
  return 0;
}

int removeMulticastGroup(struct multicast_group_list *list, struct multicast_group *group)
{
  if (list == NULL || group == NULL)
  {
    return -1;
  }

  struct multicast_group *prev = NULL;
  struct multicast_group *curr = list->head;
  int count = list->count;

  while (count--)
  {
    if (memcmp(curr, group, sizeof(multicast_group)) == 0)
      break;

    prev = curr;
    curr = curr->next;
  }

  if (curr == NULL)
  {
    return -1;
  }

  if (prev == NULL)
  {
    list->head = curr->next;
    if (curr->next)
      curr->next->prev = NULL;
  }
  else
  {
    prev->next = curr->next;
    if (curr->next)
      curr->next->prev = prev;
  }

  list->count -= 1;
  return 0;
}

int listGroups(struct multicast_group_list *list)
{
  if (list == NULL)
    return -1;

  if (list->count == 0)
  {
    printf("\nConnected to 0 groups.\n");
    return 0;
  }

  printf("\nConnected to %d group(s).\n", list->count);
  struct multicast_group *curr = list->head;
  for (int i = 0; i < list->count; i++)
  {
    printf("%d. %s: %s %d\n", i + 1, curr->name, curr->ip, curr->port);
  }

  return 0;
}

int max(int num1, int num2)
{
  return (num1 > num2) ? num1 : num2;
}