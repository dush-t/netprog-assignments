#include "utils.h"

struct command *parseCommand(char *raw_cmd)
{
  struct command *command_obj = (struct command *)calloc(1, sizeof(struct command));
  if (command_obj == NULL)
  {
    perror("calloc()");
    return NULL;
  }
  memset(command_obj, 0, sizeof(struct command));

  /* copy command and remove last character (\n) */
  char cmd[COMMAND_LEN];
  memset(&cmd, '\0', COMMAND_LEN);

  int cmd_len = strlen(raw_cmd);
  if (raw_cmd[cmd_len - 1] == '\n')
    cmd_len--;
  if (cmd_len == 0)
  {
    free(command_obj);
    return NULL;
  }
  if (raw_cmd[0] == '\n')
    strncpy(cmd, raw_cmd + 1, cmd_len - 1);
  else
    strncpy(cmd, raw_cmd, cmd_len);

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
      free(command_obj);
      return NULL;
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
    char *group_name = strtok(NULL, " ");

    if (group_name == NULL)
    {
      free(command_obj);
      return NULL;
    }
    strcpy(join_grp_cmd.grp_name, group_name);

    command_obj->cmd_type.join_grp_cmd = join_grp_cmd;
  }
  else if (strcmp(cmd_name, LEAVE_GROUP_STR) == 0)
  {
    command_obj->cmd_name = LEAVE_GROUP;
    struct leave_grp_cmd leave_grp_cmd;
    char *grp_name = strtok(NULL, " ");
    if (grp_name == NULL)
    {
      free(command_obj);
      return NULL;
    }
    strcpy(leave_grp_cmd.grp_name, grp_name);

    command_obj->cmd_type.leave_grp_cmd = leave_grp_cmd;
  }
  else if (strcmp(cmd_name, FIND_GROUP_STR) == 0)
  {
    command_obj->cmd_name = FIND_GROUP;

    struct find_grp_cmd find_grp_cmd;
    char *query = strtok(NULL, " ");
    if (query == NULL)
    {
      free(command_obj);
      return NULL;
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
      free(command_obj);
      return NULL;
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

    if (grp_name == NULL || que == NULL || option_cnt == NULL)
    {
      free(command_obj);
      return NULL;
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
        free(command_obj);
        return NULL;
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
  else if (strcmp(cmd_name, LIST_FILES_STR) == 0)
  {
    command_obj->cmd_name = LIST_FILES;
  }
  else if (strcmp(cmd_name, REQUEST_FILE_STR) == 0)
  {
    command_obj->cmd_name = REQUEST_FILE;
    char *file_name = strtok(NULL, " ");
    if (file_name == NULL)
    {
      free(command_obj);
      return NULL;
    }

    if (strlen(file_name) > FILE_NAME_LEN)
    {
      perror("File name length is greater than allowed.");
      free(command_obj);
      return NULL;
    }

    strcpy(command_obj->cmd_type.request_file_cmd.file_name, file_name);
  }
  else
  {
    free(command_obj);
    return NULL;
  }

  return command_obj;
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
    printf("join-group %s\n", cmd.grp_name);
    break;
  }

  case LEAVE_GROUP:
  {
    struct leave_grp_cmd cmd = command_obj->cmd_type.leave_grp_cmd;
    printf("leave-group %s\n", cmd.grp_name);
    break;
  }

  case FIND_GROUP:
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

  case LIST_FILES:
  {
    printf("list-files\n");
    break;
  }

  case REQUEST_FILE:
  {
    printf("request-file %s\n", command_obj->cmd_type.request_file_cmd.file_name);
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

/*
* Create sockets and fds for a new multicast group
* err = -1 if name exists
* err = -2 is IP:Port exists
*/
struct multicast_group *initMulticastGroup(char *name, char *ip, int port, struct multicast_group_list *mc_list, int *err)
{
  if (findGroupByName(name, mc_list) != NULL)
  {
    *err = -1;
    return NULL;
  }

  if (findGroupByIpAndPort(ip, port, mc_list) != NULL)
  {
    *err = -2;
    return NULL;
  }

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
    close(recv_fd);
    perror("[initMulticastGroup] setsockopt() SO_REUSEADDR");
    return NULL;
  }

  if (bind(recv_fd, (struct sockaddr *)&recv_addr, (socklen_t)sizeof(recv_addr)) == -1)
  {
    close(recv_fd);
    perror("[initMulticastGroup] bind()");
    return NULL;
  }

  send_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (send_fd == -1)
  {
    close(recv_fd);
    perror("[initMulticastGroup] socket()");
    return NULL;
  }

  int not_ok = 0;
  if (setsockopt(send_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &not_ok, sizeof(not_ok)) == -1)
  {
    close(recv_fd);
    close(send_fd);
    perror("[initMulticastGroup] setsockopt() IP_MULTICAST_LOOP");
    return NULL;
  }

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
    printf("\n>> Connected to 0 groups.\n\n");
    return 0;
  }

  printf("\n>> Connected to %d group(s).\n", list->count);
  struct multicast_group *curr = list->head;
  for (int i = 0; i < list->count; i++)
  {
    printf("%d. %s: %s %d\n", i + 1, curr->name, curr->ip, curr->port);
    curr = curr->next;
  }

  printf("\n");
  return 0;
}

int max(int num1, int num2)
{
  return (num1 > num2) ? num1 : num2;
}

char *serialize(struct message *msg, int *len)
{
  char *res = (char *)calloc(PACKET_SIZE, sizeof(char));
  if (res == NULL)
  {
    perror("calloc");
    return NULL;
  }
  memset(res, '\0', PACKET_SIZE);

  int offset = 0;
  memcpy(res + offset, &(msg->msg_type), sizeof(enum message_type));
  offset += sizeof(enum message_type);

  switch (msg->msg_type)
  {
  case SIMPLE_MSG:
  {
    int sz = sizeof(struct simple_msg);
    memcpy(res + offset, &(msg->payload.simple_msg), sz);
    offset += sz;
    break;
  }

  case FIND_GROUP_REQ:
  {
    int sz = sizeof(struct find_grp_req);
    memcpy(res + offset, &(msg->payload.find_grp_req), sz);
    offset += sz;
    break;
  }

  case FIND_GROUP_REPLY:
  {
    int sz = sizeof(struct find_grp_reply);
    memcpy(res + offset, &(msg->payload.find_grp_reply), sz);
    offset += sz;
    break;
  }

  case POLL_REQ:
  {
    int sz = sizeof(struct poll_req);
    memcpy(res + offset, &(msg->payload.poll_req), sz);
    offset += sz;
    break;
  }

  case POLL_REPLY:
  {
    int sz = sizeof(struct poll_reply);
    memcpy(res + offset, &(msg->payload.poll_reply), sz);
    offset += sz;
    break;
  }

  case FILE_REQ:
  {
    int sz = sizeof(struct file_req);
    memcpy(res + offset, &(msg->payload.file_req), sz);
    offset += sz;
    break;
  }

  case FILE_LIST_BROADCAST:
  {
    int sz = sizeof(struct file_list_broadcast);
    memcpy(res + offset, &(msg->payload.file_list_broadcast), sz);
    offset += sz;
    break;
  }

  default:
    perror("Message type not recognized.");
    return NULL;
    break;
  }

  *len = offset;
  return res;
}

struct message *deserialize(char *msg)
{
  struct message *msg_obj = (struct message *)calloc(1, sizeof(struct message));

  int msg_type;
  int offset = 0;
  memcpy(&msg_type, msg + offset, sizeof(int));
  offset += sizeof(int);

  msg_obj->msg_type = msg_type;

  switch (msg_type)
  {
  case SIMPLE_MSG:
  {
    memcpy(&(msg_obj->payload.simple_msg), msg + offset, sizeof(struct simple_msg));
    break;
  }

  case FIND_GROUP_REQ:
  {
    memcpy(&(msg_obj->payload.find_grp_req), msg + offset, sizeof(struct find_grp_req));
    break;
  }

  case FIND_GROUP_REPLY:
  {
    memcpy(&(msg_obj->payload.find_grp_reply), msg + offset, sizeof(struct find_grp_reply));
    break;
  }

  case POLL_REQ:
  {
    memcpy(&(msg_obj->payload.poll_req), msg + offset, sizeof(struct poll_req));
    break;
  }

  case POLL_REPLY:
  {
    memcpy(&(msg_obj->payload.poll_reply), msg + offset, sizeof(struct poll_reply));
    break;
  }

  case FILE_REQ:
  {
    memcpy(&(msg_obj->payload.file_req), msg + offset, sizeof(struct file_req));
    break;
  }

  case FILE_LIST_BROADCAST:
  {
    memcpy(&(msg_obj->payload.file_list_broadcast), msg + offset, sizeof(struct file_list_broadcast));
    break;
  }

  default:
    perror("Message type not recognized.");
    return NULL;
    break;
  }

  return msg_obj;
}

int joinMulticastGroup(struct multicast_group *grp, struct multicast_group_list *mc_list)
{
  struct ip_mreq mreq;
  mreq.imr_multiaddr.s_addr = inet_addr(grp->ip);
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);

  if (setsockopt(grp->recv_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == -1)
  {
    perror("setsockopt()");
    return -1;
  }

  if (insertMulticastGroup(mc_list, grp) == -1)
  {
    perror("insertMulticastGroup()");
    return -1;
  }

  return 0;
}

int leaveMulticastGroup(struct multicast_group *grp, struct multicast_group_list *mc_list)
{
  struct ip_mreq mreq;
  mreq.imr_multiaddr.s_addr = inet_addr(grp->ip);
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);

  if (setsockopt(grp->recv_fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) == -1)
  {
    perror("setsockopt()");
    return -1;
  }

  if (removeMulticastGroup(mc_list, grp) == -1)
  {
    perror("removeMulticastGroup()");
    return -1;
  }

  close(grp->send_fd);
  close(grp->recv_fd);
  free(grp);
  return 0;
}

