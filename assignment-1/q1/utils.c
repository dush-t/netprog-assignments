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

int stringToNum(char *str)
{
  int num = 0;
  int len = strlen(str);

  for (int i = 0; i < len; i++)
  {
    if (str[i] == '\n')
      continue;

    assert(isdigit(str[i]), "invalid number");
    num = num * 10 + str[i] - '0';
  }
  return num;
}