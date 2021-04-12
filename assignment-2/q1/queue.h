#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdlib.h>

typedef struct queue_elem queue_elem;

struct queue_elem
{
  queue_elem *prev;
  void *data;
};

typedef struct
{
  queue_elem *front;
  int count;
} queue;

queue *initQueue();

void *qFront(queue *q);

void qPop(queue *q);

void qPush(queue *q, void *data);

void deleteQueue(queue *q);

#endif