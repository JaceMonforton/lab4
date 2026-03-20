#include "queue.h"
#include <stdlib.h>

/* initialize queue */
void queue_init(queue_t *q) {
    q->head = NULL;
    q->tail = NULL;
}

/* return 1 if empty, 0 otherwise */
int queue_empty(queue_t *q) {
    return q->head == NULL;
}

/* push to tail */
void queue_push(queue_t *q, process_t *p) {
    qnode_t *n = (qnode_t *)malloc(sizeof(qnode_t));
    n->p = p;
    n->next = NULL;

    if (q->tail) {
        q->tail->next = n;
    } else {
        q->head = n;
    }
    q->tail = n;
}

/* pop from head */
process_t *queue_pop(queue_t *q) {
    if (!q->head) return NULL;

    qnode_t *n = q->head;
    process_t *p = n->p;

    q->head = n->next;
    if (!q->head) {
        q->tail = NULL;
    }

    free(n);
    return p;
}

/* peek head */
process_t *queue_peek(queue_t *q) {
    return q->head ? q->head->p : NULL;
}

/* clear queue (does NOT free process_t) */
void queue_clear(queue_t *q) {
    while (!queue_empty(q)) {
        queue_pop(q);
    }
}
