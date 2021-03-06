#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/types.h>
#include <pthread.h>
#include "./utils.h"

// server socket file descriptor
int sfd;

// struct for argument passed on to thread
typedef struct
{
  int sfd;
  int cfd;
  struct sockaddr_in caddr;
  parsed_config *config;
  bool *active_connections;
  int *connection_ports;
} arg_struct;

void *connectionHandler(void *args);

int registerClientConnection(char *ip, parsed_config *config, bool *active_connections);

int main(int argc, char **argv)
{
  if (argc != 2)
  {
    errExit("\nUsage: server.out <SERVER_PORT>\n", sfd, -1);
  }
  assert(atoi(argv[1]) != CLIENT_PORT, "Given port reserved for client. Please enter a different port.", sfd, -1);

  // init server setup
  int cfd;
  sfd = serverSetup(atoi(argv[1]));
  struct sockaddr_in caddr;
  int clen = sizeof(caddr);

  // activeConnections[i] is true when server is connected with client (i+1)
  bool *active_connections = (bool *)calloc(MAX_CLIENTS_ALLOWED, sizeof(bool));
  int *connection_ports = (int *)calloc(MAX_CLIENTS_ALLOWED, sizeof(int));
  memset(active_connections, false, MAX_CLIENTS_ALLOWED);

  // parse config file
  parsed_config *config = parseConfigFile();
  pthread_t thread_id;

  struct sockaddr_in saddr;
  socklen_t slen = sizeof(saddr);
  getsockname(sfd, (struct sockaddr *)&saddr, &slen);
  printf("\n ===== Server setup at %s:%d, waiting for connections =====\n", inet_ntoa(saddr.sin_addr), ntohs(saddr.sin_port));

  for (;;)
  {
    // accept connection
    cfd = accept(sfd, (struct sockaddr *)&caddr, (socklen_t *)&clen);
    assert(cfd != -1, "error while clustershell_server accepting clustershell_client request.", sfd, -1);

    // create args structure for thread
    arg_struct *args = (arg_struct *)calloc(1, sizeof(arg_struct));
    assert(args != NULL, "calloc error while creating args object", sfd, -1);
    args->cfd = cfd;
    args->sfd = sfd;
    args->caddr = caddr;
    args->config = config;
    args->active_connections = active_connections;
    args->connection_ports = connection_ports;

    // create thread
    assert(pthread_create(&thread_id, NULL, connectionHandler, (void *)args) == 0, "pthread_create error", sfd, -1);
  }

  // perform cleanup
  close(sfd);
  resetConfigObj(config); // to prevent memory leak
}

/**
 * @brief Return the index of the client corresponding to the IP and register it
 * 
 * @param ip 
 * @param config 
 * @param active_connections 
 * @return int 
 */
int registerClientConnection(char *ip, parsed_config *config, bool *active_connections)
{
  for (int i = 0; i < MAX_CLIENTS_ALLOWED && i < config->count; i++)
  {
    if (strcmp(config->data[i], ip) == 0 && !active_connections[i])
    { // found ip address
      active_connections[i] = true;
      return i;
    }
  }

  return -1;
}

/**
 * @brief Handle each client
 * 
 * @param args 
 * @return void* 
 */
