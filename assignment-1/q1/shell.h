#ifndef SHELL_H
#define SHELL_H

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include "./hash_map.h"

#define MAX_NUM_ARGS 20 // max number of args allowed
#define MAX_ARG_LEN 30  // max arg length allowed
#define STDOUT_BUF_SIZE 500

/* ---- VARIABLES ---- */

typedef enum
{
  SINGLE_PIPE = 1,
  DOUBLE_PIPE,
  TRIPLE_PIPE
} pipe_count;

/**
 * @brief Struct for a single command
 * 
 */
typedef struct __COMMAND_NODE__ command;
struct __COMMAND_NODE__
{
  int argc;                       // num of args
  char *argv[MAX_NUM_ARGS + 1];   // list of args
  int par_offset;                 // parent offset for || and ||| pipes. = -2 for 2nd command in || or |||. = -3 for 3rd command.
  pipe_count prev_pipe_count;     // count of pipes on the left of the command
  bool in_redirect;               // is input redirected (<)
  char in_file[MAX_ARG_LEN + 1];  // input file in case input redirected
  bool out_redirect;              // is output redirected (>)
  bool out_append;                // is output appended (>>)
  char out_file[MAX_ARG_LEN + 1]; // output file in case of output redirection
  char *token;                    // raw string of the command
  command *next;
};

/**
 * @brief LinkedList structure for piped commands
 * 
 */
typedef struct
{
  command *head;      // first command in linked list
  command *tail;      // last command in linked list
  int count;          // number of commands
  bool is_background; // true if the commands are to be run in background
} command_pipe;

/* ---- FUNCTIONS ---- */

/**
 * @brief 
 * 
 * @return command_pipe* 
 */
command_pipe *initCmdPipe();

/**
 * @brief Create command pipeline linked list from the input
 * 
 * @param cmd_input 
 * @param cmd_pipe 
 * @param sc_map 
 * @return true when it is not a shortcut command
 * @return false when it is a shortcut command
 */
bool createCmdPipe(char *cmd_input, command_pipe *cmd_pipe, hash_map *sc_map);

/**
 * @brief Utility function to print all commands in pipeline
 * 
 * @param cmd_pipe 
 */
void printCmdPipe(command_pipe *cmd_pipe);

/**
 * @brief Reset (deallocate memory) command pipeline after running shell commands
 * 
 * @param cmd_pipe 
 */
void resetCmdPipe(command_pipe *cmd_pipe);

/**
 * @brief Execute the commands in the given pipeline
 * 
 */
void executeCmdPipe(command_pipe *cmd_pipe, pid_t initial_pgrp);

#endif