#include "utils.h"
#include "queue.h"
#include "hash_map.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#define HASH_MAP_SZ 7919
#define DEFAULT_TIME_LIMIT 30

/* socket descriptors for sending over IPv4 and IPv6 */
int v4_fd, v6_fd;
/* array of proto* object corresponding to each IP */
struct proto **sockets = NULL;
/* total number of IPs in file (may include duplicates) */
int count;
/* total number of unique IPs in file */
int unique_count = 0;
/* Queue having proto* structures to send data to (like a job queue) */
queue *sendQ = NULL;
/* mutex for synchronization b/w threads */
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
/* hash map to store IPs and search in O(1) */
hash_map *ip_proto_map = NULL;
/* parse IP file into ip_list_struct */
ip_list_struct *ip_list = NULL;
/* number of unique IPv4 addresses in IP file */
int ipv4_count = 0;
/* number of unique IPv6 addresses in IP file */
int ipv6_count = 0;
/* number of IPs in which last packet failed to send */
int ipv4_fail = 0, ipv6_fail = 0;
/* threads */
pthread_t recv_thread_v4, recv_thread_v6, send_thread;
/* stop threads if true */
bool stop_threads = false;
/* number of packets for which RTT could not be calculated */
int loss_count = 0;

void *ip4RecvHelper(void *args);
void *ip6RecvHelper(void *args);
void *sendHelper(void *args);
void printRtts();
void cleanup();
void cleanupAndExit(char *err);
void sigAlrmHandler(int signum);

int main(int argc, char **argv)
{
  signal(SIGALRM, sigAlrmHandler);
  if (argc < 2)
  {
    cleanupAndExit("Usage: ./rtt.out IP_LIST_FILE_PATH [TIME_LIMIT] [SHOW_STATS (y/n)]\n");
  }

  /* Parse IP input file */
  char *ip_list_file = argv[1];
  ip_list = parseIpList(argv[1]);
  if (ip_list == NULL)
    cleanupAndExit("parseIpList()");
  count = ip_list->count;

  /* get time limit in seconds */
  int time_limit = DEFAULT_TIME_LIMIT;
  if (argc == 3)
    time_limit = atoi(argv[2]);

  /* check if stats are to be shown */
  bool show_stats = false;
  if (argc == 4 && strcmp(argv[3], "y") == 0)
    show_stats = true;

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
    if (find_in_map(ip_proto_map, ip_list->ip[i]->ip) == NULL)
    {
      if ((sockets[unique_count] = initIp(ip_list->ip[i], v4_fd, v6_fd)) == NULL)
      {
        cleanupAndExit("Error while initialising IP address structure.\n");
      }

      /* add to hash map */
      char *ip = (char *)calloc(IP_V6_BUF_LEN, sizeof(char));
      if (ip == NULL)
        cleanupAndExit("calloc()");

      if (getIpAddrFromProto(sockets[unique_count], ip) == -1)
        cleanupAndExit("getIpAddrFromProto");

      int res;
      if ((res = insert_into_map(ip_proto_map, ip, sockets[unique_count])) == -1)
        cleanupAndExit("insert_into_map");
      else if (res == 0)
      {
        /* inserting in map was success */
        if (sockets[unique_count]->icmpproto == IPPROTO_ICMP)
          ipv4_count++;
        else
          ipv6_count++;

        unique_count++;
      }
    }
    else
    {
      /* ignore duplicate IP */
      printf("Note: Ignoring duplicate IP %s\n", ip_list->ip[i]->ip);
    }
  }

  /* IP List no longer needed */
  freeIpList(ip_list);
  ip_list = NULL;

  /* Initialize send queue and add sockets to it */
  sendQ = initQueue();
  if (sendQ == NULL)
    cleanupAndExit("initQueue()");

  for (int i = 0; i < unique_count; i++)
  {
    if (qPush(sendQ, sockets[i]) == -1)
      cleanupAndExit("qPush()");
  }

  /* start time for throughput calculation */
  struct timeval tv_start, tv_end;
  gettimeofday(&tv_start, NULL);

  /* start alarm for time limit */
  alarm(time_limit);

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

  /* end time for throughput calculation */
  gettimeofday(&tv_end, NULL);
  tv_sub(&tv_end, &tv_start);
  double time_taken = tv_end.tv_sec * 1000 + tv_end.tv_usec / 1000; // in ms

  printRtts();

  if (show_stats)
  {
    printf("\n==== BEGIN STATS ====\n");
    printf("\nNo. of unique IPs: %d, RTT success: %d, RTT loss: %d", unique_count, 3 * unique_count - loss_count, loss_count);
    printf("\nTime taken: %.3fs, Timeout expired: %s, Throughput: %.2f (IP/sec)\n", time_taken / 1000, stop_threads ? "yes" : "no", ((double)unique_count / time_taken) * 1000);
    printf("\n==== END STATS ====\n");
  }

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

  while (ipv4_count - ipv4_fail > 0)
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
      if (stop_threads)
        break;
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

  while (ipv6_count - ipv6_fail > 0)
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
      if (stop_threads)
        break;

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
  int *send_count = (int *)calloc(unique_count, sizeof(int));
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
      else
      {
        /* 
        * There was an error while sending data.
        * Increase send count but do not pop element from queue
        * if tries remaining in order to retry sending.
        */
        tot_send_cnt++;

        if (proto->nsent == 3)
        {
          /* if it has been tried 3 times, simply pop it */
          qPop(sendQ);
          proto->nsent += 1;

          /* receiver should not wait for this */
          if (proto->icmpproto == IPPROTO_ICMP)
            ipv4_fail++;
          else
            ipv6_fail++;
        }
        else if (proto->nsent < 3)
        {
          /* retry */
          proto->rtt[proto->nsent] = -1;
          proto->nsent += 1;
        }
      }
    }
    else
    {
      if (stop_threads)
      {
        if (pthread_mutex_unlock(&mtx) != 0)
          cleanupAndExit("pthread_mutex_unlock()");
        break;
      }
    }

    if (pthread_mutex_unlock(&mtx) != 0)
      cleanupAndExit("pthread_mutex_unlock()");

    /* if 3 ICMP messages sent per IP, break */
    if (tot_send_cnt == 3 * unique_count)
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
  for (int i = 0; i < unique_count; i++)
  {
    struct proto *proto = sockets[i];

    memset(ipaddr, '\0', sizeof(ipaddr));
    if (getIpAddrFromProto(proto, ipaddr) == -1)
      cleanupAndExit("getIpAddrFromProto");

    printf("%s:", ipaddr);

    if (proto->rtt[0] == -1)
    {
      printf(" *");
      loss_count++;
    }
    else
    {
      printf(" %.2fms", proto->rtt[0]);
    }
    if (proto->rtt[1] == -1)
    {
      printf(" *");
      loss_count++;
    }
    else
    {
      printf(" %.2fms", proto->rtt[1]);
    }
    if (proto->rtt[2] == -1)
    {
      printf(" *");
      loss_count++;
    }
    else
    {
      printf(" %.2fms", proto->rtt[2]);
    }

    printf("\n");
  }
  printf("\n==== END RTTs ====\n");
}

/* Free heap memory */
void cleanup()
{
  if (sockets != NULL)
    freeSocketList(sockets, unique_count);
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
  errExit(err);
}

void sigAlrmHandler(int signum)
{
  if (signum != SIGALRM)
    cleanupAndExit("sigalrm");
  stop_threads = true;
  printf("\n##### Time exceeded, stopping threads #####\n");
}