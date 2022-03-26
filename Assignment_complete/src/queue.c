#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
    /* TODO: put a new process to queue [q] */	
    if (q->size == MAX_QUEUE_SIZE) return;
    q->proc[q->size++] = proc;
}

struct pcb_t * dequeue(struct queue_t * q) {
    /* TODO: return a pcb whose prioprity is the highest
    * in the queue [q] and remember to remove it from q
    * */

    struct pcb_t *proc = NULL;
    if(empty(q)) return NULL;	//Nothing to pop
    int max = 0;
    uint16_t priority_max = q->proc[0]->priority;
    for(int i = 0; i < q->size; i++){
    if(q->proc[i]->priority > priority_max){
      priority_max = q->proc[i]->priority;
      max = i;
    }
    }
    proc = q->proc[max];
    for(int i = max; i < q->size - 1; i++){
    q->proc[i] = q->proc[i+1];
    }
    q->size--;
    return proc;
}

