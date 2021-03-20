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
#include "utils.h"

int sfd1, sfd2;

void cleanup()
{
  close(sfd1);
  close(sfd2);
}

void sigIntHandler(int sig_num)
{
  assert(sig_num == SIGINT, "unexpected signal received");
  printf("\n=== Disconnecting from server ===\n");
  cleanup();
  _exit(EXIT_SUCCESS);
}

void sigChldHandler(int sig_num)
{
  assert(sig_num == SIGCHLD, "unexpected signal received");
  cleanup();
  _exit(0);
}

char *executeCmd(char *cmd)
{
  while (*cmd == ' ')
    cmd++;

  int len = strlen(cmd);

  if (cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == ' ')
  { // if it is a change dir command
    char *path = cmd + 3;
    // printf("Received cd command with path '%s'\n", path); // debug

    // change directory
    if (chdir(path) == -1)
    {
      char *buff = (char *)calloc(MAX_OUTPUT_SIZE + 1, sizeof(char));
      sprintf(buff, "Error occurred while changing path to %s", path);
      return buff;
    }
  }
  else
  {
    FILE *fptr = popen(cmd, "r"); // run command
                                  // store command in char buffer
    int fd = fileno(fptr);
    assert(fd != -1, "[executeCmd] error getting fd from fptr");

    char *actual_cmd = cmd;
    int actual_cmd_len = strlen(actual_cmd);
    char *cmd_input = cmd + actual_cmd_len + 1;

    // printf("Executing command %s with input %s\n", actual_cmd, cmd_input); // debug

    int pipe_fd[2];
    assert(pipe(pipe_fd) != -1, "[executeCmd] pipe creation error");

    write(pipe_fd[1], cmd_input, strlen(cmd_input));
    close(pipe_fd[1]);

    assert(dup2(pipe_fd[0], STDIN_FILENO) != -1, "[executeCmd] dup2 pipe error");

    int num_read = 0;
    char *buff = (char *)calloc(MAX_OUTPUT_SIZE + 1, sizeof(char));
    num_read = read(fd, buff, MAX_OUTPUT_SIZE);
    assert(num_read != -1, "error while reading from popen output");
    buff[num_read] = '\0';

    close(pipe_fd[0]);
    // return the output
    return buff;
  }

  return NULL;
}

int main(int argc, char **argv)
{
  if (argc != 4)
  {
    errExit("\nUsage: client.out <SERVER_IP> <SERVER_PORT> <CLIENT_PORT>\n");
  }

  int client_port = atoi(argv[3]);

  pid_t child_pid;
  assert((child_pid = fork()) != -1, "client fork error");

  if (child_pid == 0)
  {
    // inside child, do TCP communication
    // here, the clustershell_client will act as a server and wait for the
    // clustershell_server to send a request to run

    int sfd = sfd2, cfd;
    sfd = serverSetup(client_port);

    for (;;)
    {
      struct sockaddr_in caddr;
      int clen = sizeof(caddr);

      // printf("Waiting for connections from server...\n"); // debug
      assert((cfd = accept(sfd, (struct sockaddr *)&caddr, (socklen_t *)&clen)) != -1, "error while clustershell_client accepting clustershell_server request.");

      // printf("Connected with server! Waiting to read...\n"); // debug

      // read command from server
      int sz = MAX_COMMAND_SIZE + 1 + MAX_OUTPUT_SIZE + 1;
      char buff[sz];
      int num_read = read(cfd, buff, sz);
      if (num_read == -1)
      {
        cleanup();
        errExit("error while reading from server");
      }
      buff[num_read] = '\0';

      // printf("Read command %s from server\n", buff); // debug
      // execute the command
      char *out = executeCmd(buff);

      // send output to server
      if (out != NULL)
      {
        // printf("Writing back to server: %s\n", out); // debug
        write(cfd, out, strlen(out));
      }

      free(out);
      close(cfd);
    }
  }
  else
  {
    signal(SIGINT, sigIntHandler);
    signal(SIGCHLD, sigChldHandler);

    // in parent, run shell
    int sfd = sfd1;
    sfd = clientSetup(argv[1], atoi(argv[2]));

    write(sfd, argv[3], strlen(argv[3])); // send the client port to the server to enable client setup from server end

    for (;;)
    {
      printf("shell> ");
      char *cmd_input = (char *)calloc(MAX_COMMAND_SIZE, sizeof(char));
      assert(cmd_input != NULL, "calloc error for command buffer");
      size_t sz = MAX_COMMAND_SIZE;
      int count = getline(&cmd_input, &sz, stdin);
      if (count == 0 || !cmd_input || strcmp(cmd_input, "\n") == 0)
      {
        continue;
      }
      cmd_input[count - 1] = '\0'; // replace \n at end with \0
      count--;
      if (strcmp(cmd_input, "exit") == 0)
      {
        kill(child_pid, SIGKILL); // kill child
        free(cmd_input);
        close(sfd2);
        break;
      }
      // send command to server
      if (write(sfd, cmd_input, count) == -1)
      {
        close(sfd);
        close(sfd2);
        free(cmd_input);
        errExit("error while sending command to server");
        kill(child_pid, SIGKILL); // kill child
      }

      free(cmd_input);

      // printf("Waiting for output from server\n"); // debug
      // read output from server
      char buff[MAX_OUTPUT_SIZE + 1];
      int num_read = read(sfd, buff, MAX_OUTPUT_SIZE);
      if (num_read <= 0)
      {
        close(sfd);
        close(sfd2);
        errExit("error while reading from server");
        kill(child_pid, SIGKILL); // kill child
      }
      buff[num_read] = '\0';
      // printf("Got output from server: %s\n", buff); // debug
      printf("%s\n", buff); // print output
    }
    close(sfd);
  }
}