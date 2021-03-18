#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

void assert(bool condition, char *error_string);

void errExit(char *err);

int stringToNum(char *str);

#endif