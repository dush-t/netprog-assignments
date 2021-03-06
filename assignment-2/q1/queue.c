#include "queue.h"

queue *initQueue()
{
  queue *q = (queue *)calloc(1, sizeof(queue));
  if (q == NULL)
  {
    perror("calloc");
    return NULL;
  }
  q->count = 0;
  q->front = NULL;
  return q;
}

void *qFront(queue *q)
{
  if (q == NULL)
    return NULL;
  if (q->count == 0)
    return NULL;
  return q->front->data;
}

void qPop(queue *q)
{
  if (q == NULL)
    return;
  if (q->count == 0)
    return;
  queue_elem *prev = q->front->prev;
  free(q->front);
  q->front = prev;
  q->count -= 1;
}

int qPush(queue *q, void *data)
{
  if (q == NULL)
    return -1;
  queue_elem *elem = (queue_elem *)calloc(1, sizeof(queue_elem));
  if (elem == NULL)
  {
    perror("calloc");
    return -1;
  }
  elem->data = data;
  elem->prev = q->front;

  q->front = elem;
  q->count += 1;
  return 0;
}

void deleteQueue(queue *q)
{
  if (q == NULL)
    return;
  while (q->count > 0)
    qPop(q);
  free(q);
}