struct multicast_group *findGroupByName(char *grp_name, struct multicast_group_list *mc_list)
{
  if (mc_list == NULL)
  {
    return NULL;
  }

  struct multicast_group *curr = mc_list->head;
  while (curr)
  {
    if (strcmp(grp_name, curr->name) == 0)
    {
      return curr;
    }
    curr = curr->next;
  }

  return NULL;
}

struct multicast_group *findGroupByIpAndPort(char *ip, int port, struct multicast_group_list *mc_list)
{
  if (mc_list == NULL)
  {
    return NULL;
  }

  struct multicast_group *curr = mc_list->head;
  while (curr)
  {
    if (strcmp(ip, curr->ip) == 0 && port == curr->port)
    {
      return curr;
    }
    curr = curr->next;
  }

  return NULL;
}

int findGroupSend(struct message *msg)
{
  int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (fd == -1)
  {
    perror("socket()");
    return -1;
  }

  /* Set socket to allow broadcast */
  int broadcast_perm = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (void *)&broadcast_perm, sizeof(broadcast_perm)) < 0)
  {
    perror("setsockopt()");
    return -1;
  }

  /* Construct local address structure */
  struct sockaddr_in baddr;
  memset(&baddr, 0, sizeof(baddr));                /* Zero out structure */
  baddr.sin_family = AF_INET;                      /* Internet address family */
  baddr.sin_addr.s_addr = htonl(INADDR_BROADCAST); /* Broadcast IP address */
  baddr.sin_port = htons(BROADCAST_REQ_PORT);      /* Broadcast port */

  int buff_len;
  char *buff = serialize(msg, &buff_len);
  if (buff == NULL)
  {
    perror("serialize()");
    return -1;
  }

  if (sendto(fd, buff, buff_len, 0, (struct sockaddr *)&baddr, (socklen_t)sizeof(baddr)) == -1)
  {
    free(buff);
    perror("sendto()");
    return -1;
  }

  free(buff);
  close(fd);
  return 0;
}

