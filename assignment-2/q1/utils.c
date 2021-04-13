#include "./utils.h"

void errExit(char *err)
{
  perror(err);
  exit(EXIT_FAILURE);
}

/*
* Parse IP list file and identify whether each IP is
* v4 or v6. A simple method of identifying . vs :
* is adopted.
* Returns NULL on error.
*/
ip_list_struct *parseIpList(char *file_name)
{
  FILE *fptr = fopen(file_name, "r");
  if (fptr == NULL)
  {
    perror("Error while opening IP list file.\n");
    return NULL;
  }
  ip_list_struct *list = (ip_list_struct *)calloc(1, sizeof(ip_list_struct));
  if (list == NULL)
  {
    perror("Memory error while allocating IP list structure.\n");
    return NULL;
  }
  list->count = 0;
  int curr_sz = 1000, curr_cnt = 0;
  list->ip = (ip_struct **)calloc(curr_sz, sizeof(ip_struct *));
  if (list->ip == NULL)
  {
    perror("Memory error while allocating IP list structure.\n");
    return NULL;
  }
  char ip[IP_V6_BUF_LEN];
  while (fscanf(fptr, " %s", ip) != EOF)
  {
    if (curr_cnt == curr_sz)
    {
      // reallocate memory
      curr_sz *= 2;
      list->ip = (ip_struct **)realloc(list->ip, curr_sz);
      if (list->ip == NULL)
      {
        perror("Memory error while re-allocating IP list structure.\n");
        return NULL;
      }
    }

    list->ip[curr_cnt] = (ip_struct *)calloc(1, sizeof(ip_struct));
    if (list->ip[curr_cnt] == NULL)
    {
      perror("Memory error while allocating ip_struct.\n");
      return NULL;
    }
    // get ip length ignoring any invalid char
    for (int i = 0; i < 10; i++)
    {
      if (ip[i] == '.')
      {
        list->ip[curr_cnt]->isV4 = true;
        break;
      }
      else if (ip[i] == ':')
      {
        list->ip[curr_cnt]->isV4 = false;
        break;
      }
    }

    (list->ip)[curr_cnt]->ip = strdup(ip);
    curr_cnt++;
  }
  list->count = curr_cnt;

  fclose(fptr);
  return list;
}

/* 
* Deallocate the IP list structure
*/
void freeIpList(ip_list_struct *list)
{
  if (list == NULL)
    return;
  int cnt = list->count;
  for (int i = 0; i < cnt; i++)
    free((list->ip)[i]);
  free(list->ip);
  free(list);
}

/*
* Subtract 2 timeval structures (out - in) and store the
* result in `out`.
*/
void tv_sub(struct timeval *out, struct timeval *in)
{
  if ((out->tv_usec -= in->tv_usec) < 0)
  {
    --out->tv_sec;
    out->tv_usec += 1000000;
  }
  out->tv_sec -= in->tv_sec;
}

/*
* Checksum for ICMP message (on IPv4)
*/
u_int16_t in_cksum(u_int16_t *addr, int len)
{
  int nleft = len;
  u_int32_t sum = 0;
  u_int16_t *w = addr;
  u_int16_t answer = 0;

  /*
    * Using a 32 bit accumulator (sum), we add sequential 16 bit words
    * to it, and at the end, fold back all the carry bits from the top
    * 16 bits into the lower 16 bits.
    */
  while (nleft > 1)
  {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1)
  {
    *(unsigned char *)(&answer) = *(unsigned char *)w;
    sum += answer;
  }

  /* add back carry outs from top 16 bits to low 16 bitss */
  sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
  sum += (sum >> 16);                 /* add carry */
  answer = ~sum;                      /* truncate to 16 bits */

  return answer;
}

/*
* Get IP address from struct proto*
* Return:
*   0: success
*   -1: error
*/
int getIpAddrFromProto(struct proto *ip, char *res)
{
  char ipaddr[IP_V6_BUF_LEN];
  if (ip->icmpproto == IPPROTO_ICMP)
  {
    // IPv4
    struct in_addr inaddr = ((struct sockaddr_in *)ip->sasend)->sin_addr;
    if (inet_ntop(AF_INET, &inaddr, ipaddr, sizeof(ipaddr)) == NULL)
      return -1;
  }
  else
  {
    // IPv6
    struct in6_addr inaddr = ((struct sockaddr_in6 *)ip->sasend)->sin6_addr;
    if (inet_ntop(AF_INET6, &inaddr, ipaddr, sizeof(ipaddr)) == NULL)
      return -1;
  }

  strncpy(res, ipaddr, strlen(ipaddr));
  return 0;
}

