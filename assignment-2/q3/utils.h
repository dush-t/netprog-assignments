#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define GROUP_NAME_LEN 30
#define IP_LEN 20
#define MESSAGE_LEN 100
#define NUM_OPTIONS 5
#define COMMAND_LEN 1024

#define CREATE_GROUP_STR "create-group"
#define JOIN_GROUP_STR "join-group"
#define LEAVE_GROUP_STR "leave-group"
#define FIND_GROUPS_STR "find-groups"
#define SEND_MESSAGE_STR "send-message"
#define INIT_POLL_STR "init-poll"
#define EXIT_CMD_STR "exit\n"

enum cmds
{
  CREATE_GROUP,
  JOIN_GROUP,
  LEAVE_GROUP,
  FIND_GROUPS,
  SEND_MESSAGE,
  INIT_POLL,
  EXIT_CMD
};

struct create_grp_cmd
{
  char grp_name[GROUP_NAME_LEN];
  char ip[IP_LEN];
  int port;
};

struct join_grp_cmd
{
  char ip[IP_LEN];
  int port;
};

struct leave_grp_cmd
{
  char grp_name[GROUP_NAME_LEN];
};

struct find_grp_cmd
{
  char query[GROUP_NAME_LEN];
};

struct send_msg_cmd
{
  char grp_name[GROUP_NAME_LEN];
  char msg[MESSAGE_LEN];
};

struct init_poll_cmd
{
  char grp_name[GROUP_NAME_LEN];
  char que[MESSAGE_LEN];
  char options[NUM_OPTIONS][MESSAGE_LEN];
  int option_cnt;
};

struct command
{
  enum cmds cmd_name;
  union
  {
    struct create_grp_cmd create_grp_cmd;
    struct join_grp_cmd join_grp_cmd;
    struct leave_grp_cmd leave_grp_cmd;
    struct find_grp_cmd find_grp_cmd;
    struct send_msg_cmd send_msg_cmd;
    struct init_poll_cmd init_poll_cmd;
  } cmd_type;
};

typedef struct multicast_group multicast_group;
struct multicast_group
{
  char name[GROUP_NAME_LEN];
  char ip[IP_LEN];
  int port;
  bool is_admin;
  multicast_group *next;
  multicast_group *prev;
};

struct multicast_groups
{
  struct multicast_group *head;
  int count;
};

void parseCommand(char *cmd, struct command *command_obj);

void printCommand(struct command *command_obj);

#endif