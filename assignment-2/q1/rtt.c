#include "utils.h"
#include "queue.h"
#include "hash_map.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#define HASH_MAP_SZ 7919

int v4_fd, v6_fd;
double *rtt_vals;
struct proto **sockets;
int count, remaining_count;
queue *sendQ;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
hash_map *ip_proto_map = NULL;

void *ip4RecvHelper(void *args);
void *ip6RecvHelper(void *args);
void *sendHelper(void *args);
void printRtts();

int main(int argc, char **argv)
{
  if (argc != 2)
  {
    errExit("Usage: ./rtt.out [IP_LIST_FILE_PATH]\n");
  }

  /* Parse IP input file */
  char *ip_list_file = argv[1];
  ip_list_struct *ip_list = parseIpList(argv[1]);
  count = ip_list->count;

  /* Assign memory for storing proto* structures */
  sockets = (struct proto **)calloc(count, sizeof(struct proto *));
  memset(sockets, 0, count * sizeof(struct proto *));
  if (sockets == NULL)
  {
    // TO DO: cleanup
    errExit("calloc error while creating sockets struct\n");
  }

  setuid(getuid());

  /* initialise fd for communication */
  v4_fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  if (v4_fd == -1)
    errExit("v4_fd error.");
  v6_fd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
  if (v6_fd == -1)
    errExit("v6_fd error.");
  // int no = 0;
  // if (setsockopt(v6_fd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&no, sizeof(no)) == -1)
  //   errExit("setsockopt IPV6_V6ONLY error.");

  /* make sockets nonblocking */
  int flags;
  if ((flags = fcntl(v4_fd, F_GETFL)) == -1)
    errExit("fctnl");
  if (fcntl(v4_fd, F_SETFL, flags | O_NONBLOCK) == -1)
    errExit("fcntl");
  if ((flags = fcntl(v6_fd, F_GETFL)) == -1)
    errExit("fctnl");
  if (fcntl(v6_fd, F_SETFL, flags | O_NONBLOCK) == -1)
    errExit("fcntl");

  /* To store rtt values */
  rtt_vals = (double *)calloc(count * 3, sizeof(double));

  /* init hash map */
  ip_proto_map = init_map(HASH_MAP_SZ);

  /* initialise proto structure for each ip */
  bool v4_found = false, v6_found = false;
  for (int i = 0; i < count; i++)
  {
    if ((sockets[i] = initIp(ip_list->ip[i], v4_fd, v6_fd)) == NULL)
    {
      // TO DO: cleanup
      errExit("Error while initialising IP address structure.\n");
    }

    /* add to hash map */
    char *ip = (char *)calloc(25, sizeof(char));
    if (getIpAddrFromProto(sockets[i], ip) == -1)
    {
      // TO DO: cleanup
      errExit("getIpAddr");
    }
    if (insert_into_map(ip_proto_map, ip, sockets[i]) == -1)
    {
      // TO DO cleanup
      errExit("insert_into_map");
    }
  }

  /* IP List no longer needed */
  freeIpList(ip_list);

  /* Initialize send queue and add sockets to it */
  sendQ = initQueue();
  for (int i = 0; i < count; i++)
  {
    qPush(sendQ, sockets[i]);
  }

  struct timeval tv_start, tv_end;
  gettimeofday(&tv_start, NULL);

  remaining_count = count;

  pthread_t recv_thread_v4, recv_thread_v6, send_thread;
  pthread_create(&recv_thread_v4, NULL, ip4RecvHelper, NULL);
  pthread_create(&recv_thread_v6, NULL, ip6RecvHelper, NULL);
  pthread_create(&send_thread, NULL, sendHelper, NULL);

  pthread_join(recv_thread_v4, NULL);
  pthread_join(recv_thread_v6, NULL);
  pthread_join(send_thread, NULL);

  gettimeofday(&tv_end, NULL);
  tv_sub(&tv_end, &tv_start);
  double time_taken = tv_end.tv_sec * 1000 + tv_end.tv_usec / 1000; // in ms

  printRtts();

  printf("\n==== BEGIN STATS ====\n");
  printf("\nTime taken: %.3fs, Throughput: %.2f (IP/sec)\n", time_taken / 1000, ((double)count / time_taken) * 1000);
  printf("\n==== END STATS ====\n");

  // TO DO: cleanup
  freeSocketList(sockets, count);
  deleteQueue(sendQ);
  delete_map(ip_proto_map);
}