struct message *findGroupRecv(int recv_fd, char *grp_name)
{
  char buff[PACKET_SIZE];
  struct message *recv_msg = NULL;

  printf("\n>> Finding group, please wait for about %d seconds...\n\n", FIND_GROUP_TIMEOUT);

  fd_set find_grp_set;

  /* 
  * Find group blocks on recvfrom() so it ignores all other 
  * messages received while finding a group. This can be improved.
  * */
  for (;;)
  {
    FD_ZERO(&find_grp_set);
    FD_SET(recv_fd, &find_grp_set);

    struct timeval tv;
    tv.tv_sec = FIND_GROUP_TIMEOUT;
    tv.tv_usec = 0;

    int nready = select(recv_fd + 1, &find_grp_set, NULL, NULL, &tv);
    if (nready == 0)
    {
      /* Time expired */
      return NULL;
    }

    if (FD_ISSET(recv_fd, &find_grp_set))
    {
      struct sockaddr_in caddr;
      int len = sizeof(caddr);

      int n = recvfrom(recv_fd, buff, PACKET_SIZE, 0, (struct sockaddr *)&caddr, (socklen_t *)&len);

      recv_msg = deserialize(buff);

      /* ensure that this is a reply for FIND_GROUP and the group name is same */
      if (recv_msg->msg_type != FIND_GROUP_REPLY || strcmp(recv_msg->payload.find_grp_reply.grp_name, grp_name) != 0)
      {
        free(recv_msg);
        recv_msg = NULL;
      }
      else
      {
        return recv_msg;
      }
    }
  }

