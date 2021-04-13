#include "utils.h"
#include "queue.h"
#include "hash_map.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#define HASH_MAP_SZ 7919

/* socket descriptors for sending over IPv4 and IPv6 */
int v4_fd, v6_fd;
/* array of proto* object corresponding to each IP */
struct proto **sockets = NULL;
/* total number of IPs */
int count;
/* Queue having proto* structures to send data to (like a job queue) */
queue *sendQ = NULL;
/* mutex for synchronization b/w threads */
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
/* hash map to store IPs and search in O(1) */
hash_map *ip_proto_map = NULL;
/* parse IP file into ip_list_struct */
ip_list_struct *ip_list = NULL;
/* number of IPv4 addresses in IP file */
int ipv4_count = 0;
/* number of IPv6 addresses in IP file */
int ipv6_count = 0;

void *ip4RecvHelper(void *args);
void *ip6RecvHelper(void *args);
void *sendHelper(void *args);
void printRtts();
void cleanup();
void cleanupAndExit(char *err);

int main(int argc, char **argv)
{
  if (argc != 2)
  {
    cleanupAndExit("Usage: ./rtt.out [IP_LIST_FILE_PATH]\n");
  }

  /* Parse IP input file */
  char *ip_list_file = argv[1];
  ip_list = parseIpList(argv[1]);
  if (ip_list == NULL)
    cleanupAndExit("parseIpList()");
  count = ip_list->count;

  for (int i = 0; i < count; i++)
  {
    if (ip_list->ip[i]->isV4)
      ipv4_count++;
    else
      ipv6_count++;
  }

  /* Assign memory for storing proto* structures */
  sockets = (struct proto **)calloc(count, sizeof(struct proto *));
  if (sockets == NULL)
    cleanupAndExit("calloc error while creating sockets struct\n");
  memset(sockets, 0, count * sizeof(struct proto *));

  /* try to get root permissions, ignore if failed */
  setuid(getuid());

  /* initialise fd for communication */
  v4_fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  if (v4_fd == -1)
    cleanupAndExit("v4_fd error.");
  v6_fd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
  if (v6_fd == -1)
    cleanupAndExit("v6_fd error.");

  int no = 0;
  setsockopt(v6_fd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&no, sizeof(no)); /* ignore if it gives error */

  /* make sockets nonblocking */
  int flags;
  if ((flags = fcntl(v4_fd, F_GETFL)) == -1)
    cleanupAndExit("fctnl");
  if (fcntl(v4_fd, F_SETFL, flags | O_NONBLOCK) == -1)
    cleanupAndExit("fcntl");
  if ((flags = fcntl(v6_fd, F_GETFL)) == -1)
    cleanupAndExit("fctnl");
  if (fcntl(v6_fd, F_SETFL, flags | O_NONBLOCK) == -1)
    cleanupAndExit("fcntl");

  /* init hash map */
  ip_proto_map = init_map(HASH_MAP_SZ);
  if (ip_proto_map == NULL)
    cleanupAndExit("init_map()");

  /* initialise proto structure for each ip */
  bool v4_found = false, v6_found = false;
  for (int i = 0; i < count; i++)
  {
    if ((sockets[i] = initIp(ip_list->ip[i], v4_fd, v6_fd)) == NULL)
    {
      cleanupAndExit("Error while initialising IP address structure.\n");
    }

    /* add to hash map */
    char *ip = (char *)calloc(IP_V6_BUF_LEN, sizeof(char));
    if (ip == NULL)
      cleanupAndExit("calloc()");

    if (getIpAddrFromProto(sockets[i], ip) == -1)
      cleanupAndExit("getIpAddrFromProto");

    if (insert_into_map(ip_proto_map, ip, sockets[i]) == -1)
      cleanupAndExit("insert_into_map");
  }

  /* IP List no longer needed */
  freeIpList(ip_list);
  ip_list = NULL;

  /* Initialize send queue and add sockets to it */
  sendQ = initQueue();
  if (sendQ == NULL)
    cleanupAndExit("initQueue()");

  for (int i = 0; i < count; i++)
  {
    if (qPush(sendQ, sockets[i]) == -1)
      cleanupAndExit("qPush()");
  }

  struct timeval tv_start, tv_end;
  gettimeofday(&tv_start, NULL);

  pthread_t recv_thread_v4, recv_thread_v6, send_thread;
  if (pthread_create(&recv_thread_v4, NULL, ip4RecvHelper, NULL) != 0)
    cleanupAndExit("pthread_create");

  if (pthread_create(&recv_thread_v6, NULL, ip6RecvHelper, NULL) != 0)
    cleanupAndExit("pthread_create");

  if (pthread_create(&send_thread, NULL, sendHelper, NULL) != 0)
    cleanupAndExit("pthread_create");

  /* create threads for receiving ICMP msg, receiving ICMPV6 msg and sending ICMP msg */
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

  cleanup();
}