/*
* Initialize a struct proto object through IP
* Return NULL on error.
*/
struct proto *initIp(ip_struct *ip, int v4_fd, int v6_fd)
{
  struct proto *res = (struct proto *)calloc(1, sizeof(struct proto));
  if (res == NULL)
    return NULL;

  memset(res, 0, sizeof(struct proto));
  res->sasend = NULL;

  if (ip->isV4)
  {
    res->fd = v4_fd;
    res->icmpproto = IPPROTO_ICMP;
    res->fproc = procV4;
    res->fsend = sendV4;
    res->nsent = 0;

    struct sockaddr_in *saddr = (struct sockaddr_in *)calloc(1, sizeof(struct sockaddr_in));
    if (saddr == NULL)
      return NULL;
    memset(saddr, 0, sizeof(struct sockaddr_in));
    inet_pton(AF_INET, ip->ip, &(saddr->sin_addr));
    saddr->sin_family = AF_INET;

    res->sasend = (struct sockaddr *)saddr;
    res->salen = sizeof(struct sockaddr_in);
  }
  else
  {
    res->fd = v6_fd;
    res->icmpproto = IPPROTO_ICMPV6;
    res->fproc = procV6;
    res->fsend = sendV6;
    res->nsent = 0;

    struct sockaddr_in6 *saddr = (struct sockaddr_in6 *)calloc(1, sizeof(struct sockaddr_in6));
    if (saddr == NULL)
      return NULL;
    memset(saddr, 0, sizeof(struct sockaddr_in6));
    inet_pton(AF_INET6, ip->ip, &(saddr->sin6_addr));
    saddr->sin6_family = AF_INET6;

    res->sasend = (struct sockaddr *)saddr;
    res->salen = sizeof(struct sockaddr_in6);
  }
  return res;
}

/*
* Process ICMP data received through IPv4.
* Calculate and store RTT value in the proto object.
* Return:
*   0: success
*   -1: error
*   -2: packet not echo reply, ignore message
*/
int procV4(char *ptr, ssize_t len, struct proto *proto, struct timeval *tvrecv)
{
  int head_len, icmp_len;
  double rtt;
  struct ip *ip = (struct ip *)ptr;
  struct icmp *icmp;
  struct timeval *tvsend;

  head_len = ip->ip_hl << 2; /* length of IP header */
  if (ip->ip_p != IPPROTO_ICMP)
  {
    perror("[procV4] Invalid IP protocol found.\n");
    return -1;
  }

  icmp = (struct icmp *)(ptr + head_len); /* start of ICMP header */
  if ((icmp_len = len - head_len) < 8)
  {
    perror("[procV4] Malformed packet.\n");
    return -1;
  }

  if (icmp->icmp_type == ICMP_ECHOREPLY)
  {
    if (icmp->icmp_id != (uint16_t)getpid())
    {
      perror("[procV4] Not a response to ECHO_REQUEST.\n");
      return -1;
    }
    if (icmp_len < 16)
    {
      perror("[procV4] Not enough data to use.\n");
      return -1;
    }

    /* we sent the time as data to the ICMP msg which is echoed back */
    tvsend = (struct timeval *)icmp->icmp_data;
    tv_sub(tvrecv, tvsend);
    rtt = tvrecv->tv_sec * 1000 + tvrecv->tv_usec / 1000;

    proto->rtt[proto->nsent - 1] = rtt;
  }
  else
  {
    /* Received some other reply, ignore message */
    return -2;
  }
  return 0;
}