  return recv_msg;
}

int handleFindGroupReq(struct message *parsed_msg, struct multicast_group_list *mc_list, int broadcast_fd, struct sockaddr_in caddr)
{
  struct multicast_group *grp = findGroupByName(parsed_msg->payload.find_grp_req.query, mc_list);
  if (grp != NULL)
  {
    struct message msg_reply;
    msg_reply.msg_type = FIND_GROUP_REPLY;
    strcpy(msg_reply.payload.find_grp_reply.grp_name, grp->name);
    strcpy(msg_reply.payload.find_grp_reply.ip, grp->ip);
    msg_reply.payload.find_grp_reply.port = grp->port;

    int reply_len;
    char *reply_buff = serialize(&msg_reply, &reply_len);

    if (reply_buff == NULL)
    {
      perror("serialize()");
      return -1;
    }

    /* send group info */
    int clen = sizeof(caddr);
    caddr.sin_port = htons(parsed_msg->payload.find_grp_req.reply_port);

    /* ignore sendto() error */
    sendto(broadcast_fd, reply_buff, reply_len, 0, (struct sockaddr *)&caddr, (socklen_t)clen);

    free(reply_buff);
  }

  return 0;
}

int sendSimpleMessage(struct multicast_group *grp, char *msg)
{
  struct message pkt;
  pkt.msg_type = SIMPLE_MSG;
  strcpy(pkt.payload.simple_msg.msg, msg);

  int buff_len;
  char *buff = serialize(&pkt, &buff_len);
  if (buff == NULL)
  {
    perror("serialize()");
    return -1;
  }

  int res = sendto(grp->send_fd, buff, buff_len, 0, (struct sockaddr *)&(grp->send_addr), (socklen_t)sizeof(grp->send_addr));

  free(buff);
  return res;
}