void *connectionHandler(void *args)
{
  // deconstruct args
  int sfd = ((arg_struct *)args)->sfd, cfd = ((arg_struct *)args)->cfd;
  struct sockaddr_in caddr = ((arg_struct *)args)->caddr;
  char *client_ip = inet_ntoa(caddr.sin_addr);
  parsed_config *config = ((arg_struct *)args)->config;
  bool *active_connections = ((arg_struct *)args)->active_connections;
  int *connection_ports = ((arg_struct *)args)->connection_ports;

  printf("\n=== Connected with client IP %s ===\n", client_ip);

  // get the machine index corresponding to IP and register the client
  int idx;
  if ((idx = registerClientConnection(client_ip, config, active_connections)) == -1)
  {
    printf("\n=== IP %s not found in config or IP already has maximum existing connections, exiting ===\n", client_ip);
    pthread_exit(NULL);
  }

  // read the port from client on which the client is establishing server
  // to run commands being sent to it. This is done to enable two clients
  // having the same IP but different ports.
  char client_port_str[10];
  int nread = read(cfd, client_port_str, 10);
  if (nread == 0)
  {
    // connection has been closed
    printf("\n=== Client IP %s has closed connection ===\n", client_ip);
    active_connections[idx] = false;
    close(cfd);
    return NULL;
  }
  if (nread == -1)
  {
    close(cfd);
    printf("Error while reading port from client IP: %s. Closing connection.\n", client_ip);
    pthread_exit(NULL);
  }
  client_port_str[nread] = '\0';
  printf("~ Will be sending commands to machine n%d at %s:%s ~\n", idx + 1, client_ip, client_port_str);
  connection_ports[idx] = atoi(client_port_str);

  // read commands from client
  for (;;)
  {
    char buff[MAX_COMMAND_SIZE + 1];
    int num_read = read(cfd, buff, MAX_COMMAND_SIZE);
    if (num_read == 0 || num_read == -1)
    {
      // connection has been closed
      printf("\n=== Error while reading or Client IP %s has closed connection ===\n", client_ip);
      break;
    }
    buff[num_read] = '\0';

    printf("\n-> Command received from client %s:%d - %s\n", client_ip, ntohs(caddr.sin_port), buff);

    if (strcmp(buff, "nodes") == 0)
    {
      // return a list of active nodes
      int curr_offset = 0;
      char res[MAX_OUTPUT_SIZE + 1];
      for (int i = 0; i < MAX_CLIENTS_ALLOWED; i++)
      {
        if (active_connections[i])
        {
          curr_offset += sprintf(res + curr_offset, "n%d %s\n", i + 1, config->data[i]);
        }
      }
      res[curr_offset] = '\0';

      // write output to client
      write(cfd, res, curr_offset + 1);
      continue;
    }

    /** --- Command is other than "nodes" --- **/

    // create command pipe linked list
    struct command_pipe *cmd_pipe = initCommandPipe();
    createCommandPipe(buff, cmd_pipe);
    struct command *curr_cmd = cmd_pipe->head;
    char output[MAX_OUTPUT_SIZE + 1];
    int output_read_num;
    memset(output, '\0', MAX_OUTPUT_SIZE + 1);

    bool err = false;

    while (curr_cmd)
    {
      if (curr_cmd->machine == 0)
      { // broadcast

        char res[MAX_OUTPUT_SIZE + 1];
        memset(res, '\0', MAX_OUTPUT_SIZE + 1);
        int offset = 0;

        // loop through all currently active connections
        for (int i = 0; i < config->count; i++)
        {
          if (active_connections[i] == false)
          {
            continue;
          }
          printf("-> Running command %s on machine n%d (%s:%d)\n", curr_cmd->cmd, i + 1, config->data[i], connection_ports[i]);
          int cfd_client = clientSetup(config->data[i], connection_ports[i], sfd); // act as a client and send request to the machine
          int cmd_len = strlen(curr_cmd->cmd);

          // send command to server (clustershell_client)
          char input[MAX_COMMAND_SIZE + 1 + MAX_OUTPUT_SIZE + 1];
          strcpy(input, curr_cmd->cmd);                      // copy the command to input array
          input[strlen(curr_cmd->cmd)] = '\0';               // a \0 (null) separater between the command and input to the command
          strcpy(input + strlen(curr_cmd->cmd) + 1, output); // copy the previous output which will hence be the input to this command

          int sz = strlen(input) + 1 + strlen(output) + 1;
          int write_num = write(cfd_client, input, sz);
          if (write_num != sz)
          {
            printf("Error in writing command %s to machine n%d.\n", curr_cmd->cmd, i + 1);
            err = true;
            sprintf(output, "Error in writing command %s to machine n%d.\n", curr_cmd->cmd, i + 1);
            break;
          }

          // read output of command from server (clustershell_client)
          char temp[MAX_OUTPUT_SIZE + 1];
          int res_read_num = read(cfd_client, temp, MAX_OUTPUT_SIZE);
          if (res_read_num == -1)
          {
            printf("Error in reading output of command %s from machine n%d.\n", curr_cmd->cmd, i + 1);
            err = true;
            sprintf(output, "Error in reading output of command %s from machine n%d.\n", curr_cmd->cmd, i + 1);
            break;
          }
          temp[res_read_num] = '\0';
          strcat(res, temp); // concatenate current response to final response
          offset += res_read_num;

          close(cfd_client);
        }

        if (err)
          break;

        strcpy(output, res);
        curr_cmd = curr_cmd->next;
        continue;
      }

      /** ---- It is not a broadcast ---- **/

      int machine = curr_cmd->machine;
      if (machine == -1)
      { // run on same machine from which request came
        machine = idx;
      }
      else
      {
        machine -= 1; // since it is used as array index
      }

      if (machine >= config->count)
      {
        printf("Invalid machine name found: n%d. Please verify that it exists in config in correct order.\n", machine + 1);
        err = true;
        sprintf(output, "Invalid machine name found: n%d. Please verify that it exists in config in correct order.\n", machine + 1);
        break;
      }

      if (active_connections[machine] == false)
      {
        printf("Machine n%d is not connected.", machine + 1);
        err = true;
        sprintf(output, "Machine n%d is not connected.", machine + 1);
        break;
      }

      printf("-> Running command %s on machine n%d (%s:%d)\n", curr_cmd->cmd, machine + 1, config->data[machine], connection_ports[machine]);
      int cfd_client = clientSetup(config->data[machine], connection_ports[machine], sfd); // act as a client and send request to the machine
      int cmd_len = strlen(curr_cmd->cmd);

      // send command to server (clustershell_client)
      char input[MAX_COMMAND_SIZE + 1 + MAX_OUTPUT_SIZE + 1];
      strcpy(input, curr_cmd->cmd);                      // copy the command to input array
      input[strlen(curr_cmd->cmd)] = '\0';               // a \0 (null) separater between the command and input to the command
      strcpy(input + strlen(curr_cmd->cmd) + 1, output); // copy the previous output which will hence be the input to this command

      int sz = strlen(input) + 1 + strlen(output) + 1;
      int write_num = write(cfd_client, input, sz);
      if (write_num != sz)
      {
        printf("Error in writing command %s to machine n%d.\n", curr_cmd->cmd, machine + 1);
        err = true;
        sprintf(output, "Error in writing command %s to machine n%d.\n", curr_cmd->cmd, machine + 1);
        break;
      }

      // read output of command from server (clustershell_client)
      output_read_num = read(cfd_client, output, MAX_OUTPUT_SIZE);
      if (output_read_num == -1)
      {
        printf("Error in reading output of command %s from machine n%d.\n", curr_cmd->cmd, machine + 1);
        err = true;
        sprintf(output, "Error in reading output of command %s from machine n%d.\n", curr_cmd->cmd, machine + 1);
        break;
      }
      output[output_read_num] = '\0';

      close(cfd_client);
      curr_cmd = curr_cmd->next;
    }

    // write final output to the client we got input from
    write(cfd, output, strlen(output) + 1);

    // TO DO: Check for background command?
    resetCommandPipe(cmd_pipe);
  }

  active_connections[idx] = false;
  close(cfd);
  return NULL;
}