/*
* Send ICMP message over IPv4
* Return:
*   0: success
*   -1: error
*/
int sendV4(struct proto *proto)
{
  int len;
  struct icmp *icmp;
  char send_buf[BUF_SIZE];
  icmp = (struct icmp *)send_buf;
  icmp->icmp_type = ICMP_ECHO;
  icmp->icmp_code = 0;
  icmp->icmp_id = getpid();
  icmp->icmp_seq = proto->nsent;
  proto->nsent += 1;
  memset(icmp->icmp_data, 0xa5, DATA_LEN);
  gettimeofday((struct timeval *)icmp->icmp_data, NULL);

  len = 8 + DATA_LEN; /* checksum ICMP header and data */
  icmp->icmp_cksum = 0;
  icmp->icmp_cksum = in_cksum((u_short *)icmp, len);

  int n;
  int offset = 0;
  /* since it is non blocking I/O, send until complete data is sent */
  while (len > 0 && (n = sendto(proto->fd, send_buf + offset, len, 0, proto->sasend, proto->salen)) < len)
  {
    if (n == -1)
    {
      return -1;
    }
    len -= n;
    offset += n;
  }
  return 0;
}

/*
* Process ICMPV6 data received through IPv6.
* Calculate and store RTT value in the proto object.
* Return:
*   0: success
*   -1: error
*   -2: packet not echo reply, ignore message
*/
int procV6(char *ptr, ssize_t len, struct proto *proto, struct timeval *tvrecv)
{
  double rtt;
  struct icmp6_hdr *icmp6;
  struct timeval *tvsend;
  int hlim;

  icmp6 = (struct icmp6_hdr *)ptr;
  if (len < 8)
    return -1; /* malformed packet */

  if (icmp6->icmp6_type == ICMP6_ECHO_REPLY)
  {
    if (icmp6->icmp6_id != (uint16_t)getpid())
      return -1; /* incorrect pid */
    if (len < 16)
      return -1; /* not enough data to use */

    /* we sent the time as data to the ICMP msg which is echoed back */
    tvsend = (struct timeval *)(icmp6 + 1);
    tv_sub(tvrecv, tvsend);
    rtt = tvrecv->tv_sec * 1000 + tvrecv->tv_usec / 1000;

    (proto->rtt)[proto->nsent - 1] = rtt;
  }
  else
  {
    /* Received some other reply, ignore message */
    return -2;
  }

  return 0;
}

/*
* Send ICMPV6 message over IPv6
* Return:
*   0: success
*   -1: error
*/
int sendV6(struct proto *proto)
{
  int len;
  struct icmp6_hdr *icmp6;

  char send_buf[BUF_SIZE];
  icmp6 = (struct icmp6_hdr *)send_buf;
  icmp6->icmp6_type = ICMP6_ECHO_REQUEST;
  icmp6->icmp6_code = 0;
  icmp6->icmp6_id = getpid();
  icmp6->icmp6_seq = proto->nsent;
  proto->nsent += 1;
  memset((icmp6 + 1), 0xa5, DATA_LEN);

  gettimeofday((struct timeval *)(icmp6 + 1), NULL);

  len = 8 + DATA_LEN; /* checksum ICMP header and data */

  /* Note: for ICMPV6, kernel calculates checksum for us */

  /* since it is non blocking I/O, send until complete data is sent */
  int n;
  int offset = 0;
  while (len > 0 && (n = sendto(proto->fd, send_buf + offset, len, 0, proto->sasend, proto->salen)) < len)
  {
    if (n == -1)
    {
      return -1;
    }
    len -= n;
    offset += n;
  }
  return 0;
}

/*
* Deallocate struct proto**
*/
void freeSocketList(struct proto **sockets, int count)
{
  if (sockets == NULL)
    return;
  for (int i = 0; i < count; i++)
  {
    struct proto *s = sockets[i];
    if (s != NULL)
    {
      close(s->fd);
      free(s->sasend);
      free(s);
    }
  }
  free(sockets);
}

/*
* Get IPv4 address from struct in_addr
*/
int getIp4Addr(struct in_addr saddr, char *dest)
{
  char ipaddr[IP_V4_BUF_LEN];
  if (inet_ntop(AF_INET, &saddr, ipaddr, sizeof(ipaddr)) == NULL)
    return -1;

  strcpy(dest, ipaddr);
  return 0;
}

/*
* Get IPv6 address from struct in6_addr
*/
int getIp6Addr(struct in6_addr saddr, char *dest)
{
  char ipaddr[IP_V6_BUF_LEN];
  if (inet_ntop(AF_INET6, &saddr, ipaddr, sizeof(ipaddr)) == NULL)
    return -1;

  strcpy(dest, ipaddr);
  return 0;
}