/*
  return value = 0 --> success 
                -1 --> error while sending
                -2 --> error while receiving
*/
int handlePollCommand(struct multicast_group *grp, struct poll_req poll_req)
{
  if (grp == NULL)
  {
    return -1;
  }

  /* create socket for receiving responses */
  int reply_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (reply_fd == -1)
  {
    perror("socket()");
    return -2;
  }
  struct sockaddr_in saddr;
  memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family = AF_INET;
  saddr.sin_port = 0;
  saddr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(reply_fd, (struct sockaddr *)&saddr, (socklen_t)sizeof(saddr)) == -1)
  {
    close(reply_fd);
    perror("bind()");
    return -2;
  }
  int slen = sizeof(saddr);
  if (getsockname(reply_fd, (struct sockaddr *)&saddr, (socklen_t *)&slen) == -1)
  {
    close(reply_fd);
    perror("getsockname()");
    return -2;
  }
  poll_req.reply_port = ntohs(saddr.sin_port);

  struct message pkt;
  pkt.msg_type = POLL_REQ;
  pkt.payload.poll_req = poll_req;

  int buff_len;
  char *buff = serialize(&pkt, &buff_len);
  if (buff == NULL)
  {
    perror("serialize()");
    return -1;
  }

  /* Send poll message to group */
  if (sendto(grp->send_fd, buff, buff_len, 0, (struct sockaddr *)&(grp->send_addr), (socklen_t)sizeof(grp->send_addr)) == -1)
  {
    close(reply_fd);
    perror("sendto()");
    free(buff);
    return -1;
  }

  /* Receive poll reply from peers */
  printf("\n>> Getting replies, please wait for about %ds...\n\n", POLL_TIMEOUT);

  /* create a new process as recvfrom() is blocking and we want the user itself to reply to poll message if recvd */
  if (fork() == 0)
  {
    fd_set monitor_fd;
    int option_cnt[NUM_OPTIONS] = {0};

    for (;;)
    {
      FD_ZERO(&monitor_fd);
      FD_SET(reply_fd, &monitor_fd);

      struct timeval tv;
      tv.tv_sec = POLL_TIMEOUT;
      tv.tv_usec = 0;

      int nready = select(reply_fd + 1, &monitor_fd, NULL, NULL, &tv);
      if (nready == -1)
      {
        free(buff);
        return -1;
      }
      if (nready == 0)
      {
        /* Time expired */
        break;
      }

      if (FD_ISSET(reply_fd, &monitor_fd))
      {
        struct sockaddr_in caddr;
        int len = sizeof(caddr);
        memset(buff, '\0', PACKET_SIZE);

        int n = recvfrom(reply_fd, buff, PACKET_SIZE, 0, (struct sockaddr *)&caddr, (socklen_t *)&len);

        struct message *recv_msg = deserialize(buff);

        /* ensure that this is a reply for FIND_GROUP */
        if (recv_msg->msg_type != POLL_REPLY || recv_msg->payload.poll_reply.id != poll_req.id)
        {
          free(recv_msg);
          continue;
        }
        else
        {
          int selected_opt_idx = recv_msg->payload.poll_reply.option;
          printf(">> %s:%d selected option %d. Waiting %ds for other replies...\n", inet_ntoa(caddr.sin_addr), caddr.sin_port, selected_opt_idx + 1, POLL_TIMEOUT);
          option_cnt[selected_opt_idx] += 1;
        }

        free(recv_msg);
      }
    }

    printf("\n>> Final option count: \n");
    for (int i = 0; i < poll_req.option_cnt; i++)
    {
      printf(">> Option %d -> %d\n", i + 1, option_cnt[i]);
    }
    printf("\n");
    free(buff);

    close(reply_fd);
    exit(EXIT_SUCCESS);
  }

  close(reply_fd);
  free(buff);
  return 0;
}

/*
* err_no =  -1 --> error while finding
*           -2 --> error while sending
*           -3 ---> error while receiving
*/
struct message *handleFindGroupCmd(char *grp_name, struct multicast_group_list *mc_list, int *err_no)
{
  /* ensure that the group is not joined already */
  if (findGroupByName(grp_name, mc_list) != NULL)
  {
    *err_no = -1;
    printf(">> Group %s already joined.\n\n", grp_name);
    return NULL;
  }

  int reply_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (reply_fd == -1)
  {
    perror("socket()");
    *err_no = -3;
    return NULL;
  }

