#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define TCP_BACKLOG 5
#define CLIENT_PORT 8000
#define CONFIG_FILE_PATH "./config.txt"
#define MAX_CLIENTS_ALLOWED 100
#define MAX_COMMAND_SIZE 1024
#define MAX_OUTPUT_SIZE 1024

void assert(bool condition, char *error_string);

void errExit(char *err);

int stringToNum(char *str);

int serverSetup(int port);

typedef struct
{
  char **data;
  int count;
} parsed_config;

struct command
{
  char *cmd;
  int machine;
  struct command *next;
};

struct command_pipe
{
  struct command *head;
  struct command *tail;
  int count;
};

parsed_config *parseConfigFile();

void resetConfigObj(parsed_config *config);

struct command_pipe *initCommandPipe();

void resetCommandPipe(struct command_pipe *cmd_pipe);

void createCommandPipe(char *cmd_input, struct command_pipe *cmd_pipe);

void printCommandPipe(struct command_pipe *cmd_pipe);

int clientSetup(char *addr, int port);

#endif