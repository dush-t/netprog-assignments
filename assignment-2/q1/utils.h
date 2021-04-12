#ifndef UTILS_H
#define UTILS_H

#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <math.h>
#include <sys/wait.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>

#define BUF_SIZE 1500
#define DATA_LEN 56
#define IP_V4_BUF_LEN 25
#define IP_V6_BUF_LEN 100

typedef struct
{
  bool isV4;
  char *ip;
} ip_struct;

typedef struct
{
  ip_struct **ip;
  int count;
} ip_list_struct;

struct proto
{
  int (*fproc)(char *, ssize_t, struct proto *, struct timeval *);
  int (*fsend)(struct proto *);
  struct sockaddr *sasend; /* sockaddr for send, from getaddrinfo */
  socklen_t salen;         /* length of sockaddr */
  int icmpproto;           /* IPPROTO_ICMP or IPPROTO_ICMPV6 */
  int fd;                  /* socket fd */
  int nsent;               /* number of packets sent */
  double rtt[3];           /* store RTTs */
};

void errExit(char *err);

ip_list_struct *parseIpList(char *file_name);

void freeIpList(ip_list_struct *list);

void tv_sub(struct timeval *out, struct timeval *in);

u_int16_t in_cksum(u_int16_t *addr, int len);

struct proto *initIp(ip_struct *, int v4_fd, int v6_fd);

int procV4(char *, ssize_t, struct proto *, struct timeval *);

int sendV4(struct proto *);

int procV6(char *, ssize_t, struct proto *, struct timeval *);

int sendV6(struct proto *);

void freeSocketList(struct proto **sockets, int count);

int getIpAddrFromProto(struct proto *ip, char *res);

int getIp4Addr(struct in_addr saddr, char *dest);

int getIp6Addr(struct in6_addr saddr, char *dest);

#endif