  struct sockaddr_in saddr;
  memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family = AF_INET;
  saddr.sin_port = 0;
  saddr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(reply_fd, (struct sockaddr *)&saddr, (socklen_t)sizeof(saddr)) == -1)
  {
    close(reply_fd);
    perror("bind()");
    *err_no = -3;
    return NULL;
  }

  int slen = sizeof(saddr);
  if (getsockname(reply_fd, (struct sockaddr *)&saddr, (socklen_t *)&slen) == -1)
  {
    close(reply_fd);
    perror("getsockname()");
    *err_no = -3;
    return NULL;
  }

  /* send broacast message to find the group with given name */
  struct message req_msg;
  req_msg.msg_type = FIND_GROUP_REQ;
  req_msg.payload.find_grp_req.reply_port = ntohs(saddr.sin_port);
  strcpy(req_msg.payload.find_grp_req.query, grp_name);

  /* Send the broadcast message */
  if (findGroupSend(&req_msg) == -1)
  {
    perror("findGroupSend()");
    *err_no = -2;
    close(reply_fd);
    return NULL;
  }

  /* Receive reply */
  struct message *reply_msg = findGroupRecv(reply_fd, grp_name);
  if (reply_msg == NULL)
  {
    close(reply_fd);
    return NULL;
  }

  close(reply_fd);
  return reply_msg;
}

int requestFileReqHandler(struct file_req file_req, char my_files[][FILE_NAME_LEN], int file_count, struct sockaddr_in caddr)
{
  /* check if the file exists */
  bool found_file = false;
  for (int i = 0; i < file_count; i++)
  {
    if (strcmp(my_files[i], file_req.file_name) == 0)
    {
      found_file = true;
      break;
    }
  }
  if (!found_file)
    return 0;

  char file_path[100];
  strcpy(file_path, FILE_DIR);
  strcat(file_path, file_req.file_name);
  int file_fd = open(file_path, O_RDONLY);
  if (file_fd == -1)
  {
    perror("[requestFileReqHandler] open()");
    return -1;
  }

  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1)
  {
    perror("[requestFileReqHandler] socket()");
    close(file_fd);
    return -1;
  }

  caddr.sin_port = htons(file_req.port);
  /* ignore connect error */
  int cfd = connect(sfd, (struct sockaddr *)&caddr, (socklen_t)sizeof(caddr));
  if (cfd == -1)
    return 0;

  struct message reply;
  reply.msg_type = FILE_REPLY;
  char buff[FILE_CHUNK_LEN];
  int nread;
  for (;;)
  {
    memset(buff, '\0', sizeof(buff));
    nread = read(file_fd, buff, FILE_CHUNK_LEN);
    if (nread == 0)
    {
      reply.payload.file_reply.is_last = true;
      reply.payload.file_reply.len = 0;
      strcpy(reply.payload.file_reply.data, "\0");
    }
    else
    {
      reply.payload.file_reply.len = nread;
      strcpy(reply.payload.file_reply.data, buff);
    }

    send(cfd, &reply, sizeof(reply), 0);
    if (nread == 0)
      break;
  }

  return 0;
}

