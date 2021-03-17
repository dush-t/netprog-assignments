#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "./hash_map.h"

#ifndef SHELL_H
#define SHELL_H

#define MAX_NUM_ARGS 20 // max number of args allowed
#define MAX_ARG_LEN 30  // max arg length allowed

/* ---- VARIABLES ---- */

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
  command *head;     // first command in linked list
  command *tail;     // last command in linked list
  int count;         // number of commands
  bool isBackground; // true if the commands are to be run in background
} command_pipe;

char *cmd_input; // input typed in shell

/* ---- FUNCTIONS ---- */

/**
 * @brief 
 * 
 * @return command_pipe* 
 */
command_pipe *initCmdPipe();

/**
 * @brief Create the command pipeline
 * 
 * @param cmd_input 
 * @param cmd_pipe 
 */
void createCmdPipe(char *cmd_input, command_pipe *cmd_pipe, hash_map *sc_map);

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

#endif