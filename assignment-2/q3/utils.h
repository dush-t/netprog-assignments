#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

#define GROUP_NAME_LEN 20
#define IP_LEN 20
#define MESSAGE_LEN 100
#define NUM_OPTIONS 5
#define COMMAND_LEN 1024
#define MAX_GROUPS 5
#define PACKET_SIZE 1024
#define FIND_GROUP_TIMEOUT 5

#define BROADCAST_REQ_PORT 8000
#define BROADCAST_REPLY_PORT 8001

#define CREATE_GROUP_STR "create-group"
#define JOIN_GROUP_STR "join-group"
#define LEAVE_GROUP_STR "leave-group"
#define FIND_GROUP_STR "find-group"
#define SEND_MESSAGE_STR "send-message"
#define INIT_POLL_STR "init-poll"
#define LIST_GROUPS_STR "list-groups"
#define EXIT_CMD_STR "exit"
#define HELP_CMD_STR "help"

enum cmds
{
  CREATE_GROUP,
  JOIN_GROUP,
  LEAVE_GROUP,
  FIND_GROUP,
  SEND_MESSAGE,
  INIT_POLL,
  LIST_GROUPS,
  HELP_CMD,
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
  int send_fd;
  struct sockaddr_in send_addr;
  int recv_fd;
  struct sockaddr_in recv_addr;
};

struct multicast_group_list
{
  struct multicast_group *head;
  struct multicast_group *tail;
  int count;
};

enum message_type
{
  SIMPLE_MSG,
  FIND_GROUP_REQ,
  FIND_GROUP_REPLY,
  POLL_REQ,
  POLL_REPLY
};

struct simple_msg
{
  char msg[MESSAGE_LEN];
};
struct find_grp_req
{
  char query[GROUP_NAME_LEN];
};

struct find_grp_reply
{
  char grp_name[GROUP_NAME_LEN];
  char ip[IP_LEN];
  int port;
};

struct message
{
  enum message_type msg_type;
  union
  {
    struct simple_msg simple_msg;
    struct find_grp_req find_grp_req;
    struct find_grp_reply find_grp_reply;
  } payload;
};

struct command *parseCommand(char *raw_cmd);

void printCommand(struct command *command_obj);

struct multicast_group *initMulticastGroup(char *name, char *ip, int port, bool is_admin);

int insertMulticastGroup(struct multicast_group_list *list, struct multicast_group *group);

int removeMulticastGroup(struct multicast_group_list *list, struct multicast_group *group);

int listGroups(struct multicast_group_list *list);

int max(int num1, int num2);

char *serialize(struct message *, int *);

struct message *deserialize(char *);

int joinMulticastGroup(struct multicast_group *, struct multicast_group_list *);

int leaveMulticastGroup(struct multicast_group *, struct multicast_group_list *);

struct multicast_group *findGroupByName(char *grp_name, struct multicast_group_list *mc_list);

int findGroupSend(struct message *msg);

struct message *findGroupRecv();

int handleFindGroupReq(struct message *parsed_msg, struct multicast_group_list *mc_list, int broadcast_fd, struct sockaddr_in caddr);

#endif