/*
* Return:
*     -1 -> file name already exists
*     -2 -> error while receiving
*     -3 -> error while sending
*     -4 -> max files reached
*/
int requestFileCmdHandler(char *file_name, char my_files[][FILE_NAME_LEN], int *file_count, struct multicast_group_list *mc_list)
{
  /* ensure that file does not already exist */
  for (int i = 0; i < *file_count; i++)
  {
    if (strcmp(my_files[i], file_name) == 0)
      return -1;
  }

  if (*file_count == MAX_FILE_ALLOWED)
    return -4;

  /* create TCP socket for receiving file response */
  int sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sfd == -1)
  {
    perror("socket()");
    return -2;
  }
  struct sockaddr_in saddr;
  memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family = AF_INET;
  saddr.sin_port = 0;
  saddr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(sfd, (struct sockaddr *)&saddr, (socklen_t)sizeof(saddr)) == -1)
  {
    close(sfd);
    perror("bind()");
    return -2;
  }
  int slen = sizeof(saddr);
  if (getsockname(sfd, (struct sockaddr *)&saddr, (socklen_t *)&slen) == -1)
  {
    close(sfd);
    perror("getsockname()");
    return -2;
  }
  if (listen(sfd, 5) == -1)
  {
    close(sfd);
    perror("listen()");
    return -2;
  }

  struct message msg;
  msg.msg_type = FILE_REQ;
  strcpy(msg.payload.file_req.file_name, file_name);
  msg.payload.file_req.port = ntohs(saddr.sin_port);

  int buff_len;
  char *buff = serialize(&msg, &buff_len);

  /* child listens for TCP requests */
  if (fork() == 0)
  {
    free(buff);

    struct sockaddr_in caddr;
    int clen = sizeof(caddr);
    memset(&caddr, 0, clen);

    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sfd, &fdset);

    struct timeval tv;
    tv.tv_sec = REQ_FILE_TIMEOUT;
    tv.tv_usec = 0;

    int nready = select(sfd + 1, &fdset, NULL, NULL, &tv);
    if (nready == -1)
    {
      close(sfd);
      perror("select() error while receiving file");
      exit(EXIT_FAILURE);
    }
    if (nready == 0)
    {
      /* timed out */
      printf(">> No response for file %s received within %d seconds.\n\n", file_name, REQ_FILE_TIMEOUT);
      exit(EXIT_FAILURE);
    }

    if (FD_ISSET(sfd, &fdset))
    {
      int connfd = accept(sfd, (struct sockaddr *)&caddr, (socklen_t *)&clen);
      char file_path[100];
      strcpy(file_path, FILE_DIR);
      strcat(file_path, file_name);
      int fd = open(file_path, O_WRONLY | O_CREAT);
      if (fd == -1)
      {
        perror("open()");
        exit(EXIT_FAILURE);
      }
      struct file_reply res;
      for (;;)
      {
        int n = read(connfd, &res, sizeof(res));
        if (n < 0)
        {
          perror("read() error");
          exit(EXIT_FAILURE);
        }

        if (write(fd, res.data, res.len) == -1)
        {
          perror("write() error");
          exit(EXIT_FAILURE);
        }

        if (res.is_last)
          break;
      }
    }

    printf(">> Received file %s.\n\n", file_name);
    close(sfd);
    exit(EXIT_SUCCESS);
  }
  /* parent multicasts to all groups */
  else
  {
    close(sfd);
    struct multicast_group *curr_grp = mc_list->head;

    while (curr_grp)
    {
      sendto(curr_grp->send_fd, buff, buff_len, 0, (struct sockaddr *)&(curr_grp->send_addr), (socklen_t)sizeof(struct sockaddr_in));
      curr_grp = curr_grp->next;
    }

    free(buff);

    int status;
    wait(&status);
    if (WIFEXITED(status))
    {
      if (WEXITSTATUS(status) == EXIT_SUCCESS)
      {
        strcpy(my_files[*file_count], file_name);
        *file_count += 1;
        return 0;
      }
      else
      {
        return -2;
      }
    }
    else
    {
      return -2;
    }
  }
}

int getFileList(char files[][FILE_NAME_LEN], int *count)
{
  struct stat st = {0};

  /* create dir if it doesn't exist */
  if (stat(FILE_DIR, &st) == -1)
  {
    if (mkdir(FILE_DIR, 0700) == -1)
    {
      perror("mkdir()");
      return -1;
    }
  }

  DIR *d;
  struct dirent *dir;
  d = opendir(FILE_DIR);
  if (d == NULL)
  {
    perror("opendir");
    return -1;
  }

  while ((dir = readdir(d)) != NULL)
  {
    if (dir->d_type == DT_REG)
    {
      strcpy(files[*count], dir->d_name);
      (*count)++;
    }
  }
  closedir(d);

  return 0;
}