/*
* Receive ICMP messages over IPv4 until all IP's RTTs have been calculated
*/
void *ip4RecvHelper(void *args)
{
  struct sockaddr_in saddr, caddr;
  memset(&caddr, 0, sizeof(caddr));
  int clen;

  int sfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  int flags;
  if ((flags = fcntl(sfd, F_GETFL)) == -1)
    cleanupAndExit("fctnl");
  if (fcntl(sfd, F_SETFL, flags | O_NONBLOCK) == -1)
    cleanupAndExit("fcntl");

  memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family = AF_INET;
  saddr.sin_port = 0;
  saddr.sin_addr.s_addr = htonl(INADDR_ANY);

  //bind socket to port
  if (bind(sfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1)
    cleanupAndExit("bind()");

  int n;
  char buff[BUF_SIZE];

  while (ipv4_count > 0)
  {
    memset(buff, 0, BUF_SIZE);
    clen = sizeof(caddr);

    if ((n = recvfrom(sfd, buff, BUF_SIZE, 0, (struct sockaddr *)&caddr, (socklen_t *)&clen)) > 0)
    {
      struct timeval tv;
      gettimeofday(&tv, NULL);

      char ip[IP_V4_BUF_LEN];
      if (getIp4Addr(caddr.sin_addr, ip) == -1)
        cleanupAndExit("getIp4Addr()");

      // printf("Recv Ip v4: %s\n", ip);

      struct proto *proto = find_in_map(ip_proto_map, ip);
      if (proto == NULL)
      {
        /* IP not found in hash map */
        continue;
      }

      int proc_res;
      if ((proc_res = (proto->fproc)(buff, n, proto, &tv)) == -1)
        cleanupAndExit("procV4()");
      else if (proc_res == -2)
        continue;

      if (proto->nsent < 3)
      {
        if (pthread_mutex_lock(&mtx) != 0)
          cleanupAndExit("pthread_mutex_lock()");

        qPush(sendQ, proto);

        if (pthread_mutex_unlock(&mtx) != 0)
          cleanupAndExit("pthread_mutex_unlock()");
      }
      else if (proto->nsent == 3)
      {
        proto->nsent += 1; // increase it so that this else block is not entered again
        ipv4_count--;
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

/*
* Receive ICMPV6 messages over IPv6 until all IP's RTTs have been calculated
*/
void *ip6RecvHelper(void *args)
{
  struct sockaddr_in6 saddr, caddr;
  memset(&caddr, 0, sizeof(caddr));
  int clen;

  int sfd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
  int flags;
  if ((flags = fcntl(sfd, F_GETFL)) == -1)
    cleanupAndExit("fctnl");
  if (fcntl(sfd, F_SETFL, flags | O_NONBLOCK) == -1)
    cleanupAndExit("fcntl");

  memset(&saddr, 0, sizeof(saddr));
  saddr.sin6_family = AF_INET6;
  saddr.sin6_port = 0;
  saddr.sin6_addr = in6addr_any;

  struct icmp6_filter myfilt;
  ICMP6_FILTER_SETBLOCKALL(&myfilt);
  ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &myfilt);
  setsockopt(sfd, IPPROTO_IPV6, ICMP6_FILTER, &myfilt, sizeof(myfilt)); /* ignore errors as this is an optimization */

  //bind socket to port
  if (bind(sfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1)
    cleanupAndExit("bind()");

  int n;
  char buff[BUF_SIZE];

  while (ipv6_count > 0)
  {
    memset(buff, 0, BUF_SIZE);
    clen = sizeof(caddr);

    if ((n = recvfrom(sfd, buff, BUF_SIZE, 0, (struct sockaddr *)&caddr, (socklen_t *)&clen)) > 0)
    {
      struct timeval tv;
      gettimeofday(&tv, NULL);

      char ip[IP_V6_BUF_LEN];
      getIp6Addr(caddr.sin6_addr, ip);

      // printf("Recv Ip v6: %s\n", ip);

      struct proto *proto = find_in_map(ip_proto_map, ip);
      if (proto == NULL)
      {
        /* IP not found in hash map */
        continue;
      }

      int proc_res;
      if ((proc_res = (proto->fproc)(buff, n, proto, &tv)) == -1)
        cleanupAndExit("procV6()");
      else if (proc_res == -2)
        continue;

      if (proto->nsent < 3)
      {
        if (pthread_mutex_lock(&mtx) != 0)
          cleanupAndExit("pthread_mutex_lock()");

        qPush(sendQ, proto);

        if (pthread_mutex_unlock(&mtx) != 0)
          cleanupAndExit("pthread_mutex_unlock()");
      }
      else if (proto->nsent == 3)
      {
        proto->nsent += 1; // increase it so that this else block is not entered again
        ipv6_count--;
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

/*
* Send ICMP messages. No need to differentiate between v4 and v6 here as proto->fsend handles it.
*/
void *sendHelper(void *args)
{
  int *send_count = (int *)calloc(count, sizeof(int));
  if (send_count == NULL)
    cleanupAndExit("calloc");

  int tot_send_cnt = 0;

  for (;;)
  {
    if (pthread_mutex_lock(&mtx) != 0)
      cleanupAndExit("pthread_mutex_lock()");

    if (sendQ->count > 0)
    {
      struct proto *proto = qFront(sendQ);
      if ((proto->fsend)(proto) == 0)
      {
        qPop(sendQ);
        tot_send_cnt++;
      }
    }

    if (pthread_mutex_unlock(&mtx) != 0)
      cleanupAndExit("pthread_mutex_unlock()");

    /* if 3 ICMP messages sent per IP, break */
    if (tot_send_cnt == 3 * count)
      break;
  }

  return NULL;
}

/*
* Print all RTT values
*/
void printRtts()
{
  printf("\n==== BEGIN RTTs ====\n\n");
  char ipaddr[IP_V6_BUF_LEN];
  for (int i = 0; i < count; i++)
  {
    struct proto *proto = sockets[i];

    memset(ipaddr, '\0', sizeof(ipaddr));
    if (getIpAddrFromProto(proto, ipaddr) == -1)
      cleanupAndExit("getIpAddrFromProto");

    printf("%s: %.2fms %.2fms %.2fms\n", ipaddr, proto->rtt[0], proto->rtt[1], proto->rtt[2]);
  }
  printf("\n==== END RTTs ====\n");
}

/* Free heap memory */
void cleanup()
{
  if (sockets != NULL)
    freeSocketList(sockets, count);
  if (sendQ != NULL)
    deleteQueue(sendQ);
  if (ip_proto_map != NULL)
    delete_map(ip_proto_map);
  if (ip_list != NULL)
    freeIpList(ip_list);

  sockets = NULL;
  sendQ = NULL;
  ip_proto_map = NULL;
  ip_list = NULL;
}

void cleanupAndExit(char *err)
{
  cleanup();
  cleanupAndExit(err);
}