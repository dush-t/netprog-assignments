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
  else if (strcmp(cmd_name, EXIT_CMD_STR) == 0)
  {
    command_obj->cmd_name = EXIT_CMD;
  }
  else
  {
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