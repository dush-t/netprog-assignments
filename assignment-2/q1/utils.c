#include "./utils.h"

void errExit(char *err)
{
  perror(err);
  exit(EXIT_FAILURE);
}

ip_list_struct *parseIpList(char *file_name)
{
  FILE *fptr = fopen(file_name, "r");
  if (fptr == NULL)
    errExit("Error while opening IP list file.\n");
  ip_list_struct *list = (ip_list_struct *)calloc(1, sizeof(ip_list_struct));
  if (list == NULL)
    errExit("Memory error while allocating IP list structure.\n");
  list->count = 0;
  int curr_sz = 1000, curr_cnt = 0;
  list->ip = (ip_struct **)calloc(curr_sz, sizeof(ip_struct *));
  if (list->ip == NULL)
    errExit("Memory error while allocating IP list structure.\n");
  char ip[100];
  while (fscanf(fptr, " %s", ip) != EOF)
  {
    if (curr_cnt == curr_sz)
    {
      // reallocate memory
      curr_sz *= 2;
      list->ip = (ip_struct **)realloc(list->ip, curr_sz);
      if (list->ip == NULL)
        errExit("Memory error while re-allocating IP list structure.\n");
    }

    // get ip length ignoring any invalid char
    int len = 0;
    for (int i = 0; i < strlen(ip); i++)
    {
      if (isalnum(ip[i]) || ip[i] == '.' || ip[i] == ':')
        len++;
    }

    list->ip[curr_cnt] = (ip_struct *)calloc(1, sizeof(ip_struct));
    if (len <= 15)
      list->ip[curr_cnt]->isV4 = true;
    else if (len <= 39)
      list->ip[curr_cnt]->isV4 = false;
    else
    {
      char err[200];
      list->count = curr_cnt + 1;
      freeIpList(list);
      sprintf(err, "Unrecognized IP type %s\n", ip);
      errExit(err);
    }

    (list->ip)[curr_cnt]->ip = strdup(ip);
    curr_cnt++;
  }
  list->count = curr_cnt;

  fclose(fptr);
  return list;
}

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

void tv_sub(struct timeval *out, struct timeval *in)
{
  if ((out->tv_usec -= in->tv_usec) < 0)
  {
    --out->tv_sec;
    out->tv_usec += 1000000;
  }
  out->tv_sec -= in->tv_sec;
}

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

int getIpAddrFromProto(struct proto *ip, char *res)
{
  char ipaddr[100];
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

struct proto *initIp(ip_struct *ip, int v4_fd, int v6_fd)
{
  struct proto *res = (struct proto *)calloc(1, sizeof(struct proto));
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
    memset(saddr, 0, sizeof(struct sockaddr_in6));
    inet_pton(AF_INET6, ip->ip, &(saddr->sin6_addr));
    saddr->sin6_family = AF_INET6;

    res->sasend = (struct sockaddr *)saddr;
    res->salen = sizeof(struct sockaddr_in6);
  }

  return res;
}

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
    if (icmp->icmp_id != getpid())
    {
      perror("[procV4] Not a response to ECHO_REQUEST.\n");
      return -1;
    }
    if (icmp_len < 16)
    {
      perror("[procV4] Not enough data to use.\n");
      return -1;
    }

    tvsend = (struct timeval *)icmp->icmp_data;
    tv_sub(tvrecv, tvsend);
    rtt = tvrecv->tv_sec * 1000 + tvrecv->tv_usec / 1000;

    char ip_address[200];
    // struct in_addr addr = ((struct sockaddr_in *)proto->sasend)->sin_addr;
    inet_ntop(AF_INET, &ip->ip_src, ip_address, sizeof(ip_address));
    printf("%d bytes from %s: rtt=%.3f ms\n", icmp_len, ip_address, rtt);

    proto->rtt[proto->nsent - 1] = rtt;
  }
  else
  {
    // TO DO
    return -1;
  }
  return 0;
}

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

  return sendto(proto->fd, send_buf, len, 0, proto->sasend, proto->salen);
}

int procV6(char *ptr, ssize_t len, struct proto *proto, struct timeval *tvrecv)
{
  return -1;
}

int sendV6(struct proto *proto)
{
  return -1;
}

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

int getIpAddrFromPayload(char *payload, char *dest)
{
  struct ip *ip = (struct ip *)payload;
  char ipaddr[25];
  if (inet_ntop(AF_INET, &(ip->ip_src), ipaddr, sizeof(ipaddr)) == NULL)
    return -1;

  strcpy(dest, ipaddr);
  return 0;
}