void *ip4RecvHelper(void *args)
{
  struct sockaddr_in saddr, caddr;
  memset(&caddr, 0, sizeof(caddr));
  int clen;

  int sfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  int flags;
  if ((flags = fcntl(sfd, F_GETFL)) == -1)
    errExit("fctnl");
  if (fcntl(sfd, F_SETFL, flags | O_NONBLOCK) == -1)
    errExit("fcntl");

  memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family = AF_INET;
  saddr.sin_port = 0;
  saddr.sin_addr.s_addr = htonl(INADDR_ANY);

  //bind socket to port
  if (bind(sfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1)
    errExit("bind()");

  int n;
  char buff[BUF_SIZE];

  while (remaining_count > 0)
  {
    memset(buff, 0, BUF_SIZE);

    if ((n = recvfrom(sfd, buff, BUF_SIZE, 0, (struct sockaddr *)&caddr, (socklen_t *)&clen)) > 0)
    {
      struct timeval tv;
      gettimeofday(&tv, NULL);

      char ip[IP_V4_BUF_LEN];
      if (getIp4AddrFromPayload(buff, ip) == -1)
        errExit("getIp4AddrFromPayload()");

      struct proto *proto = find_in_map(ip_proto_map, ip);
      if (proto == NULL)
      {
        char err[200];
        sprintf(err, "IP %s not found in hash map", ip);
        errExit(err);
      }

      if (procV4(buff, n, proto, &tv) == -1)
        errExit("provV4()");

      if (proto->nsent < 3)
      {
        if (pthread_mutex_lock(&mtx) != 0)
          errExit("pthread_mutex_lock()");

        qPush(sendQ, proto);

        if (pthread_mutex_unlock(&mtx) != 0)
          errExit("pthread_mutex_unlock()");
      }
      else if (proto->nsent == 3)
      {
        proto->nsent += 1; // increase it so that this else block is not entered again
        remaining_count--;
      }
    }
    else
    {
      /* there was nothing to received, sleep for 10microsec and give sender a chance */
      usleep(10);
    }
  }

  return NULL;
}

void *ip6RecvHelper(void *args)
{
  struct sockaddr_in6 saddr, caddr;
  memset(&caddr, 0, sizeof(caddr));
  int clen;

  int sfd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
  int flags;
  if ((flags = fcntl(sfd, F_GETFL)) == -1)
    errExit("fctnl");
  if (fcntl(sfd, F_SETFL, flags | O_NONBLOCK) == -1)
    errExit("fcntl");

  memset(&saddr, 0, sizeof(saddr));
  saddr.sin6_family = AF_INET6;
  saddr.sin6_port = 0;
  saddr.sin6_addr = in6addr_any;

  //bind socket to port
  if (bind(sfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1)
    errExit("bind()");

  int n;
  char buff[BUF_SIZE];

  while (remaining_count > 0)
  {
    memset(buff, 0, BUF_SIZE);

    if ((n = recvfrom(sfd, buff, BUF_SIZE, 0, (struct sockaddr *)&caddr, (socklen_t *)&clen)) > 0)
    {
      struct timeval tv;
      gettimeofday(&tv, NULL);

      char ip[IP_V6_BUF_LEN];
      // if (getIp6AddrFromPayload(buff, ip) == -1)
      //   errExit("getIp6AddrFromPayload()");
      inet_ntop(AF_INET6, &(caddr.sin6_addr), ip, sizeof(ip));

      printf("Got IPv6: %s\n", ip);

      // struct proto *proto = find_in_map(ip_proto_map, ip);
      // if (proto == NULL)
      // {
      //   char err[200];
      //   sprintf(err, "IP %s not found in hash map", ip);
      //   errExit(err);
      // }

      // if (procV4(buff, n, proto, &tv) == -1)
      //   errExit("provV4()");

      // if (proto->nsent < 3)
      // {
      //   if (pthread_mutex_lock(&mtx) != 0)
      //     errExit("pthread_mutex_lock()");

      //   qPush(sendQ, proto);

      //   if (pthread_mutex_unlock(&mtx) != 0)
      //     errExit("pthread_mutex_unlock()");
      // }
      // else if (proto->nsent == 3)
      // {
      //   proto->nsent += 1; // increase it so that this else block is not entered again
      //   remaining_count--;
      // }
    }
    else
    {
      /* there was nothing to received, sleep for 10microsec and give sender a chance */
      usleep(10);
    }
  }

  return NULL;
}

void *sendHelper(void *args)
{
  int *send_count = (int *)calloc(count, sizeof(int));
  int tot_send_cnt = 0;
  int n;
  struct sockaddr_in saddr;

  for (;;)
  {
    if (pthread_mutex_lock(&mtx) != 0)
      errExit("pthread_mutex_lock()");

    if (sendQ->count > 0)
    {
      struct proto *proto = qFront(sendQ);
      if ((n = (proto->fsend)(proto)) > 0)
      {
        qPop(sendQ);
        tot_send_cnt++;
      }
      else
      {
        printf("sendto returned -1\n");
      }
    }

    if (pthread_mutex_unlock(&mtx) != 0)
      errExit("pthread_mutex_unlock()");
    if (tot_send_cnt == 3 * count)
      break;
  }

  return NULL;
}

void printRtts()
{
  printf("\n==== BEGIN RTTs ====\n\n");
  char ipaddr[IP_V6_BUF_LEN];
  for (int i = 0; i < count; i++)
  {
    struct proto *proto = sockets[i];

    memset(ipaddr, '\0', sizeof(ipaddr));
    if (getIpAddrFromProto(proto, ipaddr) == -1)
      errExit("getIpAddrFromProto");

    printf("%s: %.2fms %f.2ms %f.2ms\n", ipaddr, proto->rtt[0], proto->rtt[1], proto->rtt[2]);
  }
  printf("\n==== END RTTs ====\n");
}