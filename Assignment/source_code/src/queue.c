#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
	/* TODO: put a new process to queue [q] */
	if(q->size >= MAX_QUEUE_SIZE)
		return;
	q->proc[q->size] = proc;
	q->size++;
}

struct pcb_t * dequeue(struct queue_t * q) {
	/* TODO: return a pcb whose prioprity is the highest
	 * in the queue [q] and remember to remove it from q
	 * */
	if(q->size == 0)
		return NULL;
	struct pcb_t * item = NULL;
	uint32_t maxPriority = q->proc[0]->priority;
	int idxOfMaxPriotity = 0;
	for(int i = 0 ; i < q->size ; i++)
	{
		if(maxPriority < q->proc[i]->priority)
		{
			maxPriority = q->proc[i]->priority;
			idxOfMaxPriotity = i;
		}
	}
	item = q->proc[idxOfMaxPriotity];
	for(int i = idxOfMaxPriotity + 1; i < q->size ; i++)
	{
		q->proc[i - 1] = q->proc[i];
	}
	q->size--;
	return item;
}

