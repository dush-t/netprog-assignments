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
#include "./utils.h"

// sfd1 is for client -> server connection (when clustershell_client acts as a client)
// sfd2 is for when clustershell_client acts as a server
int sfd1 = -1, sfd2 = -1;

void cleanup()
{
  close(sfd1);
  close(sfd2);
}

// If Ctrl+C is pressed, cleanup and terminate
void sigIntHandler(int sig_num)
{
  assert(sig_num == SIGINT, "unexpected signal received", sfd1, sfd2);
  printf("\n=== Disconnecting from server ===\n");
  cleanup();
  _exit(EXIT_SUCCESS);
}

// if child (the server waiting to receive commands) is terminated,
// cleanup and terminate parent
void sigChldHandler(int sig_num)
{
  assert(sig_num == SIGCHLD, "unexpected signal received", sfd1, sfd2);
  cleanup();
  printf("\n=== Disconnecting from server ===\n");
  _exit(0);
}

// SIGUSR1 handler for child
void sigUsrHandler(int sig_num)
{
  assert(sig_num == SIGUSR1, "unexpected signal received", sfd1, sfd2);
  cleanup();
  _exit(EXIT_FAILURE);
}

char *executeCmd(char *cmd)
{
  // remove leading spaces
  while (*cmd == ' ')
    cmd++;

  int len = strlen(cmd);

  if (cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == ' ')
  { // if it is a change dir command
    char *path = cmd + 3;

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
    assert(fd != -1, "[executeCmd] error getting fd from fptr", sfd1, sfd2);

    // the command received from server has the form: "command\0command_input" where \0
    // is the null character. The "command" is the actual shell command like "ls" and "cat"
    // and the "command_input" is the input that should be sent to command (ie, the output of
    // previous command in the pipe).

    char *actual_cmd = cmd;
    int actual_cmd_len = strlen(actual_cmd);    // since there is a null character after the actual command, this length would be length of actual command
    char *cmd_input = cmd + actual_cmd_len + 1; // command input starts after the actual command and a null character

    // pipe is created here so that the command running on shell can take in the command_input we have passed
    // we write the input to the pipe, close the write end and replace stdin with the read end of pipe
    int pipe_fd[2];
    assert(pipe(pipe_fd) != -1, "[executeCmd] pipe creation error", sfd1, sfd2);

    write(pipe_fd[1], cmd_input, strlen(cmd_input));
    close(pipe_fd[1]);

    assert(dup2(pipe_fd[0], STDIN_FILENO) != -1, "[executeCmd] dup2 pipe error", sfd1, sfd2);

    int num_read = 0;
    char *buff = (char *)calloc(MAX_OUTPUT_SIZE + 1, sizeof(char));
    num_read = read(fd, buff, MAX_OUTPUT_SIZE);
    assert(num_read != -1, "error while reading from popen output", sfd1, sfd2);
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
    errExit("\nUsage: client.out <SERVER_IP> <SERVER_PORT> <CLIENT_PORT>\n", sfd1, sfd2);
  }

  int client_port = atoi(argv[3]);

  pid_t child_pid;
  assert((child_pid = fork()) != -1, "client fork error", sfd1, sfd2);

  if (child_pid == 0)
  {
    signal(SIGUSR1, sigUsrHandler);
    // inside child, do TCP communication
    // here, the clustershell_client will act as a server and wait for the
    // clustershell_server to send a request to run on this machine

    sfd2 = serverSetup(client_port);
    int sfd = sfd2, cfd;

    // wait for clustershell_server to send commands to clustershell_client
    for (;;)
    {
      struct sockaddr_in caddr;
      int clen = sizeof(caddr);

      assert((cfd = accept(sfd, (struct sockaddr *)&caddr, (socklen_t *)&clen)) != -1, "error while clustershell_client accepting clustershell_server request.", sfd1, sfd2);

      // read command from server
      int sz = MAX_COMMAND_SIZE + 1 + MAX_OUTPUT_SIZE + 1;
      char buff[sz];
      int num_read = read(cfd, buff, sz);
      assert(num_read != -1, "error while reading from server", sfd1, sfd2);
      buff[num_read] = '\0';

      // execute the command
      char *out = executeCmd(buff);

      // send output to server
      if (out != NULL)
      {
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
    sfd1 = clientSetup(argv[1], atoi(argv[2]), -1);
    int sfd = sfd1;

    write(sfd, argv[3], strlen(argv[3])); // send the port on which cs_client has setup server to the cs_server to enable cs_server to connect

    for (;;)
    {
      printf("shell> ");

      // read command from shell
      char *cmd_input = (char *)calloc(MAX_COMMAND_SIZE, sizeof(char));
      assert(cmd_input != NULL, "calloc error for command buffer", sfd1, sfd2);
      size_t sz = MAX_COMMAND_SIZE;
      int count = getline(&cmd_input, &sz, stdin);
      if (count == 0 || !cmd_input || strcmp(cmd_input, "\n") == 0)
      {
        continue;
      }
      cmd_input[count - 1] = '\0'; // replace \n at end with \0
      count--;

      // close and connection if exit command
      if (strcmp(cmd_input, "exit") == 0)
      {
        printf("\nExiting...\n");
        kill(child_pid, SIGUSR1); // kill child
        free(cmd_input);
        close(sfd1);
        close(sfd2);
        break;
      }

      // send command to server
      if (write(sfd, cmd_input, count) == -1)
      {
        kill(child_pid, SIGUSR1); // kill child
        free(cmd_input);
        errExit("error while sending command to server", sfd1, sfd2);
      }

      free(cmd_input);

      // read output from server
      char buff[MAX_OUTPUT_SIZE + 1];
      int num_read = read(sfd, buff, MAX_OUTPUT_SIZE);
      if (num_read <= 0)
      {
        kill(child_pid, SIGUSR1); // kill child
        errExit("error while reading from server", sfd1, sfd2);
      }
      buff[num_read] = '\0';

      printf("%s\n", buff); // print output
    }
    close(sfd);
  }
}