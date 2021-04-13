#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include <stdbool.h>

void errExit(char *err)
{
  perror(err);
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
  if (argc < 2)
  {
    errExit("Usage: ./get_ip_from_host.out HOSTS_FILE_NAME [OUTPUT_FILE] [Get IPv6? 'y'/'n']");
  }

  FILE *fp_in = fopen(argv[1], "r");
  if (fp_in == NULL)
    errExit("fopen");

  FILE *fp_out;
  if (argc == 3)
  {
    fp_out = fopen(argv[2], "w");
  }
  else
  {
    fp_out = fopen("ip_list_output.txt", "w");
  }
  if (fp_out == NULL)
    errExit("fopen");

  bool is_ip_v6 = false;
  if (argc == 4)
  {
    if (strcmp(argv[3], "y") == 0)
      is_ip_v6 = true;
  }

  char hostname[100], ipaddr[100];
  struct addrinfo hints;
  struct addrinfo *result;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  if (is_ip_v6)
    hints.ai_family = AF_INET6;

  while (fscanf(fp_in, " %s", hostname) != EOF)
  {
    int s;
    if ((s = getaddrinfo(hostname, NULL, &hints, &result)) == 0)
    {
      memset(ipaddr, 0, sizeof(ipaddr));
      if (is_ip_v6)
      {
        inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)result->ai_addr)->sin6_addr), ipaddr, sizeof(ipaddr));
      }
      else
      {
        inet_ntop(AF_INET, &(((struct sockaddr_in *)result->ai_addr)->sin_addr), ipaddr, sizeof(ipaddr));
      }
      fprintf(fp_out, "%s\n", ipaddr);
    }
    else
    {
      fprintf(stderr, "%s: getaddrinfo: %s\n", hostname, gai_strerror(s));
    }
  }

  fclose(fp_in);
  fclose(fp_out);
}