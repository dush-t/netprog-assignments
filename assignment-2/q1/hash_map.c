#include "hash_map.h"

/*
  Hashes a string to a number
*/
int djb2Hash(char *str, int map_size)
{
  unsigned long hash = 5381;
  unsigned int len = strlen(str);
  int c;

  for (unsigned int i = 0; i < len; i++)
  {
    c = str[i];
    if (isalnum(c))
      hash = (((hash << 5) % map_size + hash) % map_size + c) % map_size; /* hash * 33 + c */
  }
  return hash % map_size;
}

hash_map *init_map(int map_size)
{
  hash_map *map = (hash_map *)calloc(1, sizeof(hash_map));
  if (map == NULL)
  {
    perror("init_map()");
    return NULL;
  }
  map->map_size = map_size;
  map->buckets = (map_bucket **)calloc(map_size, sizeof(map_bucket *));
  if (map->buckets == NULL)
  {
    perror("init_map()");
    return NULL;
  }

  for (int i = 0; i < map_size; i++)
  {
    map->buckets[i] = (map_bucket *)calloc(1, sizeof(map_bucket));
    if (map->buckets[i] == NULL)
    {
      perror("init_map()");
      return NULL;
    }
    (map->buckets[i])->head = NULL;
    (map->buckets[i])->capacity = 0;
  }

  return map;
}

/*
  Inserts data on the front of the bucket.
*/
int insert_into_bucket(map_bucket *bucket, char *ip, void *data)
{
  map_node *node = (map_node *)calloc(1, sizeof(map_node));
  if (node == NULL)
  {
    perror("insert_into_bucket()");
    return -1;
  }
  node->ip = ip;
  node->data = data;
  node->next = bucket->head;
  bucket->head = node;
  bucket->capacity += 1;
  return 0;
}

/*
  Insert data into the map.
  Return:
    0: success
    -1: error
    -2: duplicate found
*/
int insert_into_map(hash_map *map, char *ip, void *data)
{
  /* return -1 if already exists */
  if (find_in_map(map, ip) != NULL)
    return -2;

  int bucket_idx = djb2Hash(ip, map->map_size);
  return insert_into_bucket(map->buckets[bucket_idx], ip, data);
}

/*
  Find an element in a particular bucket.
*/
void *find_in_bucket(map_bucket *bucket, char *to_find)
{
  if (bucket->capacity == 0)
    return NULL;

  map_node *temp = bucket->head;
  void *data = NULL;
  while (temp != NULL)
  {
    if (strcmp(to_find, temp->ip) == 0)
    {
      data = temp->data;
      break;
    }
    temp = temp->next;
  }
  return data;
}

/*
  Find an element in the map.
*/
void *find_in_map(hash_map *map, char *to_find)
{
  int bucket_idx = djb2Hash(to_find, map->map_size);
  return find_in_bucket(map->buckets[bucket_idx], to_find);
}

/*
  Deallocate a map node.
*/
void deallocate_map_node(map_node *node)
{
  if (node->next != NULL)
  {
    deallocate_map_node(node->next);
    node->next = NULL;
  }
  free(node->ip);
  free(node);
  node = NULL;
}

/*
  Deallocate a particular bucket.
*/
void deallocate_map_bucket(map_bucket *bucket)
{
  if (bucket->capacity != 0)
  {
    deallocate_map_node(bucket->head);
    bucket->head = NULL;
  }
  free(bucket);
  bucket = NULL;
}

/*
  Deallocate the map
*/
void delete_map(hash_map *map)
{
  for (int i = 0; i < map->map_size; i++)
  {
    deallocate_map_bucket(map->buckets[i]);
    map->buckets[i] = NULL;
  }
  free(map->buckets);
  free(map);
  map = NULL;
}