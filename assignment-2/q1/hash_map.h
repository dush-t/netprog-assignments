#ifndef HASH_MAP_H
#define HASH_MAP_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

typedef struct __HASH_MAP_NODE__ map_node;

struct __HASH_MAP_NODE__
{
  char *ip;
  void *data;
  map_node *next;
};

struct __HASH_MAP_BUCKET__
{
  map_node *head;
  int capacity;
};

typedef struct __HASH_MAP_BUCKET__ map_bucket;

struct __HASH_MAP__
{
  map_bucket **buckets;
  int map_size;
};

typedef struct __HASH_MAP__ hash_map;

hash_map *init_map(int map_size);

int insert_into_map(hash_map *map, char *ip, void *data);

void *find_in_map(hash_map *map, char *ip);

void delete_map(hash_map *map);

#endif