#include "utils.h"

/**
 * @brief Print error and exit
 * 
 * @param err 
 */
void errExit(char *err)
{
  perror("ERROR:\n");
  perror(err);
  exit(EXIT_FAILURE);
}

void assert(bool condition, char *error_string)
{
  if (!condition)
  {
    errExit(error_string);
  }
}
